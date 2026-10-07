#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "PluginProcessor.h"

// =============================================================
// Farben
// =============================================================

namespace ui
{
static const juce::Colour bg{0xff0f1218};
static const juce::Colour panel{0xff171b24};
static const juce::Colour panelHigh{0xff1e2330};
static const juce::Colour border{0xff262d3c};
static const juce::Colour text{0xffe7eaf1};
static const juce::Colour muted{0xff7f8799};
static const juce::Colour accent{0xff5eead4}; // mint
static const juce::Colour amber{0xfffbbf24};
} // namespace ui

// =============================================================
// Look and Feel: Drehregler, Auswahlbox, Bypass-Pille
// =============================================================

class HumanizerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HumanizerLookAndFeel();

    void drawRotarySlider(juce::Graphics &, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider &) override;

    void drawComboBox(juce::Graphics &, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox &) override;

    void drawToggleButton(juce::Graphics &, juce::ToggleButton &,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

// =============================================================
// Visualisierung des letzten Akkords:
// pro Note ein Balken an der Stelle seiner Verzoegerung (ms),
// Helligkeit = Velocity, rechts "Velocity vorher -> nachher"
// =============================================================

class ChordView : public juce::Component
{
public:
    void setReport(const hz::GroupReport &r);
    void paint(juce::Graphics &) override;

private:
    hz::GroupReport report;
    bool hasReport = false;
};

// =============================================================
// Editor
// =============================================================

class HumanizerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit HumanizerAudioProcessorEditor(HumanizerAudioProcessor &);
    ~HumanizerAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

    // vom Processor (Message-Thread) aufgerufen
    void setMonitor(const HumanizerMonitor &m);
    void showGroup(const hz::GroupReport &r);

private:
    void timerCallback() override;
    void updateBypassLook();
    void applyPreset(int index);
    void addHint(juce::Component &c, const juce::String &title, const juce::String &text);

public:
    void mouseEnter(const juce::MouseEvent &) override;
    void mouseExit(const juce::MouseEvent &) override;

private:

    HumanizerAudioProcessor &audioProcessor;

    HumanizerLookAndFeel laf; // zuerst deklariert -> zuletzt zerstoert

    struct Knob
    {
        juce::Slider slider;
        juce::String name;
        juce::Rectangle<int> labelBounds;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::array<Knob, 11> knobs; // 0-2: Hauptregler, 3-10: Advanced

    juce::ComboBox directionBox;
    juce::ToggleButton bypassButton{"Bypass"};
    ChordView chordView;
    juce::ComboBox presetBox;
    juce::TextButton advancedButton{"Advanced"};
    bool advanced = false;
    void layoutKnobs();

    struct Hint
    {
        juce::Component *comp;
        juce::String title, text;
    };
    std::vector<Hint> hints;
    juce::String hintTitle, hintText;
    juce::Rectangle<int> infoBounds;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> directionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::Rectangle<int> innerBounds, knobArea, headerBounds, leftPanelBounds, dirLabelBounds, monitorBounds;
    HumanizerMonitor monitor;
    bool lastBypass = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HumanizerAudioProcessorEditor)
};
