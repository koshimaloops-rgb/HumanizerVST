#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class HumanizerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    HumanizerAudioProcessorEditor(HumanizerAudioProcessor &);
    ~HumanizerAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void changePlaceholderText(const juce::String &newText);
    void changePlaceholderTextVelocity(const juce::String &newTextVelocity);
    void changeIntervalText(const juce::String &newInterval);
    void changeRecentNotesText(const juce::String &newNotes);
    void changeDurationText(const juce::String &newDuration);
    void changeVelocityDifferenceText(const juce::String &newDifference);
    void changeTimeBetweenText(const juce::String &newTime);
    void changeBeatDurationText(const juce::String &newDuration);
    void changeTimingDeviationText(const juce::String &newDeviation);
    void changePpqPositionText(const juce::String &newPosition);

private:
    HumanizerAudioProcessor &audioProcessor;
    
    juce::Label recentNotesLabel;
    juce::Label timeBetweenLabel;
    juce::Label durationLabel;
    juce::Label midiPlaceholder;
    juce::Label ppqPositionLabel;
    juce::Slider velocityHumanizeSlider;
    juce::Label velocityPlaceholder;
    juce::Label velocityDifferenceLabel;
    juce::Label sliderLabel;
    juce::Label timingDeviationLabel;
    juce::Slider threshholdHumanizeSlider;
    juce::Label sliderLabelThreshhold;
    juce::Label intervalLabel;
    juce::Label beatDurationLabel;
    float keyStroke;
    float velocityThreshold;

    // DEKLARATION
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        HumanizerAudioProcessorEditor)
};