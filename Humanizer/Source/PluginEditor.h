#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class HumanizerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    HumanizerAudioProcessorEditor(HumanizerAudioProcessor&);
    ~HumanizerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void changePlaceholderText (const juce::String& newText);
    void changePlaceholderTextVelocity(const juce:: String& newTextVelocity);
    void changeIntervalText(const juce::String& newInterval);
    void changeRecentNotesText(const juce::String &newNotes);
    void changeDurationText(const juce::String& newDuration);
void changeTimeBetweenText(const juce::String& newTime);
private:
    HumanizerAudioProcessor& audioProcessor;
    juce::Label recentNotesLabel;
    juce::Label timeBetweenLabel;
juce::Label durationLabel;
    juce::Label midiPlaceholder;
    juce::Slider velocityHumanizeSlider;
    juce::Label velocityPlaceholder;
    juce::Label sliderLabel;
    juce::Slider threshholdHumanizeSlider;
    juce::Label sliderLabelThreshhold;
    juce::Label intervalLabel;
     float keyStroke; 
     float velocityThreshold;
    


//DEKLARATION
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        HumanizerAudioProcessorEditor)

        
};