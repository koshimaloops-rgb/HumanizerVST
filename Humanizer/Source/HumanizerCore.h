#pragma once
// =============================================================
// HumanizerCore.h
//
// JUCE-freier Kern des Humanizers. Kennt nur Noten-Events mit
// Sample-Position und laeuft deshalb auch ohne DAW in Tests.
//
// Ablauf pro Audio-Block:
//   1. Note-Ons innerhalb von kGroupWindowMs werden zu einer Gruppe
//   2. Fuer Gruppen >= 2 Noten wird aus den gelernten Daten ein
//      Strum-Profil (Timing) und ein Velocity-Profil gewaehlt, passend
//      zur Richtung (aufwaerts/abwaerts) und zur Notenanzahl
//   3. Jede Note bekommt einen Verzoegerungs-Offset (nie negativ,
//      die frueheste Note bleibt an ihrer Originalposition)
//   4. Note-Offs werden um denselben Offset verschoben wie ihr Note-On,
//      die Notenlaenge bleibt also erhalten
//   5. Ein Scheduler mit fester Groesse gibt die Events im richtigen
//      Block mit der richtigen Sample-Position aus
//
// Keine Speicherallokation im process()-Aufruf.
// =============================================================

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

#include "strum_patterns.h"
#include "velocity_patterns.h"

namespace hz
{
// -------------------------------------------------------------
// Konstanten (hier drehen, wenn etwas anders klingen soll)
// -------------------------------------------------------------

// Die gelernten Strum-Positionen sind Bruchteile eines Beats.
// Wir rechnen sie mit diesem Tempo in Millisekunden um (feste Zeit,
// nicht abhaengig vom DAW-Tempo). 130 = Standardwert des alten Codes.
// ANNAHME: die Patterns wurden bei ~130 BPM aufgenommen.
constexpr double kLearnedReferenceBpm = 130.0;

// Noten, deren Note-On weniger weit auseinanderliegt, gelten als Akkord
// (Standardwert, im Plugin per Regler "Window" einstellbar).
constexpr double kGroupWindowMs = 25.0;

// Akzent: Velocity-Aenderung in Stufen bei Regler = 1 und Gewicht 1
// (Taktanfang +1.0, andere Zaehlzeiten +0.3..0.55, Achtel-Offbeat -0.15, Rest -0.35)
constexpr float kAccentVelocityRange = 10.0f;

// Kontur: Velocity folgt der Melodie-Richtung (steigend lauter, fallend leiser)
constexpr float kContourPerSemitone = 0.8f;
constexpr float kContourMaxVelocity = 6.0f;

// Zeitstruktur, die ein Mensch von selbst mitbringt (keine gelernten Daten, Regler "Timing"):
constexpr float kVelocityLagMs = 8.0f;  // leise Noten kommen bis zu so viel spaeter als laute (Klaviermechanik)
constexpr float kMelodyLeadMs = 6.0f;   // Begleitnoten eines Akkords hinken der obersten Note bis zu so viel hinterher

// Regler "Groove": Offbeat-Achtel bis zu diesen Anteil einer Achtelnote spaeter (bei 100 %)
constexpr float kGrooveFraction = 0.16f;

// Regler "Length": Notenende bis zu so viel spaeter (Legato-Ueberlappung)
constexpr float kMaxLegatoMs = 60.0f;

// "Timing humanize" (alles skaliert mit dem Regler 0..1):
constexpr float kMaxTimingJitterMs = 12.0f; // ganze Gruppe/Einzelnote wird bis zu so viel spaeter
constexpr float kMicroJitterMs = 1.0f;      // zusaetzlich pro Akkordnote

// Hat der Akkord keine Richtung (alle Noten exakt gleichzeitig),
// wird "aufwaerts" (Downstrum) mit dieser Wahrscheinlichkeit gewaehlt.
constexpr float kAscendingProbability = 0.70f;

// Einzelnoten: Velocity-Streuung (Gauss-Sigma) bei Regler = 1
constexpr float kSingleVelSigma = 2.0f;

constexpr int kMaxGroup = 16;
constexpr int kMaxIn = 2048;
constexpr int kMaxPending = 4096;
constexpr int kMaxOut = kMaxPending + 16 * 128;

// -------------------------------------------------------------
// Datentypen
// -------------------------------------------------------------

struct NoteEvent
{
    int pos = 0; // Sample-Position im Block
    bool on = false;
    uint8_t ch = 0; // 0..15
    uint8_t note = 0;
    uint8_t vel = 0;
};

// Fuer die Anzeige: jedes eingehende Note-On (vor dem Humanizen)
struct NoteInfo
{
    uint8_t note = 0;
    uint8_t vel = 0;
    int64_t absSample = 0;
};

// Fuer die Anzeige: eine humanisierte Gruppe, nach Tonhoehe sortiert
struct GroupReport
{
    int count = 0;
    bool ascending = true;
    std::array<uint8_t, kMaxGroup> note{};
    std::array<uint8_t, kMaxGroup> velIn{};
    std::array<uint8_t, kMaxGroup> velOut{};
    std::array<float, kMaxGroup> offsetMs{};
};

// Infos der DAW zum aktuellen Block (alles optional)
struct BlockInfo
{
    double bpm = 120.0;
    bool ppqValid = false;   // false -> kein Akzent
    double ppqStart = 0.0;   // Position des Blockanfangs in Viertelnoten
    double barStartPpq = 0.0; // Position des letzten Taktanfangs in Viertelnoten
    int tsNum = 4;
    int tsDen = 4;
};

// Richtung des Strums / der Velocity-Profile
enum DirectionMode
{
    kDirAuto = 0,      // aus der Eingabe erkennen, sonst Zufall (70 % aufwaerts)
    kDirLowToHigh = 1, // immer tief -> hoch (Downstrum)
    kDirHighToLow = 2, // immer hoch -> tief (Upstrum)
    kDirAlternate = 3  // Downstrum, Upstrum, Downstrum ...
};

// -------------------------------------------------------------
// Kleine Helfer: Zufall, korrelierter Zufall, Lock-free-Ring
// -------------------------------------------------------------

class Rng
{
public:
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) { reseed(seed); }

