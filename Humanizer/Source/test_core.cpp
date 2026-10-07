// Tests fuer HumanizerCore (ohne JUCE).
// Bauen: g++ -std=c++14 -O1 -g -Wall -Wextra -fsanitize=address,undefined -I../Source test_core.cpp -o test_core
// Laufen: ./test_core
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <vector>

#include "HumanizerCore.h"

using namespace hz;

static int g_fail = 0;
static int g_checks = 0;
#define CHECK(cond)                                                               \
    do                                                                            \
    {                                                                             \
        ++g_checks;                                                               \
        if (!(cond))                                                              \
        {                                                                         \
            ++g_fail;                                                             \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                         \
    } while (0)

struct Out
{
    int64_t abs;
    bool on;
    int ch, note, vel;
};

using Block = std::vector<NoteEvent>;

static NoteEvent ev(int pos, bool on, int note, int vel = 90, int ch = 0)
{
    NoteEvent e;
    e.pos = pos;
    e.on = on;
    e.note = static_cast<uint8_t>(note);
    e.vel = static_cast<uint8_t>(vel);
    e.ch = static_cast<uint8_t>(ch);
    return e;
}

// laesst Bloecke durch den Kern laufen, danach noch `tail` leere Bloecke zum Ausleeren
static std::vector<Out> run(HumanizerCore& c, const std::vector<Block>& blocks, int blockSize, int tail = 200,
                            const BlockInfo* info = nullptr, double sr = 44100.0)
{
    std::vector<Out> res;
    int64_t base = 0;
    BlockInfo bi;
    if (info)
        bi = *info;
    const int total = static_cast<int>(blocks.size()) + tail;
    for (int b = 0; b < total; ++b)
    {
        Block empty;
        const Block& blk = b < static_cast<int>(blocks.size()) ? blocks[static_cast<size_t>(b)] : empty;
        const int n = c.process(blk.data(), static_cast<int>(blk.size()), blockSize, bi);
        bi.ppqStart += blockSize * bi.bpm / (60.0 * sr);
        int lastPos = 0;
        for (int i = 0; i < n; ++i)
        {
            const NoteEvent& o = c.output()[i];
            CHECK(o.pos >= 0 && o.pos < blockSize);
            CHECK(o.pos >= lastPos); // Ausgabe zeitlich sortiert
            lastPos = o.pos;
            res.push_back({base + o.pos, o.on, o.ch, o.note, o.vel});
        }
        base += blockSize;
    }
    return res;
}

static std::unique_ptr<HumanizerCore> make(double sr, float strum, float vel, float tim, uint64_t seed)
{
    std::unique_ptr<HumanizerCore> c(new HumanizerCore(seed));
    c->prepare(sr);
    c->setStrumIntensity(strum);
    c->setVelocityAmount(vel);
    c->setTimingAmount(tim);
    return c;
}

static void testChordBasics()
{
    std::printf("chord basics\n");
    double maxOffMs = 0;
    int withOffset = 0;
    for (uint64_t seed = 1; seed <= 300; ++seed)
    {
        auto c = make(44100.0, 1.0f, 0.0f, 0.0f, seed);
        std::vector<Block> blocks(1);
        blocks[0] = {ev(100, true, 60), ev(100, true, 64), ev(100, true, 67), ev(100, true, 72),
                     ev(400, false, 60), ev(400, false, 64), ev(400, false, 67), ev(400, false, 72)};
        auto o = run(*c, blocks, 512);

        std::map<int, int64_t> on, off;
        for (auto& e : o)
            (e.on ? on : off)[e.note] = e.abs;

        CHECK(on.size() == 4 && off.size() == 4);
        int64_t minOn = 1 << 30;
        for (auto& kv : on)
        {
            minOn = std::min(minOn, kv.second);
            CHECK(kv.second >= 100);
            CHECK(off[kv.first] - kv.second == 300); // Notenlaenge bleibt erhalten
            const double ms = (kv.second - 100) * 1000.0 / 44100.0;
            maxOffMs = std::max(maxOffMs, ms);
            if (ms > 3.0)
                ++withOffset;
        }
        CHECK(minOn == 100); // fruehste Note bleibt an der Originalposition
    }
    std::printf("  max strum offset: %.1f ms, notes with >3ms offset: %d\n", maxOffMs, withOffset);
    CHECK(maxOffMs <= 28.9 * 1.5 + 0.1); // gelernte Obergrenze * max. Breite
    CHECK(withOffset > 50);              // es passiert wirklich etwas
}

static void testNeutralSettings()
{
    std::printf("neutral settings\n");
    auto c = make(44100.0, 0.0f, 0.0f, 0.0f, 7);
    std::vector<Block> blocks(1);
    blocks[0] = {ev(10, true, 50, 80), ev(10, true, 57, 81), ev(10, true, 62, 82), ev(300, false, 50), ev(300, false, 57), ev(300, false, 62)};
    auto o = run(*c, blocks, 256 * 2);
    int n = 0;
    for (auto& e : o)
        if (e.on)
        {
            ++n;
            CHECK(e.abs == 10);
            CHECK(e.vel == 80 + (e.note - 50 == 0 ? 0 : (e.note == 57 ? 1 : 2)));
        }
        else
            CHECK(e.abs == 300);
    CHECK(n == 3);
}

static void testCrossBlock()
{
    std::printf("cross block\n");
    for (uint64_t seed = 1; seed <= 100; ++seed)
    {
        auto c = make(44100.0, 4.0f, 1.0f, 1.0f, seed);
        c->setDriftEnabled(false);
        std::vector<Block> blocks(1);
        blocks[0] = {ev(500, true, 60), ev(500, true, 64), ev(500, true, 67), ev(500, true, 71),
                     ev(505, false, 60), ev(505, false, 64), ev(505, false, 67), ev(505, false, 71)};
        auto o = run(*c, blocks, 512);
        std::map<int, int64_t> on, off;
        for (auto& e : o)
        {
            (e.on ? on : off)[e.note] = e.abs;
            CHECK(e.vel >= 1 && e.vel <= 127);
        }
        CHECK(on.size() == 4 && off.size() == 4);
        for (auto& kv : on)
        {
            CHECK(off[kv.first] - kv.second == 5);
            CHECK(kv.second >= 500);
        }
    }
}

static void testSampleRateIndependence()
{
    std::printf("samplerate independence\n");
    auto c1 = make(44100.0, 1.0f, 0.0f, 0.0f, 99);
    auto c2 = make(96000.0, 1.0f, 0.0f, 0.0f, 99);
    for (int rep = 0; rep < 50; ++rep)
    {
        std::vector<Block> b1(1), b2(1);
        b1[0] = {ev(0, true, 55), ev(0, true, 59), ev(0, true, 62)};
        b2[0] = b1[0];
        auto o1 = run(*c1, b1, 256, 20);
        auto o2 = run(*c2, b2, 256, 20);
        CHECK(o1.size() == o2.size());
        for (size_t i = 0; i < o1.size() && i < o2.size(); ++i)
            CHECK(std::fabs(o1[i].abs * 1000.0 / 44100.0 - o2[i].abs * 1000.0 / 96000.0) < 0.05);
    }
}

static void testDirectionAndMirroring()
{
    std::printf("direction + mirroring (3 notes descending has no native pattern)\n");
    for (uint64_t seed = 1; seed <= 100; ++seed)
    {
        auto c = make(44100.0, 1.0f, 0.0f, 0.0f, seed);
        // vorgespielter Abwaerts-Strum: hoechste Note zuerst
        std::vector<Block> blocks(1);
        blocks[0] = {ev(100, true, 72), ev(120, true, 64), ev(140, true, 55)};
        auto o = run(*c, blocks, 512);
        std::map<int, int64_t> on;
        for (auto& e : o)
            if (e.on)
                on[e.note] = e.abs;
        CHECK(on.size() == 3);
        CHECK(on[72] == 100);
        const int64_t dHi = on[72] - 100, dMid = on[64] - 120, dLo = on[55] - 140;
        CHECK(dHi >= 0 && dMid >= dHi - 1 && dLo >= dMid - 1);
        CHECK(on[72] < on[64] && on[64] < on[55]); // Reihenfolge bleibt hoch -> tief
    }
}

static void testVelocityMeanPreserved()
{
    std::printf("velocity: zero-sum\n");
    int checked = 0;
    for (uint64_t seed = 1; seed <= 300; ++seed)
    {
        auto c = make(44100.0, 1.0f, 1.0f, 0.0f, seed);
        c->setDriftEnabled(false);
        const int n = 2 + static_cast<int>(seed % 4); // 2..5
        std::vector<Block> blocks(1);
        int sumIn = 0;
        for (int k = 0; k < n; ++k)
        {
            blocks[0].push_back(ev(50, true, 48 + k * 4, 64));
            sumIn += 64;
        }
        auto o = run(*c, blocks, 512, 5);
        int sumOut = 0, cnt = 0;
        for (auto& e : o)
            if (e.on)
            {
                sumOut += e.vel;
                ++cnt;
            }
        CHECK(cnt == n);
        CHECK(std::abs(sumOut - sumIn) <= n); // nur Rundung
        ++checked;
    }
    CHECK(checked == 300);
}

static void testBigGroups()
{
    std::printf("big groups (7 notes resampled, 20 notes split)\n");
    for (int n : {7, 12, 20})
    {
        auto c = make(44100.0, 1.0f, 1.0f, 1.0f, 5);
        std::vector<Block> blocks(1);
        for (int k = 0; k < n; ++k)
            blocks[0].push_back(ev(0, true, 36 + k * 3, 80));
        for (int k = 0; k < n; ++k)
            blocks[0].push_back(ev(200, false, 36 + k * 3));
        auto o = run(*c, blocks, 512);
        int on = 0, off = 0;
        for (auto& e : o)
            (e.on ? on : off)++;
        CHECK(on == n && off == n);
    }
}

static void testPanic()
{
    std::printf("panic\n");
    auto c = make(44100.0, 4.0f, 0.0f, 1.0f, 3);
    std::vector<Block> blocks(1);
    blocks[0] = {ev(0, true, 60), ev(0, true, 64), ev(0, true, 67), ev(0, true, 72)};
    int ons = 0, offs = 0;
    int n = c->process(blocks[0].data(), 4, 64);
    for (int i = 0; i < n; ++i)
        (c->output()[i].on ? ons : offs)++;
    c->requestPanic();
    int lateOns = 0;
    for (int b = 0; b < 100; ++b)
    {
        n = c->process(nullptr, 0, 64);
        for (int i = 0; i < n; ++i)
        {
            if (c->output()[i].on)
                ++lateOns;
            else
                ++offs;
        }
    }
    CHECK(lateOns == 0);
    CHECK(ons == offs); // alles was klingt, wurde beendet
}

static void testStress()
{
    std::printf("stress (random chords, same-pitch retrigger, all amounts high)\n");
    Rng r(42);
    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        auto c = make(sr, 2.0f, 1.0f, 1.0f, 11);
        c->setLengthAmount(1.0f);
        c->setGrooveAmount(1.0f);
        c->setAccentAmount(1.0f);
        c->setContourAmount(1.0f);
        BlockInfo stressInfo;
        stressInfo.bpm = 128.0;
        stressInfo.ppqValid = true;
        const int bs = 256;
        const int nBlocks = 3000;
        std::vector<Block> blocks(static_cast<size_t>(nBlocks));
        std::map<int, int64_t> freeAt; // Tonhoehe -> ab wann wieder frei (Eingabe bleibt gueltig)
        int nOnIn = 0, nOffIn = 0;
        std::vector<std::pair<int64_t, NoteEvent>> offs;

        for (int b = 0; b < nBlocks; ++b)
        {
            const int64_t t0 = static_cast<int64_t>(b) * bs;
            if (r.uniform() < 0.3f)
            {
                const int cnt = 1 + r.index(7);
                const int pos = r.index(bs);
                const int dur = 200 + r.index(30000);
                for (int k = 0; k < cnt; ++k)
                {
                    const int note = 48 + r.index(12);
                    if (t0 + pos + dur >= static_cast<int64_t>(nBlocks - 1) * bs)
                        continue; // Note-Off muss noch im Lauf liegen
                    if (freeAt.count(note) && freeAt[note] > t0 + pos)
                        continue;
                    freeAt[note] = t0 + pos + dur + 1;
                    blocks[static_cast<size_t>(b)].push_back(ev(pos, true, note, 1 + r.index(127)));
                    ++nOnIn;
                    offs.push_back({t0 + pos + dur, ev(0, false, note)});
                }
            }
        }
        for (auto& o : offs)
        {
            const size_t b = static_cast<size_t>(o.first / bs);
            NoteEvent e = o.second;
            e.pos = static_cast<int>(o.first % bs);
            blocks[b].push_back(e);
            ++nOffIn;
        }
        // pro Block nach Position sortieren (stabil: On vor Off bei gleicher Position)
        for (auto& blk : blocks)
            std::stable_sort(blk.begin(), blk.end(), [](const NoteEvent& a, const NoteEvent& b)
                             { return a.pos < b.pos; });

        auto o = run(*c, blocks, bs, 600, &stressInfo, sr);

        // Pro Tonhoehe muss die Ausgabe strikt on/off/on/off alternieren
        std::map<int, bool> state;
        int on = 0, off = 0;
        bool alt = true;
        for (auto& e : o)
        {
            CHECK(e.vel >= 0 && e.vel <= 127);
            bool& s = state[e.note];
            if (e.on)
            {
                if (s)
                    alt = false;
                s = true;
                ++on;
            }
            else
            {
                if (!s)
                    alt = false;
                s = false;
                ++off;
            }
        }
        CHECK(alt);
        CHECK(on == off);
        CHECK(on > 500);
        CHECK(on == nOnIn && off == nOffIn); // nichts verloren, nichts erfunden
        std::printf("  sr %.0f: %d notes through, alternation ok=%d\n", sr, on, alt ? 1 : 0);
    }
}


