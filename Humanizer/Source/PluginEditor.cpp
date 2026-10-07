#include "PluginEditor.h"

// =============================================================
// LOOK AND FEEL
// =============================================================

HumanizerLookAndFeel::HumanizerLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, ui::bg);

    setColour(juce::ComboBox::textColourId, ui::text);
    setColour(juce::ComboBox::backgroundColourId, ui::panelHigh);
    setColour(juce::ComboBox::outlineColourId, ui::border);
    setColour(juce::ComboBox::arrowColourId, ui::muted);

    setColour(juce::PopupMenu::backgroundColourId, ui::panelHigh);
    setColour(juce::PopupMenu::textColourId, ui::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, ui::accent.withAlpha(0.22f));
    setColour(juce::PopupMenu::highlightedTextColourId, ui::text);

}

void HumanizerLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                                            float pos, float startAngle, float endAngle, juce::Slider &s)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height))
                            .reduced(2.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();

    const float arcWidth = 5.0f;
    const float arcRadius = radius - arcWidth * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Spur
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(ui::border);
    g.strokePath(track, juce::PathStrokeType(arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Wert
    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour(ui::accent);
        g.strokePath(value, juce::PathStrokeType(arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Koerper
    const float bodyRadius = radius - arcWidth - 5.0f;
    g.setColour(ui::panelHigh);
    g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour(ui::border);
    g.drawEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

    // Griff-Punkt am Bogenende
    const float dotX = centre.x + arcRadius * std::sin(angle);
    const float dotY = centre.y - arcRadius * std::cos(angle);
    g.setColour(ui::text);
    g.fillEllipse(dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);

    // Wert als Text in der Mitte
    g.setColour(ui::text);
    g.setFont(13.0f);
    g.drawText(s.getTextFromValue(s.getValue()),
               juce::Rectangle<float>(centre.x - bodyRadius, centre.y - 9.0f, bodyRadius * 2.0f, 18.0f),
               juce::Justification::centred, false);
}

void HumanizerLookAndFeel::drawComboBox(juce::Graphics &g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox &box)
{
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);

    g.setColour(ui::panelHigh);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(box.hasKeyboardFocus(true) ? ui::accent : ui::border);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    const float cx = static_cast<float>(width) - 18.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath(cx - 4.0f, cy - 2.0f);
    chevron.lineTo(cx, cy + 2.0f);
    chevron.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(ui::muted);
    g.strokePath(chevron, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void HumanizerLookAndFeel::drawToggleButton(juce::Graphics &g, juce::ToggleButton &b, bool highlighted, bool)
{
    const bool bypassed = b.getToggleState();
    const auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const auto col = bypassed ? ui::amber : ui::accent;

    g.setColour(col.withAlpha(highlighted ? 0.26f : 0.16f));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(col);
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.2f);

    g.fillEllipse(r.getX() + 12.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);

    g.setColour(ui::text);
    g.setFont(12.0f);
    g.drawText(bypassed ? "BYPASSED" : "ACTIVE",
               r.withTrimmedLeft(28.0f).toNearestInt(), juce::Justification::centredLeft, false);
}

// =============================================================
// CHORD VIEW
// =============================================================

void ChordView::setReport(const hz::GroupReport &r)
{
    report = r;
    hasReport = true;
    repaint();
}

void ChordView::paint(juce::Graphics &g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour(ui::panel);
    g.fillRoundedRectangle(b, 14.0f);
    g.setColour(ui::border);
    g.drawRoundedRectangle(b.reduced(0.5f), 14.0f, 1.0f);

    auto area = getLocalBounds().reduced(16, 14);
    auto head = area.removeFromTop(18);

    g.setFont(11.0f);
    g.setColour(ui::muted);
    g.drawText("LAST CHORD", head, juce::Justification::centredLeft, false);

    if (!hasReport || report.count < 1)
    {
        g.setColour(ui::muted);
        g.setFont(13.0f);
        g.drawFittedText("Play or click in a chord\nto see how it gets strummed.",
                         area, juce::Justification::centred, 3);
        return;
    }

    // Richtung (oben rechts): Pfeil + Text
    {
        const juce::String dirText = report.ascending ? "LOW to HIGH" : "HIGH to LOW";
        auto badge = head.removeFromRight(86);
        g.setColour(ui::accent);
        g.setFont(11.0f);
        g.drawText(dirText, badge, juce::Justification::centredRight, false);

        const float ax = static_cast<float>(badge.getX()) - 10.0f;
        const float ay = static_cast<float>(head.getCentreY());
        juce::Path arrow;
        if (report.ascending)
            arrow.addArrow(juce::Line<float>(ax, ay + 7.0f, ax, ay - 7.0f), 2.0f, 8.0f, 6.0f);
        else
            arrow.addArrow(juce::Line<float>(ax, ay - 7.0f, ax, ay + 7.0f), 2.0f, 8.0f, 6.0f);
        g.fillPath(arrow);
    }

    area.removeFromTop(10);
    auto axis = area.removeFromBottom(18);

    const int n = juce::jlimit(1, hz::kMaxGroup, report.count);
    const float laneH = juce::jmin(30.0f, static_cast<float>(area.getHeight()) / static_cast<float>(n));
    const float totalH = laneH * static_cast<float>(n);
    const float y0 = static_cast<float>(area.getY()) + (static_cast<float>(area.getHeight()) - totalH) * 0.5f;

    const int labelW = 34;
    const int valueW = 66;
    const float plotX = static_cast<float>(area.getX() + labelW);
    const float plotW = static_cast<float>(area.getWidth() - labelW - valueW);
    const float markW = 12.0f;

    float maxOffset = 0.0f;
    for (int k = 0; k < n; ++k)
        maxOffset = juce::jmax(maxOffset, report.offsetMs[static_cast<size_t>(k)]);
    const float span = juce::jmax(30.0f, std::ceil(maxOffset / 10.0f) * 10.0f);

    // Raster (alle 10 ms) + Achsenbeschriftung
    g.setFont(10.0f);
    for (float t = 0.0f; t <= span + 0.01f; t += 10.0f)
    {
        const float x = plotX + markW * 0.5f + (t / span) * (plotW - markW);
        g.setColour(ui::border);
        g.drawVerticalLine(juce::roundToInt(x), static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
        g.setColour(ui::muted);
        g.drawText(juce::String(juce::roundToInt(t)),
                   juce::Rectangle<float>(x - 14.0f, static_cast<float>(axis.getY()), 28.0f, static_cast<float>(axis.getHeight())).toNearestInt(),
                   juce::Justification::centred, false);
    }
    g.setColour(ui::muted);
    g.drawText("ms", axis.removeFromRight(valueW), juce::Justification::centredRight, false);

    // Noten: oben = hoechste Note
    for (int k = 0; k < n; ++k)
    {
        const size_t ks = static_cast<size_t>(k);
        const int row = n - 1 - k;
        const float y = y0 + static_cast<float>(row) * laneH;
        const float cy = y + laneH * 0.5f;

        g.setColour(ui::muted);
        g.setFont(11.0f);
        g.drawText(juce::MidiMessage::getMidiNoteName(report.note[ks], true, true, 4),
                   juce::Rectangle<float>(static_cast<float>(area.getX()), y, static_cast<float>(labelW), laneH).toNearestInt(),
                   juce::Justification::centredLeft, false);

        const float x = plotX + (report.offsetMs[ks] / span) * (plotW - markW);

        g.setColour(ui::muted.withAlpha(0.35f));
        g.drawLine(plotX + markW * 0.5f, cy, x + markW * 0.5f, cy, 2.0f);

        const float v = static_cast<float>(juce::jlimit(0, 127, static_cast<int>(report.velOut[ks]))) / 127.0f;
        g.setColour(ui::accent.withAlpha(0.35f + 0.65f * v));
        g.fillRoundedRectangle(x, y + 4.0f, markW, laneH - 8.0f, 3.0f);

        g.setColour(ui::text);
        g.setFont(11.0f);
        g.drawText(juce::String(static_cast<int>(report.velIn[ks])) + " -> " + juce::String(static_cast<int>(report.velOut[ks])),
                   juce::Rectangle<float>(plotX + plotW, y, static_cast<float>(valueW), laneH).toNearestInt(),
                   juce::Justification::centredRight, false);
    }
}

// =============================================================
// EDITOR
// =============================================================

HumanizerAudioProcessorEditor::HumanizerAudioProcessorEditor(HumanizerAudioProcessor &p)
    : juce::AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&laf);

    struct KnobInfo
    {
        const char *id;
        const char *name;
        const char *tip;
    };

    static const KnobInfo infos[11] = {
        {HumanizerParams::amount, "HUMANIZE",
         "Master amount: scales all the human feel at once. Start here, open Advanced only if you want to fine-tune."},
        {HumanizerParams::soft, "SOFT",
         "For gentle playing, e.g. quiet piano melodies: pulls velocities down and squeezes them together. 0 % = off."},
        {HumanizerParams::bass, "BASS",
         "Keeps the bass from overpowering the mix: notes below E3 get softer the deeper they go (up to -35 % at C2). 0 % = off."},
        {HumanizerParams::strum, "STRUM",
         "How wide chords are spread in time. 100 % = like the real players in the data (up to ~29 ms). Higher = lazier, guitar-like strums."},
        {HumanizerParams::velocity, "VELOCITY",
         "How strongly the learned velocity shapes are applied: which notes of a chord are louder or softer. The top note usually sings out."},
        {HumanizerParams::timing, "TIMING",
         "Natural looseness. Lateness drifts in slow waves instead of random jumps, soft notes come a bit later, and the melody (top note) leads the chord."},
        {HumanizerParams::groove, "GROOVE",
         "Laid-back feel: off-beat notes (8ths, 16ths) land slightly late, the beat itself stays put. Needs the DAW transport to be playing."},
        {HumanizerParams::accent, "ACCENT",
         "Louder on the downbeat, softer on weak beats, like a player feeling the bar. Needs the DAW transport to be playing."},
        {HumanizerParams::contour, "CONTOUR",
         "Velocity follows the melody: rising lines get a little louder, falling lines softer."},
        {HumanizerParams::length, "LENGTH",
         "Varies how long notes are held, with a little legato overlap. Perfectly rigid note lengths are a giveaway for programmed MIDI."},
        {HumanizerParams::window, "WINDOW",
         "Notes closer together than this count as one chord. Raise it if clicked-in chords are not detected, lower it for fast runs."}};

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto &k = knobs[i];
        k.name = infos[i].name;

        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(k.slider);
        addHint(k.slider, infos[i].name, infos[i].tip);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.apvts, infos[i].id, k.slider);
    }

    if (auto *dirParam = audioProcessor.apvts.getParameter(HumanizerParams::direction))
        directionBox.addItemList(dirParam->getAllValueStrings(), 1);
    addAndMakeVisible(directionBox);
    addHint(directionBox, "STRUM DIRECTION",
            "Auto follows how the chord was played. Or force Low to High (down-strum), High to Low (up-strum) or alternate every chord.");
    directionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, HumanizerParams::direction, directionBox);

    addAndMakeVisible(bypassButton);
    addHint(bypassButton, "BYPASS", "Passes MIDI through untouched. Great for A/B comparing before and after.");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, HumanizerParams::bypass, bypassButton);

    addAndMakeVisible(chordView);
    addHint(chordView, "LAST CHORD",
            "Each bar is one note of the last chord: further right = later. Brighter = louder. Right side shows velocity in -> out.");

    advancedButton.setClickingTogglesState(true);
    advancedButton.setColour(juce::TextButton::buttonColourId, ui::panelHigh);
    advancedButton.setColour(juce::TextButton::buttonOnColourId, ui::accent.withAlpha(0.28f));
    advancedButton.setColour(juce::TextButton::textColourOffId, ui::muted);
    advancedButton.setColour(juce::TextButton::textColourOnId, ui::text);
    advancedButton.onClick = [this]
    {
        advanced = advancedButton.getToggleState();
        layoutKnobs();
        repaint();
    };
    addAndMakeVisible(advancedButton);
    addHint(advancedButton, "ADVANCED", "Show or hide the individual controls (strum, timing, groove, length ...). The three main knobs are usually enough.");

    presetBox.addItemList({"Tight", "Natural", "Loose", "Sloppy"}, 1);
    presetBox.setTextWhenNothingSelected("Preset");
    presetBox.onChange = [this]
    { applyPreset(presetBox.getSelectedId() - 1); };
    addAndMakeVisible(presetBox);
    addHint(presetBox, "PRESET",
            "Quick starting points: Tight = subtle, Natural = recommended, Loose = relaxed band feel, Sloppy = drunk drummer. Fine-tune with the knobs afterwards.");

    hintTitle = "";
    hintText = "Hover over any control to see what it does.";

    setSize(800, 620);
    startTimerHz(15);
    updateBypassLook();
}

