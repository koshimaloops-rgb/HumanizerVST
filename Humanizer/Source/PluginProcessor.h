#pragma once

#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include <cstdint>

// =============================================================
// PENDING MIDI EVENT
// =============================================================

struct PendingMidiEvent
{
    juce::MidiMessage message;
    int64_t targetSample;
};

// =============================================================
// HUMANIZER AUDIO PROCESSOR
// =============================================================

class HumanizerAudioProcessor
    : public juce::AudioProcessor
{
public:
    // =========================================================
    // CONSTRUCTOR / DESTRUCTOR
    // =========================================================

    HumanizerAudioProcessor();

    ~HumanizerAudioProcessor() override;

    // =========================================================
    // BASIC AUDIO PROCESSOR FUNCTIONS
    // =========================================================

    const juce::String getName() const override;

    bool acceptsMidi() const override;

    bool producesMidi() const override;

    bool isMidiEffect() const override;

    double getTailLengthSeconds() const override;

    // =========================================================
    // PROGRAMS
    // =========================================================

    int getNumPrograms() override;

    int getCurrentProgram() override;

    void setCurrentProgram(int index) override;

    const juce::String getProgramName(
        int index) override;

    void changeProgramName(
        int index,
        const juce::String &newName) override;

    // =========================================================
    // AUDIO
    // =========================================================

    void prepareToPlay(
        double sampleRate,
        int samplesPerBlock) override;

    void releaseResources() override;

    bool isBusesLayoutSupported(
        const BusesLayout &layouts) const override;

    // =========================================================
    // PROCESS MIDI
    // =========================================================

    void processBlock(
        juce::AudioBuffer<float> &,
        juce::MidiBuffer &) override;

    // =========================================================
    // EDITOR
    // =========================================================

    bool hasEditor() const override;

    juce::AudioProcessorEditor *
    createEditor() override;

    // =========================================================
    // STATE
    // =========================================================

    void getStateInformation(
        juce::MemoryBlock &destData) override;

    void setStateInformation(
        const void *data,
        int sizeInBytes) override;

    // =========================================================
    // HUMANIZATION SETTINGS
    // =========================================================

    void setVelocityHumanize(
        float amount);
    void applyLearnedStrumToGroup();
    void setTimingHumanize(
        float amount);
    void applyLearnedVelocityToGroup();

    void setStrumIntensity(
        float amount);

    // =========================================================
    // LEARNED STRUM
    // =========================================================

    struct PendingGroupNote
    {
        juce::MidiMessage message;
        int64_t originalSample;
    };
    struct PendingGroupNoteOff
    {
        juce::MidiMessage message;
        int64_t originalSample;
    };
    juce::String getVelocityGroupDisplay() const;
    enum class VelocityDirection
    {
        ascending,
        descending,
        mixed
    };

   
    std::vector<PendingGroupNoteOff> pendingGroupNoteOffs;

    // =========================================================
    // PUBLIC
    // =========================================================

private:
    // =========================================================
    // HUMANIZATION VALUES
    // =========================================================
    juce::String velocityGroupDisplay;
    
    float velocityHumanizeAmount = 0.0f;

    float timingHumanizeAmount = 0.0f;

    // 0.0 = kein Strum
    // 1.0 = gelerntes normales Strum
    // 2.0 = doppelt so stark

    float strumIntensity = 1.0f;
    double beatDurationMs = 60000.0 / 130.0;

    // =========================================================
    // SAMPLE / TIMING
    // =========================================================

    int64_t totalSamples = 0;

    // =========================================================
    // GROUP SYSTEM
    // =========================================================

    int64_t groupStartSample = -1;

    int64_t currentGroupId = 0;

    juce::String currentGroupNotes;

    // =========================================================
    // CURRENT STRUM GROUP
    // =========================================================

    std::vector<PendingGroupNote>
        currentGroupEvents;

    // =========================================================
    // HUMANIZED GROUP TIMING
    // =========================================================

    int64_t currentHumanizedGroupId = -1;

    float currentGroupTimingOffsetMs = 0.0f;

    float currentNoteTimingOffsetMs = 0.0f;

    // =========================================================
    // HELD NOTES
    // =========================================================

    std::vector<int>
        heldNotes;

    // =========================================================
    // RECENT NOTE DATA
    // =========================================================

    struct RecentNote
    {
        int noteNumber = 0;

        int velocity = 0;

        int samplePosition = 0;

        int64_t absoluteSample = 0;

        int64_t noteOffSample = -1;

        int64_t duration = 0;

        int groupId = 0;
    };

    std::vector<RecentNote>
        recentNotes;

    // =========================================================
    // PENDING MIDI OUTPUT
    // =========================================================

    std::vector<PendingMidiEvent>
        pendingMidiEvents;

    struct NoteTimingShift
    {
        int noteNumber = 0;
        int64_t noteOnOriginalSample = 0;
        int64_t offsetSamples = 0;
    };

    std::vector<NoteTimingShift> noteTimingShifts;

    // =========================================================
    // JUCE
    // =========================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        HumanizerAudioProcessor)
};

// =============================================================
// PLUGIN CREATOR
// =============================================================

juce::AudioProcessor *
    JUCE_CALLTYPE
    createPluginFilter();