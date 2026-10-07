#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================
// PARAMETER
// =============================================================

juce::AudioProcessorValueTreeState::ParameterLayout
HumanizerAudioProcessor::createParameterLayout()
{
    using namespace juce;

    const auto percent = [](float v, int)
    { return String(roundToInt(v * 100.0f)) + " %"; };
    const auto fromPercent = [](const String &t)
    { return t.getFloatValue() / 100.0f; };
    const auto millis = [](float v, int)
    { return String(roundToInt(v)) + " ms"; };
    const auto fromMillis = [](const String &t)
    { return t.getFloatValue(); };

    const auto pctAttr = [&]()
    {
        return AudioParameterFloatAttributes()
            .withStringFromValueFunction(percent)
            .withValueFromStringFunction(fromPercent);
    };

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::amount, 1}, "Humanize",
        NormalisableRange<float>(0.0f, 1.5f, 0.01f), 1.0f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::soft, 1}, "Soft",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::bass, 1}, "Bass",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::strum, 1}, "Strum",
        NormalisableRange<float>(0.0f, 3.0f, 0.01f), 1.0f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::velocity, 1}, "Velocity",
        NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::timing, 1}, "Timing",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::groove, 1}, "Groove",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::accent, 1}, "Accent",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.4f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::contour, 1}, "Contour",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::length, 1}, "Length",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f, pctAttr()));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{HumanizerParams::window, 1}, "Chord Window",
        NormalisableRange<float>(5.0f, 60.0f, 1.0f), 25.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction(millis)
            .withValueFromStringFunction(fromMillis)));

    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID{HumanizerParams::direction, 1}, "Direction",
        StringArray{"Auto", "Low to High", "High to Low", "Alternate"}, 0));

    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID{HumanizerParams::bypass, 1}, "Bypass", false));

    return layout;
}

// =============================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================

HumanizerAudioProcessor::HumanizerAudioProcessor()
    : apvts(*this, nullptr, "HUMANIZER", createParameterLayout())
{
    pAmount = apvts.getRawParameterValue(HumanizerParams::amount);
    pSoft = apvts.getRawParameterValue(HumanizerParams::soft);
    pBass = apvts.getRawParameterValue(HumanizerParams::bass);
    pStrum = apvts.getRawParameterValue(HumanizerParams::strum);
    pVelocity = apvts.getRawParameterValue(HumanizerParams::velocity);
    pTiming = apvts.getRawParameterValue(HumanizerParams::timing);
    pAccent = apvts.getRawParameterValue(HumanizerParams::accent);
    pContour = apvts.getRawParameterValue(HumanizerParams::contour);
    pGroove = apvts.getRawParameterValue(HumanizerParams::groove);
    pLength = apvts.getRawParameterValue(HumanizerParams::length);
    pWindow = apvts.getRawParameterValue(HumanizerParams::window);
    pDirection = apvts.getRawParameterValue(HumanizerParams::direction);
    pBypass = apvts.getRawParameterValue(HumanizerParams::bypass);

    inBuf.resize(hz::kMaxIn);
    passthrough.reserve(1024);
    recentNotes.reserve(16);

    startTimerHz(30);
}

HumanizerAudioProcessor::~HumanizerAudioProcessor()
{
    stopTimer();
}

// =============================================================
// BASIC INFO
// =============================================================

const juce::String HumanizerAudioProcessor::getName() const { return "Humanizer"; }
bool HumanizerAudioProcessor::acceptsMidi() const { return true; }
bool HumanizerAudioProcessor::producesMidi() const { return true; }
bool HumanizerAudioProcessor::isMidiEffect() const { return true; }
double HumanizerAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int HumanizerAudioProcessor::getNumPrograms() { return 1; }
int HumanizerAudioProcessor::getCurrentProgram() { return 0; }
void HumanizerAudioProcessor::setCurrentProgram(int) {}
const juce::String HumanizerAudioProcessor::getProgramName(int) { return {}; }
void HumanizerAudioProcessor::changeProgramName(int, const juce::String &) {}

bool HumanizerAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *HumanizerAudioProcessor::createEditor()
{
    return new HumanizerAudioProcessorEditor(*this);
}

bool HumanizerAudioProcessor::isBusesLayoutSupported(const BusesLayout &) const
{
    return true;
}

// =============================================================
// PREPARE / RELEASE
// =============================================================

void HumanizerAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    // setzt den Kern zurueck; noch klingende Noten bekommen im ersten
    // Block ein Note-Off, damit nichts haengen bleibt
    core.prepare(sampleRate);
}

void HumanizerAudioProcessor::releaseResources()
{
    core.requestPanic();
}

// =============================================================
// PROCESS BLOCK
// =============================================================

