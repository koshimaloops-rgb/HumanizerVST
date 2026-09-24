#pragma once
#include <vector>
#include <JuceHeader.h>

class HumanizerAudioProcessor : public juce::AudioProcessor
{
public:
    HumanizerAudioProcessor();
    ~HumanizerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    struct NoteData
    {
        int noteNumber;
        int velocity;
        int samplePosition;
        int64_t absoluteSample;
        int groupId;
        int64_t noteOffSample = -1;
        int64_t duration = -1;
    };
    std::vector<NoteData> recentNotes;
    int64_t totalSamples = 0;
    int64_t groupStartSample = -1;
    int currentGroupId = 0;
    juce::String currentGroupNotes;
    std::vector<int> heldNotes;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    void setVelocityHumanize(float amount);
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

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

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HumanizerAudioProcessor)
    float velocityHumanizeAmount = 0.0f;
};