static std::map<int, int64_t> onsOf(const std::vector<Out>& o, int64_t from = 0, int64_t to = (int64_t)1 << 60)
{
    std::map<int, int64_t> m;
    for (auto& e : o)
        if (e.on && e.abs >= from && e.abs < to)
            m[e.note] = e.abs;
    return m;
}

static void testDirectionModes()
{
    std::printf("direction modes\n");
    for (uint64_t seed = 1; seed <= 100; ++seed)
    {
        // tief -> hoch erzwungen: tiefste Note bleibt vorne
        {
            auto c = make(44100.0, 1.0f, 0.0f, 0.0f, seed);
            c->setDirectionMode(kDirLowToHigh);
            std::vector<Block> b(1);
            b[0] = {ev(100, true, 50), ev(100, true, 57), ev(100, true, 64)};
            auto on = onsOf(run(*c, b, 512));
            CHECK(on[50] == 100);
            CHECK(on[50] <= on[57] && on[57] <= on[64]);
            CHECK(on[64] > 100);
        }
        // hoch -> tief erzwungen: hoechste Note bleibt vorne
        {
            auto c = make(44100.0, 1.0f, 0.0f, 0.0f, seed);
            c->setDirectionMode(kDirHighToLow);
            std::vector<Block> b(1);
            b[0] = {ev(100, true, 50), ev(100, true, 57), ev(100, true, 64)};
            auto on = onsOf(run(*c, b, 512));
            CHECK(on[64] == 100);
            CHECK(on[64] <= on[57] && on[57] <= on[50]);
            CHECK(on[50] > 100);
        }
    }
    // Alternate: Down, Up, Down, Up ...
    auto c = make(44100.0, 1.0f, 0.0f, 0.0f, 5);
    c->setDirectionMode(kDirAlternate);
    std::vector<Block> blocks(12);
    for (int g = 0; g < 6; ++g)
        blocks[static_cast<size_t>(g * 2)] = {ev(10, true, 50), ev(10, true, 57), ev(10, true, 64)};
    auto o = run(*c, blocks, 4000, 20);
    for (int g = 0; g < 6; ++g)
    {
        auto on = onsOf(o, static_cast<int64_t>(g) * 2 * 4000, static_cast<int64_t>(g) * 2 * 4000 + 4000 * 2);
        if (g % 2 == 0)
            CHECK(on[50] < on[64]);
        else
            CHECK(on[64] < on[50]);
    }
}