void HumanizerAudioProcessor::processBlock(
    juce::AudioBuffer<float> &buffer,
    juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // ---- Parameter an den Kern geben -------------------------
    const float m = pAmount->load(); // Master skaliert alle Humanize-Regler
    core.setStrumIntensity(pStrum->load() * m);
    core.setVelocityAmount(pVelocity->load() * m);
    core.setTimingAmount(pTiming->load() * m);
    core.setAccentAmount(pAccent->load() * m);
    core.setContourAmount(pContour->load() * m);
    core.setGrooveAmount(pGroove->load() * m);
    core.setLengthAmount(pLength->load() * m);
    core.setSoftness(pSoft->load());
    core.setBassCare(pBass->load());
    core.setGroupWindowMs(pWindow->load());
    core.setDirectionMode(static_cast<int>(pDirection->load() + 0.5f));
    core.setBypass(pBypass->load() > 0.5f);

    // ---- Tempo, Taktposition und Taktart von der DAW ---------
    hz::BlockInfo info;
    double bpmForUi = 0.0;

    if (auto *playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            const auto bpm = position->getBpm();
            const bool bpmOk = bpm.hasValue() && *bpm > 0.0;

            if (bpmOk)
            {
                info.bpm = *bpm;
                bpmForUi = *bpm;
            }

            // Akzente nur, wenn die Transportleiste laeuft und eine Taktposition vorliegt
            const auto ppq = position->getPpqPosition();
            if (bpmOk && ppq.hasValue() && position->getIsPlaying())
            {
                info.ppqValid = true;
                info.ppqStart = *ppq;

                const auto barStart = position->getPpqPositionOfLastBarStart();
                info.barStartPpq = barStart.hasValue() ? *barStart : 0.0;

                const auto ts = position->getTimeSignature();
                if (ts.hasValue())
                {
                    info.tsNum = juce::jmax(1, ts->numerator);
                    info.tsDen = juce::jmax(1, ts->denominator);
                }
            }
        }
    }

    hostBpm.store(bpmForUi);

    // ---- MIDI einlesen ---------------------------------------
    int nIn = 0;
    passthrough.clear();

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int pos = metadata.samplePosition;

        const bool isOn = message.isNoteOn();            // Velocity 0 zaehlt hier NICHT als On
        const bool isOff = !isOn && message.isNoteOff(); // inkl. NoteOn mit Velocity 0

        if ((isOn || isOff) && nIn < hz::kMaxIn)
        {
            hz::NoteEvent e;
            e.pos = pos;
            e.on = isOn;
            e.ch = static_cast<uint8_t>(juce::jlimit(1, 16, message.getChannel()) - 1);
            e.note = static_cast<uint8_t>(message.getNoteNumber() & 127);
            e.vel = static_cast<uint8_t>(message.getVelocity() & 127);
            inBuf[static_cast<size_t>(nIn++)] = e;
        }
        else
        {
            // CC, Pitchbend, Sysex ... laufen unveraendert durch.
            // Auch der seltene Fall von >2048 Noten-Events pro Block landet hier.
            if (message.isAllNotesOff() || message.isAllSoundOff())
                core.requestPanic();

            passthrough.emplace_back(message, pos);
        }
    }

    // ---- Humanizen -------------------------------------------
    const int nOut = core.process(inBuf.data(), nIn, buffer.getNumSamples(), info);

    // ---- MIDI ausgeben ---------------------------------------
    midi.clear();

    for (const auto &p : passthrough)
        midi.addEvent(p.first, p.second);

    const hz::NoteEvent *out = core.output();
    for (int i = 0; i < nOut; ++i)
    {
        const auto &e = out[i];
        const int channel = static_cast<int>(e.ch) + 1;

        if (e.on)
            midi.addEvent(juce::MidiMessage::noteOn(channel, static_cast<int>(e.note), static_cast<juce::uint8>(e.vel)), e.pos);
        else
            midi.addEvent(juce::MidiMessage::noteOff(channel, static_cast<int>(e.note), static_cast<juce::uint8>(e.vel)), e.pos);
    }
}

// =============================================================
// ANZEIGE (Message-Thread, 30 Hz)
// =============================================================

void HumanizerAudioProcessor::timerCallback()
{
    auto *editor = dynamic_cast<HumanizerAudioProcessorEditor *>(getActiveEditor());

    // ---- neue Noten ------------------------------------------
    hz::NoteInfo info;
    hz::NoteInfo prev;
    hz::NoteInfo last;
    bool anyNote = false;
    bool havePrev = false;

    while (core.popNoteInfo(info))
    {
        havePrev = !recentNotes.empty();
        if (havePrev)
            prev = recentNotes.back();

        recentNotes.push_back(info);
        if (recentNotes.size() > 8)
            recentNotes.erase(recentNotes.begin());

        last = info;
        anyNote = true;
    }

    bool changed = false;

    if (anyNote)
    {
        monitor.lastNote = juce::MidiMessage::getMidiNoteName(last.note, true, true, 4);
        monitor.lastVelocity = juce::String(static_cast<int>(last.vel));

        if (havePrev)
        {
            monitor.interval = juce::String(static_cast<int>(last.note) - static_cast<int>(prev.note));

            const double sr = getSampleRate();
            const double deviationMs =
                sr > 0.0 ? static_cast<double>(last.absSample - prev.absSample) / sr * 1000.0 : 0.0;
            monitor.deltaMs = juce::String(deviationMs, 1);

            monitor.deltaVelocity = juce::String(static_cast<int>(last.vel) - static_cast<int>(prev.vel));
        }

        juce::String recentText;
        for (size_t i = 0; i < recentNotes.size(); ++i)
        {
            recentText += juce::MidiMessage::getMidiNoteName(recentNotes[i].note, true, true, 4);
            if (i + 1 < recentNotes.size())
                recentText += "  ";
        }
        monitor.recent = recentText;
        changed = true;
    }

    const double bpm = hostBpm.load();
    if (std::abs(bpm - shownBpm) > 0.05)
    {
        shownBpm = bpm;
        monitor.bpm = bpm;
        changed = true;
    }

    if (changed && editor != nullptr)
        editor->setMonitor(monitor);

    // ---- humanisierte Gruppen --------------------------------
    hz::GroupReport rep;
    bool anyGroup = false;

    while (core.popGroupReport(rep))
        anyGroup = true; // nur die letzte zeigen

    if (anyGroup && editor != nullptr)
        editor->showGroup(rep);
}

// =============================================================
// STATE (alle Parameter werden mit dem Projekt gespeichert)
// =============================================================

void HumanizerAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void HumanizerAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// =============================================================
// PLUGIN CREATOR
// =============================================================

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new HumanizerAudioProcessor();
}