HumanizerAudioProcessorEditor::~HumanizerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void HumanizerAudioProcessorEditor::addHint(juce::Component &comp, const juce::String &title, const juce::String &text)
{
    hints.push_back({&comp, title, text});
    comp.addMouseListener(this, true);
}

void HumanizerAudioProcessorEditor::mouseEnter(const juce::MouseEvent &e)
{
    for (auto *c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
        for (auto &h : hints)
            if (h.comp == c)
            {
                hintTitle = h.title;
                hintText = h.text;
                repaint(infoBounds);
                return;
            }
}

void HumanizerAudioProcessorEditor::mouseExit(const juce::MouseEvent &)
{
    // Maus auf einer anderen Hinweis-Flaeche? Dann setzt deren mouseEnter den Text.
    const auto p = getMouseXYRelative();
    for (auto *c = getComponentAt(p); c != nullptr && c != this; c = c->getParentComponent())
        for (auto &h : hints)
            if (h.comp == c)
                return;

    hintTitle = "";
    hintText = "Hover over any control to see what it does.";
    repaint(infoBounds);
}

void HumanizerAudioProcessorEditor::applyPreset(int index)
{
    struct P
    {
        float strum, vel, timing, groove, accent, contour, length;
    };
    static const P presets[4] = {
        {0.6f, 0.7f, 0.12f, 0.10f, 0.25f, 0.20f, 0.15f}, // Tight
        {1.0f, 1.0f, 0.35f, 0.25f, 0.40f, 0.30f, 0.30f}, // Natural
        {1.4f, 1.2f, 0.60f, 0.45f, 0.55f, 0.40f, 0.50f}, // Loose
        {2.0f, 1.5f, 1.00f, 0.80f, 0.70f, 0.50f, 0.80f}  // Sloppy
    };
    if (index < 0 || index > 3)
        return;

    const auto &p = presets[index];
    const auto set = [this](const char *id, float v)
    {
        if (auto *par = audioProcessor.apvts.getParameter(id))
            par->setValueNotifyingHost(par->convertTo0to1(v));
    };
    set(HumanizerParams::amount, 1.0f);
    set(HumanizerParams::strum, p.strum);
    set(HumanizerParams::velocity, p.vel);
    set(HumanizerParams::timing, p.timing);
    set(HumanizerParams::groove, p.groove);
    set(HumanizerParams::accent, p.accent);
    set(HumanizerParams::contour, p.contour);
    set(HumanizerParams::length, p.length);
}

void HumanizerAudioProcessorEditor::setMonitor(const HumanizerMonitor &m)
{
    monitor = m;
    repaint(monitorBounds);
}

void HumanizerAudioProcessorEditor::showGroup(const hz::GroupReport &r)
{
    chordView.setReport(r);
}

void HumanizerAudioProcessorEditor::timerCallback()
{
    // Bypass kann auch per Automation / vom Host geaendert werden
    const bool bp = audioProcessor.apvts.getRawParameterValue(HumanizerParams::bypass)->load() > 0.5f;
    if (bp != lastBypass)
        updateBypassLook();
}

void HumanizerAudioProcessorEditor::updateBypassLook()
{
    lastBypass = audioProcessor.apvts.getRawParameterValue(HumanizerParams::bypass)->load() > 0.5f;
    const float a = lastBypass ? 0.4f : 1.0f;

    for (auto &k : knobs)
        k.slider.setAlpha(a);
    presetBox.setAlpha(a);
    directionBox.setAlpha(a);
    chordView.setAlpha(a);
    repaint();
}

void HumanizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);

    headerBounds = area.removeFromTop(56);
    bypassButton.setBounds(headerBounds.removeFromRight(130).withSizeKeepingCentre(130, 32));
    headerBounds.removeFromRight(12);
    presetBox.setBounds(headerBounds.removeFromRight(130).withSizeKeepingCentre(130, 32));
    area.removeFromTop(10);

    monitorBounds = area.removeFromBottom(84);
    area.removeFromBottom(10);
    infoBounds = area.removeFromBottom(52);
    area.removeFromBottom(14);

    leftPanelBounds = area.removeFromLeft(470);
    area.removeFromLeft(14);
    chordView.setBounds(area);

    innerBounds = leftPanelBounds.reduced(14);
    auto inner = innerBounds;
    auto dirRow = inner.removeFromBottom(46);
    advancedButton.setBounds(dirRow.removeFromRight(100).withSizeKeepingCentre(100, 30));
    dirRow.removeFromRight(8);
    dirLabelBounds = dirRow.removeFromLeft(112);
    directionBox.setBounds(dirRow.withSizeKeepingCentre(dirRow.getWidth(), 30));
    inner.removeFromBottom(6);
    knobArea = inner;
    layoutKnobs();
}

