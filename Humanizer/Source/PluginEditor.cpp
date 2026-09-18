#include "PluginProcessor.h"
#include "PluginEditor.h"

HumanizerAudioProcessorEditor::HumanizerAudioProcessorEditor(
    HumanizerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    setSize(500, 500);
    addAndMakeVisible(midiPlaceholder);
    midiPlaceholder.setText ("No MIDI note played yet", juce::dontSendNotification);
    midiPlaceholder.setJustificationType (juce::Justification::centred);
    midiPlaceholder.setColour (juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible (velocityHumanizeSlider);

    addAndMakeVisible(threshholdHumanizeSlider);
// 2. Sagen, dass es ein runder Drehregler (Knob) sein soll
velocityHumanizeSlider.setSliderStyle (juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
threshholdHumanizeSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);

// 3. Den Text-Wert unter dem Regler anzeigen
velocityHumanizeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
threshholdHumanizeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false , 60 ,20);


// 4. Den Bereich einstellen: von 0.0 (kein Humanize) bis 100.0 (maximales Chaos)
velocityHumanizeSlider.setRange (0.0, 100.0, 1.0);
velocityHumanizeSlider.setValue (0.0); // Startwert
threshholdHumanizeSlider.setValue(0.0);

threshholdHumanizeSlider.setRange(0.0, 100.0 ,1.0);

// --- BONUS: Beschriftung für den Slider ---
addAndMakeVisible (sliderLabel);
sliderLabel.setText ("Velocity Humanize", juce::dontSendNotification);
sliderLabel.setJustificationType (juce::Justification::centred);
sliderLabel.setColour (juce::Label::textColourId, juce::Colours::white);
addAndMakeVisible(sliderLabelThreshhold);
sliderLabelThreshhold.setText ("Thresshold Humanize", juce::dontSendNotification);
sliderLabelThreshhold.setJustificationType (juce::Justification::centred);
sliderLabelThreshhold.setColour (juce::Label::textColourId, juce::Colours::white);

}


HumanizerAudioProcessorEditor::~HumanizerAudioProcessorEditor()
{
}

void HumanizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkturquoise);

    g.setColour(juce::Colours::white);
    
    g.setFont(30.0f);

    g.drawText(
        "HUMANIZER",
        getLocalBounds(),
        juce::Justification::topLeft);
}






void HumanizerAudioProcessorEditor::resized()
{
    midiPlaceholder.setBounds (50, 50, 300, 50); 
     sliderLabel.setBounds (75, 450, 100, 20);

    // Der Slider selbst kommt darunter (schön groß, damit man ihn drehen kann)
    velocityHumanizeSlider.setBounds (75, 350, 100, 100);
   threshholdHumanizeSlider.setBounds (350, 350, 100, 100);
    sliderLabelThreshhold.setBounds (350, 450, 100, 20);

    //AUSSEHEN

}
void HumanizerAudioProcessorEditor::changePlaceholderText (const juce::String& newText)
{
    midiPlaceholder.setText (newText, juce::dontSendNotification);
}