    void reseed(uint64_t seed) { s_ = seed != 0 ? seed : 1; }

    uint64_t next()
    {
        s_ ^= s_ >> 12;
        s_ ^= s_ << 25;
        s_ ^= s_ >> 27;
        return s_ * 2685821657736338717ull;
    }

    float uniform() { return static_cast<float>(next() >> 40) * (1.0f / 16777216.0f); } // [0,1)

    float gauss() // ~N(0,1)
    {
        float a = 0.0f;
        for (int i = 0; i < 12; ++i)
            a += uniform();
        return a - 6.0f;
    }

    int index(int n) { return std::min(n - 1, static_cast<int>(uniform() * static_cast<float>(n))); }

private:
    uint64_t s_ = 1;
};

// AR(1): x' = phi * x + sigma * noise. Aufeinanderfolgende Werte
// haengen zusammen, wie bei einem Menschen der "in einer Stimmung" spielt.
struct Ar1
{
    float x = 0.0f;
    float step(Rng& r, float phi, float sigma)
    {
        x = phi * x + sigma * r.gauss();
        return x;
    }
};

// Single-Producer / Single-Consumer Ring (Audio-Thread -> Message-Thread)
template <class T, uint32_t N>
class SpscRing
{
    static_assert((N & (N - 1)) == 0, "N muss eine Zweierpotenz sein");

public:
    bool push(const T& v)
    {
        const uint32_t w = w_.load(std::memory_order_relaxed);
        const uint32_t r = r_.load(std::memory_order_acquire);
        if (w - r >= N)
            return false;
        buf_[w & (N - 1)] = v;
        w_.store(w + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& v)
    {
        const uint32_t r = r_.load(std::memory_order_relaxed);
        if (r == w_.load(std::memory_order_acquire))
            return false;
        v = buf_[r & (N - 1)];
        r_.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<T, N> buf_{};
    std::atomic<uint32_t> w_{0};
    std::atomic<uint32_t> r_{0};
};

// -------------------------------------------------------------
// Zugriff auf die gelernten Tabellen
// -------------------------------------------------------------

inline const std::array<float, 5>& profileValues(const LearnedStrumPattern& p) { return p.positions; }
inline const std::array<float, 5>& profileValues(const LearnedVelocityProfile& p) { return p.offsets; }

// Strum: die Richtung steht in den Zeitpositionen selbst (tiefste Note vs.
// hoechste Note). Das gespeicherte Flag widerspricht dem bei einem Pattern
// (4 Noten, "ascending", aber hoechste Note zuerst), deshalb Positionen
// bevorzugen und das Flag nur bei Gleichstand nehmen.
inline bool profileAscending(const LearnedStrumPattern& p)
{
    const float first = p.positions[0];
    const float last = p.positions[p.noteCount - 1];
    if (first < last)
        return true;
    if (first > last)
        return false;
    return p.ascending;
}

// Velocity: Richtung ist aus den Werten nicht ablesbar -> Flag
inline bool profileAscending(const LearnedVelocityProfile& p) { return p.ascending; }

// Holt ein Profil (optional gespiegelt) und bringt es auf n Noten.
// Mehr als 5 Noten: lineare Interpolation entlang der Tonhoehe.
inline void fetchProfileValues(const std::array<float, 5>& v, int m, bool mirror, int n, bool scaleSpread, float* out)
{
    float src[5] = {0, 0, 0, 0, 0};
    for (int k = 0; k < m; ++k)
        src[k] = mirror ? v[static_cast<size_t>(m - 1 - k)] : v[static_cast<size_t>(k)];

    if (n == m)
    {
        for (int k = 0; k < n; ++k)
            out[k] = src[k];
        return;
    }

    // Beim Strum bleibt der Abstand pro Note ungefaehr gleich -> Breite waechst mit n
    const float scale = scaleSpread ? (static_cast<float>(n) - 1.0f) / (static_cast<float>(m) - 1.0f) : 1.0f;
    for (int j = 0; j < n; ++j)
    {
        const float u = static_cast<float>(j) * (static_cast<float>(m) - 1.0f) / (static_cast<float>(n) - 1.0f);
        const int i0 = std::min(static_cast<int>(u), m - 1);
        const int i1 = std::min(i0 + 1, m - 1);
        const float f = u - static_cast<float>(i0);
        out[j] = (src[i0] * (1.0f - f) + src[i1] * f) * scale;
    }
}

// Waehlt aus der Tabelle ein passendes echtes Profil und mischt es leicht
// (bis 50 %) mit einem zweiten. So wiederholt sich nichts exakt, ohne dass
// die gelernte Form verwaschen wird.
//  - bevorzugt Profile mit gleicher Notenanzahl und gleicher Richtung
//  - gibt es davon weniger als 3, kommen Profile der Gegenrichtung dazu:
//      mirrorOpposite = true  -> gespiegelt (Reihenfolge umdrehen). Richtig fuer
//                                das Strum-TIMING, das ist zeitlich geordnet.
//      mirrorOpposite = false -> unveraendert. Richtig fuer VELOCITY: die Profile
//                                sind nach Tonhoehe geordnet und die hoechste Note
//                                ist in beiden Richtungen die lauteste (siehe Daten).
//  - n > 5: das 5-Noten-Profil wird gestreckt
template <class T>
inline bool buildProfile(const T* data, int count, int n, bool ascending, bool scaleSpread, bool mirrorOpposite, Rng& rng, float* out)
{
    const int target = std::min(n, 5);

    int idx[256];
    bool mir[256];
    int nc = 0;

    for (int i = 0; i < count && nc < 256; ++i)
        if (data[i].noteCount == target && profileAscending(data[i]) == ascending)
        {
            idx[nc] = i;
            mir[nc] = false;
            ++nc;
        }

    if (nc < 3)
        for (int i = 0; i < count && nc < 256; ++i)
            if (data[i].noteCount == target && profileAscending(data[i]) != ascending)
            {
                idx[nc] = i;
                mir[nc] = mirrorOpposite;
                ++nc;
            }

    if (nc == 0)
        return false;

    const int a = rng.index(nc);
    const int b = rng.index(nc);
    const float w = rng.uniform() * 0.5f;

    float A[kMaxGroup], B[kMaxGroup];
    fetchProfileValues(profileValues(data[idx[a]]), data[idx[a]].noteCount, mir[a], n, scaleSpread, A);
    fetchProfileValues(profileValues(data[idx[b]]), data[idx[b]].noteCount, mir[b], n, scaleSpread, B);

    for (int k = 0; k < n; ++k)
        out[k] = A[k] * (1.0f - w) + B[k] * w;

    return true;
}

// -------------------------------------------------------------
// Der Kern
// -------------------------------------------------------------

class HumanizerCore
{
public:
    explicit HumanizerCore(uint64_t seed = 0x1234ABCDull)
        : rng_(seed),
          pending_(kMaxPending),
          due_(kMaxPending),
          out_(kMaxOut),
          offs_(kMaxIn),
          newVel_(kMaxIn),
          used_(kMaxIn)
    {
        clearTables();
        reset();
    }

    // ---- Steuerung (aus jedem Thread) ------------------------

    void setVelocityAmount(float v) { velAmount_.store(std::max(0.0f, std::min(2.0f, v))); } // 0..2
    void setTimingAmount(float v) { timAmount_.store(std::max(0.0f, std::min(1.0f, v))); }   // 0..1
    void setStrumIntensity(float v) { strumIntensity_.store(std::max(0.0f, std::min(4.0f, v))); } // 0..4, 1 = gelernte Werte

    void setAccentAmount(float v) { accentAmount_.store(std::max(0.0f, std::min(1.0f, v))); }  // 0..1
    void setContourAmount(float v) { contourAmount_.store(std::max(0.0f, std::min(1.0f, v))); } // 0..1
    void setGroupWindowMs(float ms) { windowMs_.store(std::max(1.0f, std::min(100.0f, ms))); }
    void setDirectionMode(int m) { dirMode_.store(std::max(0, std::min(3, m))); }
    void setBypass(bool b) { bypass_.store(b); }
    void setGrooveAmount(float v) { grooveAmount_.store(std::max(0.0f, std::min(1.0f, v))); } // 0..1
    void setSoftness(float v) { softness_.store(std::max(0.0f, std::min(1.0f, v))); }         // 0..1 (leise, komprimiert)
    void setBassCare(float v) { bassCare_.store(std::max(0.0f, std::min(1.0f, v))); }         // 0..1 (tiefe Noten leiser)
    void setLengthAmount(float v) { lengthAmount_.store(std::max(0.0f, std::min(1.0f, v))); } // 0..1

    float getVelocityAmount() const { return velAmount_.load(); }
    float getTimingAmount() const { return timAmount_.load(); }
    float getStrumIntensity() const { return strumIntensity_.load(); }

    void setDriftEnabled(bool b) { driftEnabled_ = b; }
    void seed(uint64_t s) { rng_.reseed(s); }

    // Sampleratenwechsel / Transport-Reset (nicht waehrend process() aufrufen)
    void prepare(double sampleRate)
    {
        sr_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        abs_ = 0;
        requestPanic();
    }

    // Verwirft alles Ausstehende; klingende Noten bekommen im naechsten
    // Block ein Note-Off (verhindert haengende Noten).
    void requestPanic() { panic_.store(true); }

    // ---- Audio-Thread ----------------------------------------

    // `in` muss nach Sample-Position sortiert sein (wie MidiBuffer).
    // Rueckgabe: Anzahl Output-Events, abrufbar mit output().
    int process(const NoteEvent* in, int nIn, int numSamples, const BlockInfo& info = BlockInfo())
    {
        info_ = info;
        nOut_ = 0;
        nIn = std::min(nIn, kMaxIn);
        const int64_t blockStart = abs_;
        const int64_t blockEnd = abs_ + numSamples;

        if (panic_.exchange(false))
            doPanic();

        planGroups(in, nIn);

        for (int i = 0; i < nIn; ++i)
        {
            NoteEvent o = in[i];
            o.ch = static_cast<uint8_t>(o.ch & 15);
            o.note = static_cast<uint8_t>(o.note & 127);
            const int64_t at = blockStart + o.pos;

            if (o.on)
            {
                NoteInfo info;
                info.note = o.note;
                info.vel = o.vel;
                info.absSample = at;
                infoRing_.push(info);

                shift_[o.ch][o.note] = offs_[static_cast<size_t>(i)];
                o.vel = newVel_[static_cast<size_t>(i)];
                schedule(at + offs_[static_cast<size_t>(i)], o);
            }
            else
            {
                const int s = shift_[o.ch][o.note];
                shift_[o.ch][o.note] = 0;

                // "Length": Notenende zufaellig (eher kurz) nach hinten, wie bei echtem Legato
                int ext = 0;
                const float lenAmt = lengthAmount_.load(std::memory_order_relaxed);
                if (lenAmt > 0.0f && !bypass_.load(std::memory_order_relaxed))
                {
                    const float r1 = rng_.uniform();
                    const float r2 = rng_.uniform();
                    ext = static_cast<int>(std::llround(static_cast<double>(r1 * r2 * kMaxLegatoMs * lenAmt) * sr_ * 0.001));
                }
                schedule(at + s + ext, o);
            }
        }

        emitDue(blockStart, blockEnd, numSamples);
        abs_ = blockEnd;
        return nOut_;
    }

    const NoteEvent* output() const { return out_.data(); }

    // ---- Message-Thread (Anzeige) ----------------------------

    bool popNoteInfo(NoteInfo& v) { return infoRing_.pop(v); }
    bool popGroupReport(GroupReport& v) { return reportRing_.pop(v); }

private:
    struct Pending
    {
        int64_t target = 0;
        uint64_t seq = 0;
        NoteEvent ev;
    };

    void clearTables()
    {
        for (int c = 0; c < 16; ++c)
            for (int n = 0; n < 128; ++n)
            {
                shift_[c][n] = 0;
                lastTarget_[c][n] = 0;
                lastOnTarget_[c][n] = 0;
                lastWasOff_[c][n] = false;
                sounding_[c][n] = false;
            }
    }

    void doPanic()
    {
        for (int c = 0; c < 16; ++c)
            for (int n = 0; n < 128; ++n)
                if (sounding_[c][n])
                {
                    NoteEvent o;
                    o.pos = 0;
                    o.on = false;
                    o.ch = static_cast<uint8_t>(c);
                    o.note = static_cast<uint8_t>(n);
                    o.vel = 0;
                    out_[static_cast<size_t>(nOut_++)] = o;
                }

        nPending_ = 0;
        clearTables();
        widthAr_.x = 0.0f;
        driftAr_.x = 0.0f;
        feelAr_.x = 0.0f;
        altNext_ = true;
        prevRep_ = -1;
    }

    // Metrische Position eines Events im aktuellen Block.
    //   accent: +1 Taktanfang, +0.3..0.55 andere Zaehlzeiten, -0.15 Achtel-Offbeat, -0.35 Rest
    //   groove: 1 Achtel-Offbeat, 0.55 Sechzehntel-Offbeat, sonst 0
    // Alles 0, wenn die DAW keine Taktposition liefert.
    struct Metric
    {
        float accent = 0.0f;
        float groove = 0.0f;
    };

    Metric metricAt(int pos) const
    {
        Metric m;
        if (!info_.ppqValid || info_.bpm <= 0.0)
            return m;

        const double ppq = info_.ppqStart + static_cast<double>(pos) * info_.bpm / (60.0 * sr_);
        const double beatLen = 4.0 / static_cast<double>(std::max(1, info_.tsDen)); // in Viertelnoten
        const int beatsPerBar = std::max(1, info_.tsNum);

        double b = (ppq - info_.barStartPpq) / beatLen;
        b = std::fmod(b, static_cast<double>(beatsPerBar));
        if (b < 0.0)
            b += beatsPerBar;

        int bi = static_cast<int>(std::floor(b));
        double fr = b - std::floor(b);
        const double eps = 0.04;

        if (fr > 1.0 - eps) // knapp vor der naechsten Zaehlzeit
        {
            fr = 0.0;
            bi = (bi + 1) % beatsPerBar;
        }

        if (fr < eps) // auf der Zaehlzeit
        {
            if (bi == 0)
                m.accent = 1.0f;
            else if (beatsPerBar % 2 == 0 && bi == beatsPerBar / 2)
                m.accent = 0.55f;
            else
                m.accent = 0.3f;
            return m;
        }

        if (std::fabs(fr - 0.5) < eps) // Achtel-Offbeat
        {
            m.accent = -0.15f;
            m.groove = 1.0f;
            return m;
        }

        m.accent = -0.35f;
        if (std::fabs(fr - 0.25) < eps || std::fabs(fr - 0.75) < eps) // Sechzehntel-Offbeat
            m.groove = 0.55f;
        return m;
    }

    // ---- Pass 1: Gruppen finden und Offsets/Velocities planen ----

    void planGroups(const NoteEvent* in, int nIn)
    {
        const int64_t window = static_cast<int64_t>(std::llround(static_cast<double>(windowMs_.load()) * 0.001 * sr_));

        for (int i = 0; i < nIn; ++i)
        {
            offs_[static_cast<size_t>(i)] = 0;
            newVel_[static_cast<size_t>(i)] = in[i].vel;
            used_[static_cast<size_t>(i)] = 0;
        }

        // Bypass: alles unveraendert durchreichen (Note-Offs bereits
        // verschobener Noten werden trotzdem korrekt nachgezogen)
        if (bypass_.load(std::memory_order_relaxed))
            return;

        for (int i = 0; i < nIn; ++i)
        {
            if (!in[i].on || used_[static_cast<size_t>(i)])
                continue;

            int mem[kMaxGroup];
            int n = 0;
            mem[n++] = i;
            used_[static_cast<size_t>(i)] = 1;

            for (int j = i + 1; j < nIn && n < kMaxGroup; ++j)
            {
                if (in[j].pos - in[i].pos > window)
                    break; // Events sind zeitlich sortiert
                if (!in[j].on || used_[static_cast<size_t>(j)])
                    continue;
                mem[n++] = j;
                used_[static_cast<size_t>(j)] = 1;
            }

            planGroup(in, mem, n);
        }
    }

    void planGroup(const NoteEvent* in, int* mem, int n)
    {
        // nach Tonhoehe sortieren (Profile sind nach Tonhoehe indiziert)
        std::sort(mem, mem + n, [&](int a, int b)
                  { return in[a].note != in[b].note ? in[a].note < in[b].note : a < b; });

        const float strum = strumIntensity_.load(std::memory_order_relaxed);
        const float velAmt = velAmount_.load(std::memory_order_relaxed);
        const float timAmt = timAmount_.load(std::memory_order_relaxed);
        const double msToSamples = sr_ * 0.001;

        const float accAmt = accentAmount_.load(std::memory_order_relaxed);
        const float conAmt = contourAmount_.load(std::memory_order_relaxed);
        const int dirMode = dirMode_.load(std::memory_order_relaxed);
        const float bassAmt = bassCare_.load(std::memory_order_relaxed);
        const float softAmt = softness_.load(std::memory_order_relaxed);

        // "Feel": die Verspaetung schwankt langsam (AR(1)) statt bei jeder Gruppe neu zu wuerfeln.
        // Aufeinanderfolgende Akkorde liegen dadurch aehnlich spaet - wie ein Spieler, der eine
        // Weile leicht hinter dem Beat bleibt. Weisses Rauschen klingt dagegen sofort nach Randomizer.
        const float feelAr = std::max(-1.0f, std::min(1.0f, feelAr_.step(rng_, 0.8f, 0.35f)));
        const float feelMs = timAmt * kMaxTimingJitterMs * (0.5f + 0.5f * feelAr);

        // Metrische Position der Gruppe (fruehestes Note-On): Akzent und Groove
        int firstPos = in[mem[0]].pos;
        for (int k = 1; k < n; ++k)
            firstPos = std::min(firstPos, in[mem[k]].pos);
        const Metric metric = metricAt(firstPos);
        const float accentVel = accAmt * kAccentVelocityRange * metric.accent;

        float grooveMs = 0.0f;
        if (metric.groove > 0.0f && info_.bpm > 0.0)
        {
            const float eighthMs = static_cast<float>(30000.0 / info_.bpm);
            grooveMs = grooveAmount_.load(std::memory_order_relaxed) * kGrooveFraction * metric.groove * eighthMs * (0.8f + 0.4f * rng_.uniform());
        }

        // Kontur: Melodierichtung gegenueber der letzten Gruppe (bei Akkorden: oberste Note)
        const int repNote = static_cast<int>(in[mem[n - 1]].note);
        float contourVel = 0.0f;
        if (prevRep_ >= 0)
            contourVel = conAmt * std::max(-kContourMaxVelocity,
                                           std::min(kContourMaxVelocity, static_cast<float>(repNote - prevRep_) * kContourPerSemitone));
        prevRep_ = repNote;

        float drift = 0.0f;
        if (driftEnabled_)
            drift = std::max(-6.0f, std::min(6.0f, driftAr_.step(rng_, 0.85f, 1.0f)));

        float strumPos[kMaxGroup];
        float velOff[kMaxGroup];
        for (int k = 0; k < kMaxGroup; ++k)
        {
            strumPos[k] = 0.0f;
            velOff[k] = 0.0f;
        }

        bool asc = true;
        float width = 1.0f;

        if (n >= 2)
        {
            // Richtung: vom Nutzer erzwungen, abwechselnd, oder aus dem
            // Zeitverhalten der Eingabe erkannt (sonst Zufall mit Praeferenz)
            if (dirMode == kDirLowToHigh)
                asc = true;
            else if (dirMode == kDirHighToLow)
                asc = false;
            else if (dirMode == kDirAlternate)
            {
                asc = altNext_;
                altNext_ = !altNext_;
            }
            else
            {
                const NoteEvent& lo = in[mem[0]];
                const NoteEvent& hi = in[mem[n - 1]];
                if (lo.pos < hi.pos)
                    asc = true;
                else if (lo.pos > hi.pos)
                    asc = false;
                else
                    asc = rng_.uniform() < kAscendingProbability;
            }

            buildProfile(learnedStrumPatterns, learnedStrumPatternCount, n, asc, true, true, rng_, strumPos);

            if (buildProfile(learnedVelocityProfiles, learnedVelocityProfileCount, n, asc, false, false, rng_, velOff))
            {
                // Mittelwert wieder auf 0 (Interpolation bei n > 5 verschiebt ihn leicht)
                float mean = 0.0f;
                for (int k = 0; k < n; ++k)
                    mean += velOff[k];
                mean /= static_cast<float>(n);
                for (int k = 0; k < n; ++k)
                    velOff[k] -= mean;
            }

            // Strum-Breite schwankt langsam von Akkord zu Akkord
            width = std::max(0.6f, std::min(1.5f, 1.0f + widthAr_.step(rng_, 0.7f, 0.10f)));
        }

        GroupReport rep;
        rep.count = n;
        rep.ascending = asc;

        const float msPerBeat = static_cast<float>(60000.0 / kLearnedReferenceBpm);

        for (int k = 0; k < n; ++k)
        {
            const int i = mem[k];

            float ms = feelMs + grooveMs;
            float dv = drift;

            if (n >= 2)
            {
                ms += strumPos[k] * msPerBeat * strum * width;
                ms += rng_.uniform() * kMicroJitterMs * timAmt;
                if (k < n - 1) // alle ausser der obersten Note: Melodie fuehrt, Begleitung hinkt leicht hinterher
                    ms += rng_.uniform() * kMelodyLeadMs * timAmt;
                dv += velOff[k];
            }
            else
            {
                dv += rng_.gauss() * kSingleVelSigma;
            }

            float raw = static_cast<float>(in[i].vel) + velAmt * dv + accentVel + contourVel;
            // Soft: zieht Velocities nach unten und staucht sie (24 + 0.5*v), wie eine leise gespielte Klavier-Melodie
            raw += softAmt * ((24.0f + 0.5f * raw) - raw);
            // Bass-Pflege: Noten unter E3 werden mit der Tiefe leiser (bis -35 % ab C2), damit der Bass nichts erschlaegt
            {
                const float ramp = std::max(0.0f, std::min(1.0f, (52.0f - static_cast<float>(in[i].note)) / 16.0f));
                raw *= 1.0f - 0.35f * bassAmt * ramp;
            }
            const int v = static_cast<int>(std::lround(raw));
            const int vClamped = std::max(1, std::min(127, v));
            newVel_[static_cast<size_t>(i)] = static_cast<uint8_t>(vClamped);

            // Leise Noten kommen spaeter als laute (Tastenweg / Anschlagszeit)
            ms += timAmt * kVelocityLagMs * (1.0f - static_cast<float>(vClamped) / 127.0f);

            offs_[static_cast<size_t>(i)] = static_cast<int>(std::llround(static_cast<double>(ms) * msToSamples));

            rep.note[static_cast<size_t>(k)] = in[i].note;
            rep.velIn[static_cast<size_t>(k)] = in[i].vel;
            rep.velOut[static_cast<size_t>(k)] = newVel_[static_cast<size_t>(i)];
            rep.offsetMs[static_cast<size_t>(k)] = ms;
        }

        if (n >= 2)
            reportRing_.push(rep);
    }

    // ---- Scheduler ----

    // Setzt den noch ausstehenden Note-Off dieser Tonhoehe frueher (nur nach vorne).
    bool pullOff(int c, int n, int64_t newTarget)
    {
        bool found = false;
        for (int i = 0; i < nPending_; ++i)
        {
            Pending& p = pending_[static_cast<size_t>(i)];
            if (!p.ev.on && p.ev.ch == c && p.ev.note == n && p.target > newTarget)
            {
                p.target = newTarget;
                found = true;
            }
        }
        return found;
    }

    void schedule(int64_t target, const NoteEvent& e)
    {
        // Pro Tonhoehe nie rueckwaerts in der Zeit: ein Note-Off darf nicht vor seinem
        // Note-On landen und ein Note-On nicht vor dem Note-Off der vorigen gleichen Note.
        const int c = e.ch;
        const int n = e.note;
        int64_t last = lastTarget_[c][n];

        if (target < last)
        {
            // Der alte Note-Off wurde (Strum-Shift oder "Length") nach hinten geschoben und
            // wuerde dieses Note-On verzoegern. Dann lieber den alten Note-Off vorziehen -
            // aber nie vor sein eigenes Note-On.
            if (e.on && lastWasOff_[c][n])
            {
                const int64_t newOff = std::max(target - 1, lastOnTarget_[c][n]);
                if (pullOff(c, n, newOff))
                {
                    lastTarget_[c][n] = newOff;
                    last = newOff;
                }
            }
            if (target < last)
                target = last;
        }

        lastTarget_[c][n] = target;
        lastWasOff_[c][n] = !e.on;
        if (e.on)
            lastOnTarget_[c][n] = target;

        if (nPending_ >= kMaxPending)
            return; // Notfall, praktisch unerreichbar (4096 Events in ~100 ms)

        Pending p;
        p.target = target;
        p.seq = seq_++;
        p.ev = e;
        pending_[static_cast<size_t>(nPending_++)] = p;
    }

    void emitDue(int64_t blockStart, int64_t blockEnd, int numSamples)
    {
        int nDue = 0;
        int keep = 0;
        for (int i = 0; i < nPending_; ++i)
        {
            if (pending_[static_cast<size_t>(i)].target < blockEnd)
                due_[static_cast<size_t>(nDue++)] = pending_[static_cast<size_t>(i)];
            else
                pending_[static_cast<size_t>(keep++)] = pending_[static_cast<size_t>(i)];
        }
        nPending_ = keep;

        std::sort(due_.begin(), due_.begin() + nDue, [](const Pending& a, const Pending& b)
                  { return a.target != b.target ? a.target < b.target : a.seq < b.seq; });

        const int64_t lastPos = std::max(numSamples - 1, 0);
        for (int i = 0; i < nDue; ++i)
        {
            NoteEvent o = due_[static_cast<size_t>(i)].ev;
            const int64_t rel = due_[static_cast<size_t>(i)].target - blockStart;
            o.pos = static_cast<int>(std::min<int64_t>(std::max<int64_t>(rel, 0), lastPos));
            sounding_[o.ch][o.note] = o.on;
            out_[static_cast<size_t>(nOut_++)] = o;
        }
    }

    // ---- Zustand ----

    double sr_ = 44100.0;
    int64_t abs_ = 0;

    std::atomic<float> velAmount_{0.0f};
    std::atomic<float> timAmount_{0.0f};
    std::atomic<float> strumIntensity_{1.0f};
    std::atomic<float> accentAmount_{0.0f};
    std::atomic<float> contourAmount_{0.0f};
    std::atomic<float> grooveAmount_{0.0f};
    std::atomic<float> lengthAmount_{0.0f};
    std::atomic<float> softness_{0.0f};
    std::atomic<float> bassCare_{0.0f};
    std::atomic<float> windowMs_{static_cast<float>(kGroupWindowMs)};
    std::atomic<int> dirMode_{0};
    std::atomic<bool> bypass_{false};
    std::atomic<bool> panic_{false};
    bool driftEnabled_ = true;

    BlockInfo info_;
    bool altNext_ = true; // naechster Alternate-Strum: aufwaerts (Downstrum)
    int prevRep_ = -1;    // Leitnote der vorigen Gruppe (fuer Kontur)

    Rng rng_;
    Ar1 widthAr_;
    Ar1 driftAr_;
    Ar1 feelAr_;

    std::vector<Pending> pending_;
    std::vector<Pending> due_;
    std::vector<NoteEvent> out_;
    std::vector<int> offs_;
    std::vector<uint8_t> newVel_;
    std::vector<uint8_t> used_;
    int nPending_ = 0;
    int nOut_ = 0;
    uint64_t seq_ = 0;

    int shift_[16][128];
    int64_t lastTarget_[16][128];
    int64_t lastOnTarget_[16][128];
    bool lastWasOff_[16][128];
    bool sounding_[16][128];

    SpscRing<NoteInfo, 256> infoRing_;
    SpscRing<GroupReport, 64> reportRing_;
};

} // namespace hz