static void testBypass()
{
    std::printf("bypass\n");
    auto c = make(44100.0, 4.0f, 2.0f, 1.0f, 3);
    c->setAccentAmount(1.0f);
    c->setBypass(true);
    std::vector<Block> b(1);
    b[0] = {ev(10, true, 50, 80), ev(10, true, 57, 81), ev(10, true, 64, 82)};
    auto on = run(*c, b, 512);
    int n = 0;
    for (auto& e : on)
        if (e.on)
        {
            ++n;
            CHECK(e.abs == 10 && e.vel >= 80 && e.vel <= 82);
        }
    CHECK(n == 3);

    // Bypass waehrend einer verschobenen Note einschalten: Note-Off muss trotzdem passen
    auto d = make(44100.0, 4.0f, 0.0f, 0.0f, 9);
    std::vector<Block> b1(1);
    b1[0] = {ev(0, true, 50), ev(0, true, 57), ev(0, true, 64)};
    std::vector<Out> res = run(*d, b1, 128, 0);
    d->setBypass(true);
    std::vector<Block> b2(1);
    b2[0] = {ev(5, false, 50), ev(5, false, 57), ev(5, false, 64)};
    auto r2 = run(*d, b2, 128, 300);
    int offs = 0, ons = 0;
    for (auto& e : res)
        ons += e.on ? 1 : 0;
    for (auto& e : r2)
        offs += e.on ? 0 : 1;
    // Rest der Ons kann noch ausstehend gewesen sein und kommt in r2
    for (auto& e : r2)
        ons += e.on ? 1 : 0;
    CHECK(ons == 3 && offs == 3);
}

