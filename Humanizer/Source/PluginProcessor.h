#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <utility>
#include <vector>

#include "HumanizerCore.h"

// =============================================================
// Parameter-IDs (gleiche Strings in Processor und Editor)
// =============================================================

namespace HumanizerParams
{
static constexpr const char *amount = "amount";       // 0..1.5 (Master: skaliert alle Humanize-Regler)
static constexpr const char *soft = "soft";           // 0..1   (leise, komprimierte Spielweise)
static constexpr const char *bass = "bass";           // 0..1   (tiefe Noten leiser)
static constexpr const char *strum = "strum";         // 0..3   (1 = gelernte Strum-Breite)
static constexpr const char *velocity = "velocity";   // 0..2   (1 = gelernte Velocity-Profile)
static constexpr const char *timing = "timing";       // 0..1   (zufaellige Verzoegerung)
static constexpr const char *accent = "accent";       // 0..1   (Betonung nach Taktposition)
static constexpr const char *contour = "contour";     // 0..1   (Velocity folgt der Melodie)
static constexpr const char *groove = "groove";       // 0..1   (Swing/Laid-back auf Off-Beats)
static constexpr const char *length = "length";       // 0..1   (Notenlaenge / Legato variieren)
static constexpr const char *window = "window";       // 5..60 ms (Akkord-Erkennung)
static constexpr const char *direction = "direction"; // Auto / Low to High / High to Low / Alternate
static constexpr const char *bypass = "bypass";
} // namespace HumanizerParams

// Daten fuer die Anzeige im Editor (werden im Message-Thread gebaut)
struct HumanizerMonitor
{
    juce::String lastNote;
    juce::String lastVelocity;
    juce::String interval;
    juce::String deltaMs;
    juce::String deltaVelocity;
    juce::String recent;
    double bpm = 0.0; // 0 = unbekannt
};

// =============================================================
// HUMANIZER AUDIO PROCESSOR
//
// Duenner JUCE-Wrapper um hz::HumanizerCore.
//  - Alle Regler sind echte Parameter (automatisierbar, werden mit
//    dem Projekt gespeichert)
//  - processBlock: MidiBuffer -> Kern -> MidiBuffer, ohne Allokation
//  - Anzeige: der Audio-Thread schreibt nur in lock-free Ringe, ein
//    Timer (Message-Thread) liest sie und fuettert den Editor
// =============================================================

class HumanizerAudioProcessor
    : public juce::AudioProcessor,
      private juce::Timer
{
public:
    HumanizerAudioProcessor();
    ~HumanizerAudioProcessor() override;

    // ---- AudioProcessor ----------------------------------------
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    bool hasEditor() const override;
    juce::AudioProcessorEditor *createEditor() override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    // ---- Parameter -------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void timerCallback() override;

    hz::HumanizerCore core;

    // Rohwerte der Parameter (atomar, vom Audio-Thread gelesen)
    std::atomic<float> *pAmount = nullptr;
    std::atomic<float> *pSoft = nullptr;
    std::atomic<float> *pBass = nullptr;
    std::atomic<float> *pStrum = nullptr;
    std::atomic<float> *pVelocity = nullptr;
    std::atomic<float> *pTiming = nullptr;
    std::atomic<float> *pAccent = nullptr;
    std::atomic<float> *pContour = nullptr;
    std::atomic<float> *pGroove = nullptr;
    std::atomic<float> *pLength = nullptr;
    std::atomic<float> *pWindow = nullptr;
    std::atomic<float> *pDirection = nullptr;
    std::atomic<float> *pBypass = nullptr;

    // Arbeitspuffer, einmal im Konstruktor reserviert
    std::vector<hz::NoteEvent> inBuf;
    std::vector<std::pair<juce::MidiMessage, int>> passthrough;

    // Tempo fuer die Anzeige (Audio-Thread schreibt, Timer liest), 0 = unbekannt
    std::atomic<double> hostBpm{0.0};

    // --- nur Message-Thread ---
    std::vector<hz::NoteInfo> recentNotes; // letzte 8 Noten
    HumanizerMonitor monitor;
    double shownBpm = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HumanizerAudioProcessor)
};

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter();
