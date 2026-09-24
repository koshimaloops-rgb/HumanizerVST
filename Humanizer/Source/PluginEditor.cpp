#include "PluginProcessor.h"
#include "PluginEditor.h"

HumanizerAudioProcessorEditor::HumanizerAudioProcessorEditor(
    HumanizerAudioProcessor &p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    setSize(500, 500);
    addAndMakeVisible(velocityPlaceholder);
    addAndMakeVisible(midiPlaceholder);
    velocityPlaceholder.setText("No MIDI note played yet", juce::dontSendNotification);
    velocityPlaceholder.setJustificationType(juce::Justification::centred);
    velocityPlaceholder.setColour(juce::Label::textColourId, juce::Colours::white);
    midiPlaceholder.setText("No MIDI note played yet", juce::dontSendNotification);
    midiPlaceholder.setJustificationType(juce::Justification::centred);
    midiPlaceholder.setColour(juce::Label::textColourId, juce::Colours::white);
    recentNotesLabel.setText("NO recent notes yet", juce::dontSendNotification);
    recentNotesLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(recentNotesLabel);

    addAndMakeVisible(velocityHumanizeSlider);

    addAndMakeVisible(threshholdHumanizeSlider);
    // 2. Sagen, dass es ein runder Drehregler (Knob) sein soll
    velocityHumanizeSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    threshholdHumanizeSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);

    // 3. Den Text-Wert unter dem Regler anzeigen
    velocityHumanizeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    threshholdHumanizeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

    velocityHumanizeSlider.onValueChange = [this]()
    {
        audioProcessor.setVelocityHumanize(
            (float)velocityHumanizeSlider.getValue());

        velocityPlaceholder.setText(
            juce::String(velocityHumanizeSlider.getValue()),
            juce::dontSendNotification);
    };
    // 4. Den Bereich einstellen: von 0.0 (kein Humanize) bis 100.0 (maximales Chaos)
    velocityHumanizeSlider.setRange(0.0, 100.0, 1.0);
    velocityHumanizeSlider.setValue(0.0); // Startwert
    velocityHumanizeSlider.onValueChange = [this]()
    {
        audioProcessor.setVelocityHumanize(
            (float)velocityHumanizeSlider.getValue());
    };
    threshholdHumanizeSlider.setValue(0.0);

    threshholdHumanizeSlider.setRange(0.0, 100.0, 1.0);

    // --- BONUS: Beschriftung für den Slider ---
    addAndMakeVisible(sliderLabel);
    sliderLabel.setText("Velocity Humanize", juce::dontSendNotification);
    sliderLabel.setJustificationType(juce::Justification::centred);
    sliderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(sliderLabelThreshhold);
    sliderLabelThreshhold.setText("Thresshold Humanize", juce::dontSendNotification);
    sliderLabelThreshhold.setJustificationType(juce::Justification::centred);
    sliderLabelThreshhold.setColour(juce::Label::textColourId, juce::Colours::white);
    intervalLabel.setText("Interval: -", juce::dontSendNotification);
    addAndMakeVisible(intervalLabel);
    addAndMakeVisible(durationLabel);
    addAndMakeVisible(timeBetweenLabel);

    timeBetweenLabel.setText(
        "Time between: -",
        juce::dontSendNotification);

    timeBetweenLabel.setFont(
        juce::Font(18.0f));

    timeBetweenLabel.setColour(
        juce::Label::textColourId,
        juce::Colours::white);

    durationLabel.setText(
        "Duration: -",
        juce::dontSendNotification);

    durationLabel.setFont(
        juce::Font(18.0f));

    durationLabel.setColour(
        juce::Label::textColourId,
        juce::Colours::white);
}

HumanizerAudioProcessorEditor::~HumanizerAudioProcessorEditor()
{
}

void HumanizerAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::black);

    g.setColour(juce::Colours::black);

    g.setFont(30.0f);

    g.drawText(
        "HUMANIZER",
        getLocalBounds(),
        juce::Justification::topLeft);
}

void HumanizerAudioProcessorEditor::resized()
{
    timeBetweenLabel.setBounds(50, 240, 400, 40);
    midiPlaceholder.setBounds(50, 50, 300, 50);
    velocityPlaceholder.setBounds(80, 80, 350, 79);
    durationLabel.setBounds(50, 200, 400, 40);
    sliderLabel.setBounds(75, 450, 100, 20);
    intervalLabel.setBounds(20, 120, 200, 30);
    // Der Slider selbst kommt darunter (schön groß, damit man ihn drehen kann)
    velocityHumanizeSlider.setBounds(75, 350, 100, 100);
    threshholdHumanizeSlider.setBounds(350, 350, 100, 100);
    sliderLabelThreshhold.setBounds(350, 450, 100, 20);
    recentNotesLabel.setBounds(50, 160, 400, 40);

    // AUSSEHEN
}
void HumanizerAudioProcessorEditor::changePlaceholderText(const juce::String &newText)
{
    midiPlaceholder.setText(newText, juce::dontSendNotification);
};

void HumanizerAudioProcessorEditor::changePlaceholderTextVelocity(const juce::String &newTextVelocity)
{
    velocityPlaceholder.setText(newTextVelocity, juce::dontSendNotification);
}
void HumanizerAudioProcessorEditor::changeIntervalText(const juce::String &newInterval)
{
    intervalLabel.setText("Interval: " + newInterval + " semitones",
                          juce::dontSendNotification);
}
void HumanizerAudioProcessorEditor::changeRecentNotesText(const juce::String &newNotes)
{
    recentNotesLabel.setText("recent Notes " + newNotes, juce::dontSendNotification);
}
void HumanizerAudioProcessorEditor::changeDurationText(
    const juce::String &newDuration)
{
    durationLabel.setText(
        "Duration: " + newDuration + " samples",
        juce::dontSendNotification);
}
void HumanizerAudioProcessorEditor::changeTimeBetweenText(
    const juce::String &newTime)
{
    timeBetweenLabel.setText(
        "Time between: " + newTime + " samples",
        juce::dontSendNotification);
}