static void testAccentAndContour()
{
    std::printf("accent + contour\n");
    const double sr = 48000.0;
    BlockInfo info;
    info.bpm = 120.0;
    info.ppqValid = true;
    info.ppqStart = 0.0;
    info.barStartPpq = 0.0;
    info.tsNum = 4;
    info.tsDen = 4;

    // 120 BPM @ 48k: 1 Beat = 24000 Samples; ein Block = 1 Takt
    auto c = make(sr, 1.0f, 0.0f, 0.0f, 1);
    c->setAccentAmount(1.0f);
    std::vector<Block> b(1);
    b[0] = {ev(0, true, 50, 64), ev(6000, true, 52, 64), ev(12000, true, 54, 64), ev(24000, true, 56, 64)};
    auto o = run(*c, b, 96000, 2, &info, sr);
    std::map<int, int> v;
    for (auto& e : o)
        if (e.on)
            v[e.note] = e.vel;
    CHECK(v[50] == 74);                   // Taktanfang  +10
    CHECK(v[56] == 67);                   // Zaehlzeit 2 +3
    CHECK(v[54] == 62 || v[54] == 63);    // Achtel-Offbeat -1.5
    CHECK(v[52] == 60 || v[52] == 61);    // Sechzehntel   -3.5
    CHECK(v[50] > v[56] && v[56] > v[54] && v[54] > v[52]);

    // ohne Taktposition (ppqValid = false) kein Akzent
    auto d = make(sr, 1.0f, 0.0f, 0.0f, 1);
    d->setAccentAmount(1.0f);
    BlockInfo none;
    auto o2 = run(*d, b, 96000, 2, &none, sr);
    for (auto& e : o2)
        if (e.on)
            CHECK(e.vel == 64);

    // Kontur: steigende Melodie lauter, fallende leiser
    auto k = make(sr, 1.0f, 0.0f, 0.0f, 1);
    k->setContourAmount(1.0f);
    std::vector<Block> m(1);
    m[0] = {ev(0, true, 60, 64), ev(5000, true, 67, 64), ev(10000, true, 60, 64)};
    auto o3 = run(*k, m, 20000, 2, nullptr, sr);
    std::vector<int> vs;
    for (auto& e : o3)
        if (e.on)
            vs.push_back(e.vel);
    CHECK(vs.size() == 3);
    if (vs.size() == 3)
    {
        CHECK(vs[0] == 64);
        CHECK(vs[1] > 64);
        CHECK(vs[2] < 64);
    }
}