void HumanizerAudioProcessorEditor::layoutKnobs()
{
    const int count = advanced ? 11 : 3;
    const int cols = advanced ? 6 : 3;
    const int rows = advanced ? 2 : 1;
    const int cw = knobArea.getWidth() / cols;
    const int ch = knobArea.getHeight() / rows;

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const bool shown = static_cast<int>(i) < count;
        knobs[i].slider.setVisible(shown);
        if (!shown)
            continue;
        const int col = static_cast<int>(i) % cols;
        const int row = static_cast<int>(i) / cols;
        juce::Rectangle<int> cell(knobArea.getX() + col * cw, knobArea.getY() + row * ch, cw, ch);
        knobs[i].labelBounds = cell.removeFromBottom(20);
        knobs[i].slider.setBounds(cell.reduced(advanced ? 3 : 10));
    }
}

void HumanizerAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(ui::bg);

    // ---- Kopfzeile ----
    {
        auto h = headerBounds;
        g.setColour(ui::accent);
        g.fillRoundedRectangle(static_cast<float>(h.getX()), static_cast<float>(h.getCentreY()) - 18.0f, 4.0f, 36.0f, 2.0f);

        g.setColour(ui::text);
        g.setFont(24.0f);
        g.drawText("HUMANIZER", h.withTrimmedLeft(16).withTrimmedBottom(h.getHeight() / 2 - 2),
                   juce::Justification::bottomLeft, false);

        g.setColour(ui::muted);
        g.setFont(12.0f);
        g.drawText("natural feel for programmed MIDI", h.withTrimmedLeft(16).withTrimmedTop(h.getHeight() / 2 + 2),
                   juce::Justification::topLeft, false);
    }

    // ---- linkes Panel ----
    g.setColour(ui::panel);
    g.fillRoundedRectangle(leftPanelBounds.toFloat(), 14.0f);
    g.setColour(ui::border);
    g.drawRoundedRectangle(leftPanelBounds.toFloat().reduced(0.5f), 14.0f, 1.0f);

    g.setFont(11.0f);
    g.setColour(ui::muted);
    for (size_t i = 0; i < (advanced ? knobs.size() : 3u); ++i)
        g.drawText(knobs[i].name, knobs[i].labelBounds, juce::Justification::centred, false);
    g.drawText("STRUM DIRECTION", dirLabelBounds, juce::Justification::centredLeft, false);

    // ---- Info-Leiste (Hover-Hinweise) ----
    {
        g.setColour(ui::panel);
        g.fillRoundedRectangle(infoBounds.toFloat(), 12.0f);
        g.setColour(hintTitle.isEmpty() ? ui::border : ui::accent.withAlpha(0.5f));
        g.drawRoundedRectangle(infoBounds.toFloat().reduced(0.5f), 12.0f, 1.0f);

        auto r = infoBounds.reduced(16, 8);
        if (hintTitle.isNotEmpty())
        {
            g.setColour(ui::accent);
            g.setFont(12.0f);
            g.drawText(hintTitle, r.removeFromTop(14), juce::Justification::centredLeft, false);
        }
        g.setColour(hintTitle.isEmpty() ? ui::muted : ui::text);
        g.setFont(12.0f);
        g.drawFittedText(hintText, r, juce::Justification::centredLeft, 2);
    }

    // ---- Monitor ----
    g.setColour(ui::panel);
    g.fillRoundedRectangle(monitorBounds.toFloat(), 14.0f);
    g.setColour(ui::border);
    g.drawRoundedRectangle(monitorBounds.toFloat().reduced(0.5f), 14.0f, 1.0f);

    auto r = monitorBounds.reduced(16, 10);
    auto top = r.removeFromTop(44);

    const juce::String bpmText = monitor.bpm > 0.0 ? juce::String(monitor.bpm, 1) : juce::String("-");
    const auto orDash = [](const juce::String &s)
    { return s.isEmpty() ? juce::String("-") : s; };

    const char *labels[6] = {"NOTE", "VELOCITY", "INTERVAL", "TIME DIFF (ms)", "VEL DIFF", "BPM"};
    const juce::String values[6] = {orDash(monitor.lastNote), orDash(monitor.lastVelocity), orDash(monitor.interval),
                                    orDash(monitor.deltaMs), orDash(monitor.deltaVelocity), bpmText};

    const int cellW = top.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto cell = juce::Rectangle<int>(top.getX() + i * cellW, top.getY(), cellW, top.getHeight());
        g.setColour(ui::muted);
        g.setFont(10.0f);
        g.drawText(labels[i], cell.removeFromTop(16), juce::Justification::centredLeft, false);
        g.setColour(ui::text);
        g.setFont(16.0f);
        g.drawText(values[i], cell, juce::Justification::centredLeft, false);
    }

    g.setColour(ui::muted);
    g.setFont(10.0f);
    g.drawText("RECENT", r.removeFromLeft(54), juce::Justification::centredLeft, false);
    g.setColour(ui::text);
    g.setFont(12.0f);
    g.drawText(orDash(monitor.recent), r, juce::Justification::centredLeft, true);
}