static void testVelocityIsPitchBased()
{
    std::printf("velocity profiles are pitch-ordered (no mirroring)\n");
    for (int dir : {kDirLowToHigh, kDirHighToLow})
        for (int n : {3, 4, 5})
        {
            double sum = 0;
            const int runs = 200;
            for (int seed = 1; seed <= runs; ++seed)
            {
                auto c = make(44100.0, 0.0f, 1.0f, 0.0f, static_cast<uint64_t>(seed));
                c->setDriftEnabled(false);
                c->setDirectionMode(dir);
                std::vector<Block> b(1);
                for (int k = 0; k < n; ++k)
                    b[0].push_back(ev(10, true, 48 + k * 4, 64));
                auto o = run(*c, b, 512, 2);
                std::map<int, int> v;
                for (auto& e : o)
                    if (e.on)
                        v[e.note] = e.vel;
                sum += v[48 + (n - 1) * 4] - v[48];
            }
            const double mean = sum / runs;
            std::printf("  dir %d, %d notes: top - bottom = %+.1f\n", dir, n, mean);
            // Die Daten selbst: bei aufwaerts gespielten 3-Noten-Akkorden ist der Unterschied klein (~+2.7),
            // sonst deutlich. Entscheidend: nie negativ (das haette die alte Spiegelung geliefert).
            CHECK(mean > 1.0);
            if (n >= 4)
                CHECK(mean > 10.0);
        }
}

static void testGroupWindow()
{
    std::printf("group window\n");
    int different = 0;
    for (uint64_t seed = 1; seed <= 60; ++seed)
    {
        auto c = make(44100.0, 4.0f, 0.0f, 0.0f, seed);
        c->setGroupWindowMs(5.0f); // 10 ms Abstand -> keine Gruppe
        std::vector<Block> b(1);
        b[0] = {ev(100, true, 50), ev(541, true, 57), ev(982, true, 64)};
        auto on = onsOf(run(*c, b, 2048));
        CHECK(on[50] == 100 && on[57] == 541 && on[64] == 982);

        auto d = make(44100.0, 4.0f, 0.0f, 0.0f, seed);
        d->setGroupWindowMs(40.0f); // gleiche Noten jetzt eine Gruppe
        auto on2 = onsOf(run(*d, b, 2048));
        if (on2[50] != 100 || on2[57] != 541 || on2[64] != 982)
            ++different;
    }
    CHECK(different > 30);
}

static double mean(const std::vector<double>& v)
{
    double s = 0;
    for (double x : v)
        s += x;
    return v.empty() ? 0.0 : s / static_cast<double>(v.size());
}

static void testFeelIsCorrelated()
{
    std::printf("timing feel is correlated (not white noise)\n");
    auto c = make(44100.0, 0.0f, 0.0f, 1.0f, 21);
    const int bs = 5000, N = 400;
    std::vector<Block> blocks(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        blocks[static_cast<size_t>(i)] = {ev(0, true, 40 + i % 30, 100), ev(2000, false, 40 + i % 30)};
    auto o = run(*c, blocks, bs, 5);
    std::vector<double> d;
    for (auto& e : o)
        if (e.on)
            d.push_back((e.abs - (e.abs / bs) * bs) * 1000.0 / 44100.0); // Verspaetung in ms (Noten liegen auf Blockanfang)
    CHECK(static_cast<int>(d.size()) == N);
    double m = mean(d), num = 0, den = 0, mx = 0;
    for (size_t i = 0; i < d.size(); ++i)
    {
        den += (d[i] - m) * (d[i] - m);
        if (i + 1 < d.size())
            num += (d[i] - m) * (d[i + 1] - m);
        mx = std::max(mx, d[i]);
    }
    const double ac = num / den;
    std::printf("  lag-1 autocorrelation %.2f, mean delay %.1f ms, max %.1f ms\n", ac, m, mx);
    CHECK(ac > 0.5);
    CHECK(mx <= 12.0 + 8.0 * (1.0 - 100.0 / 127.0) + 0.1);
    CHECK(m > 2.0 && m < 9.0);
}

static void testVelocityLag()
{
    std::printf("soft notes arrive later than loud ones\n");
    auto delayFor = [](int vel)
    {
        auto c = make(44100.0, 0.0f, 0.0f, 1.0f, 33);
        const int bs = 5000, N = 300;
        std::vector<Block> blocks(static_cast<size_t>(N));
        for (int i = 0; i < N; ++i)
            blocks[static_cast<size_t>(i)] = {ev(0, true, 50, vel), ev(1000, false, 50)};
        auto o = run(*c, blocks, bs, 5);
        std::vector<double> d;
        for (auto& e : o)
            if (e.on)
                d.push_back((e.abs % bs) * 1000.0 / 44100.0);
        return mean(d);
    };
    const double diff = delayFor(20) - delayFor(120);
    std::printf("  vel 20 vs vel 120: %.1f ms later\n", diff);
    CHECK(diff > 5.5 && diff < 7.0); // 8 ms * (120-20)/127 = 6.3 ms
}

static void testMelodyLead()
{
    std::printf("melody leads: inner notes lag behind the top note\n");
    std::vector<double> top, bottom;
    for (uint64_t seed = 1; seed <= 300; ++seed)
    {
        auto c = make(44100.0, 0.0f, 0.0f, 1.0f, seed);
        std::vector<Block> b(1);
        b[0] = {ev(0, true, 48, 90), ev(0, true, 55, 90), ev(0, true, 64, 90)};
        auto on = onsOf(run(*c, b, 2048, 5));
        top.push_back(static_cast<double>(on[64]) * 1000.0 / 44100.0);
        bottom.push_back(static_cast<double>(on[48]) * 1000.0 / 44100.0);
    }
    const double diff = mean(bottom) - mean(top);
    std::printf("  bottom - top = %.1f ms\n", diff);
    CHECK(diff > 2.0 && diff < 4.5); // erwartet ~3 ms
}

static void testGroove()
{
    std::printf("groove delays off-beats only\n");
    const double sr = 48000.0;
    BlockInfo info;
    info.bpm = 120.0;
    info.ppqValid = true;

    // 120 BPM @ 48k: 1 Beat = 24000 Samples, Achtel = 12000, Sechzehntel = 6000
    for (uint64_t seed = 1; seed <= 50; ++seed)
    {
        auto c = make(sr, 0.0f, 0.0f, 0.0f, seed);
        c->setGrooveAmount(1.0f);
        std::vector<Block> b(1);
        b[0] = {ev(0, true, 50), ev(6000, true, 52), ev(12000, true, 54), ev(24000, true, 56)};
        auto o = run(*c, b, 96000, 2, &info, sr);
        std::map<int, double> ms;
        for (auto& e : o)
            if (e.on)
            {
                const int orig = e.note == 50 ? 0 : e.note == 52 ? 6000 : e.note == 54 ? 12000 : 24000;
                ms[e.note] = (e.abs - orig) * 1000.0 / sr;
            }
        CHECK(ms[50] == 0.0 && ms[56] == 0.0);            // auf der Zaehlzeit: nichts
        CHECK(ms[54] > 31.0 && ms[54] < 49.0);            // Achtel-Offbeat: 0.16 * 250 ms * (0.8..1.2)
        CHECK(ms[52] > 17.0 && ms[52] < 27.0);            // Sechzehntel: 0.55 davon

        // ohne Taktposition keine Wirkung
        auto d = make(sr, 0.0f, 0.0f, 0.0f, seed);
        d->setGrooveAmount(1.0f);
        BlockInfo none;
        auto o2 = run(*d, b, 96000, 2, &none, sr);
        for (auto& e : o2)
            if (e.on)
                CHECK(e.abs == (e.note == 50 ? 0 : e.note == 52 ? 6000 : e.note == 54 ? 12000 : 24000));
    }
}

static void testLength()
{
    std::printf("length extends note ends\n");
    std::vector<double> ext;
    for (uint64_t seed = 1; seed <= 400; ++seed)
    {
        auto c = make(44100.0, 0.0f, 0.0f, 0.0f, seed);
        c->setLengthAmount(1.0f);
        std::vector<Block> b(1);
        b[0] = {ev(100, true, 60), ev(4000, false, 60)};
        auto o = run(*c, b, 8192, 20);
        int64_t on = -1, off = -1;
        for (auto& e : o)
            (e.on ? on : off) = e.abs;
        CHECK(on == 100);
        CHECK(off >= 4000 && off <= 4000 + 2646 + 1);
        ext.push_back(static_cast<double>(off - 4000) * 1000.0 / 44100.0);
    }
    const double m = mean(ext);
    std::printf("  mean extension %.1f ms\n", m);
    CHECK(m > 10.0 && m < 20.0); // E[r1*r2]*60 = 15 ms
}

static void testSamePitchCollision()
{
    std::printf("same pitch retrigger: pull the old note-off in instead of delaying the new note\n");
    for (int bs : {4096, 512})
        for (uint64_t seed = 1; seed <= 300; ++seed)
        {
            auto c = make(44100.0, 0.0f, 0.0f, 0.0f, seed);
            c->setLengthAmount(1.0f);
            // sortierte Eingabe ueber mehrere Bloecke verteilt
            std::vector<Block> blocks(static_cast<size_t>(4096 / bs + 2));
            auto put = [&](int abs, bool on, int note)
            {
                blocks[static_cast<size_t>(abs / bs)].push_back(ev(abs % bs, on, note, 90));
            };
            put(0, true, 60);
            put(1000, false, 60);
            put(1100, true, 60);
            put(3000, false, 60);
            auto o = run(*c, blocks, bs, 200);
            std::vector<Out> seq;
            for (auto& e : o)
                if (e.note == 60)
                    seq.push_back(e);
            CHECK(seq.size() == 4);
            if (seq.size() == 4)
            {
                CHECK(seq[0].on && !seq[1].on && seq[2].on && !seq[3].on);
                CHECK(seq[2].abs == 1100);          // neues Note-On NICHT verspaetet
                CHECK(seq[1].abs <= 1100);          // altes Note-Off davor
                CHECK(seq[1].abs >= seq[0].abs);    // aber nie vor seinem Note-On
                CHECK(seq[3].abs >= 3000);
            }
        }
}

static void testSoftness()
{
    std::printf("soft: lowers and compresses velocities\n");
    auto avgFor = [](float soft, int vel)
    {
        auto c = make(44100.0, 0.0f, 0.0f, 0.0f, 5);
        c->setSoftness(soft);
        std::vector<Block> blocks(50);
        for (auto& b : blocks)
            b = {ev(0, true, 60, vel), ev(500, false, 60)};
        auto o = run(*c, blocks, 2048, 5);
        double s = 0;
        int n = 0;
        for (auto& e : o)
            if (e.on)
            {
                s += e.vel;
                ++n;
            }
        return s / n;
    };
    CHECK(std::fabs(avgFor(0.0f, 100) - 100.0) < 8.0);   // aus = unveraendert (Rest: Mikro-Streuung)
    CHECK(std::fabs(avgFor(1.0f, 127) - 87.5) < 8.0);    // 24 + 0.5*127
    CHECK(std::fabs(avgFor(1.0f, 64) - 56.0) < 8.0);
    CHECK(avgFor(1.0f, 127) - avgFor(1.0f, 64) < 0.6 * 63.0); // Spanne gestaucht
}

static void testBassCare()
{
    std::printf("bass care: deep notes get softer\n");
    auto avgFor = [](float bass, int note)
    {
        auto c = make(44100.0, 0.0f, 0.0f, 0.0f, 9);
        c->setBassCare(bass);
        std::vector<Block> blocks(50);
        for (auto& b : blocks)
            b = {ev(0, true, note, 100), ev(500, false, note)};
        auto o = run(*c, blocks, 2048, 5);
        double s = 0;
        int n = 0;
        for (auto& e : o)
            if (e.on)
            {
                s += e.vel;
                ++n;
            }
        return s / n;
    };
    CHECK(std::fabs(avgFor(1.0f, 36) - 65.0) < 6.0);  // C2: -35 %
    CHECK(std::fabs(avgFor(1.0f, 44) - 100.0 * (1 - 0.35 * 0.5)) < 6.0); // halbe Rampe
    CHECK(std::fabs(avgFor(1.0f, 60) - 100.0) < 6.0); // darueber unveraendert
    CHECK(std::fabs(avgFor(0.0f, 36) - 100.0) < 6.0); // aus
}

static void testUiRings()
{
    std::printf("ui rings\n");
    auto c = make(44100.0, 1.0f, 1.0f, 0.0f, 8);
    Block b = {ev(10, true, 67, 100), ev(10, true, 60, 90), ev(10, true, 64, 95)};
    c->process(b.data(), 3, 256);
    NoteInfo ni;
    int nInfos = 0;
    while (c->popNoteInfo(ni))
        ++nInfos;
    CHECK(nInfos == 3);
    GroupReport g;
    CHECK(c->popGroupReport(g));
    CHECK(g.count == 3);
    CHECK(g.note[0] == 60 && g.note[1] == 64 && g.note[2] == 67); // nach Tonhoehe sortiert
    CHECK(g.velIn[0] == 90 && g.velIn[2] == 100);
    CHECK(!c->popGroupReport(g));
}

static void testDeterminism()
{
    std::printf("determinism\n");
    auto a = make(44100.0, 1.5f, 1.0f, 1.0f, 1234);
    auto b = make(44100.0, 1.5f, 1.0f, 1.0f, 1234);
    std::vector<Block> blocks(1);
    blocks[0] = {ev(5, true, 40, 70), ev(5, true, 47, 70), ev(5, true, 52, 70), ev(5, true, 56, 70)};
    auto oa = run(*a, blocks, 512);
    auto ob = run(*b, blocks, 512);
    CHECK(oa.size() == ob.size());
    for (size_t i = 0; i < oa.size() && i < ob.size(); ++i)
        CHECK(oa[i].abs == ob[i].abs && oa[i].vel == ob[i].vel && oa[i].note == ob[i].note);
}

int main()
{
    testChordBasics();
    testNeutralSettings();
    testCrossBlock();
    testSampleRateIndependence();
    testDirectionAndMirroring();
    testVelocityMeanPreserved();
    testBigGroups();
    testPanic();
    testDirectionModes();
    testBypass();
    testAccentAndContour();
    testVelocityIsPitchBased();
    testFeelIsCorrelated();
    testVelocityLag();
    testMelodyLead();
    testGroove();
    testLength();
    testSoftness();
    testBassCare();
    testSamePitchCollision();
    testGroupWindow();
    testStress();
    testUiRings();
    testDeterminism();

    std::printf("\n%d checks, %d failed\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
