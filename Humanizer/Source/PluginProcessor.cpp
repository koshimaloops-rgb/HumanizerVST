#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "strum_patterns.h"

// =============================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================

HumanizerAudioProcessor::HumanizerAudioProcessor()
{
}

HumanizerAudioProcessor::~HumanizerAudioProcessor()
{
}

// =============================================================
// BASIC INFO
// =============================================================

const juce::String HumanizerAudioProcessor::getName() const
{
    return "Humanizer";
}

bool HumanizerAudioProcessor::acceptsMidi() const
{
    return true;
}

bool HumanizerAudioProcessor::producesMidi() const
{
    return true;
}

bool HumanizerAudioProcessor::isMidiEffect() const
{
    return true;
}

double HumanizerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int HumanizerAudioProcessor::getNumPrograms()
{
    return 1;
}

int HumanizerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void HumanizerAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String HumanizerAudioProcessor::getProgramName(int index)
{
    return {};
}

void HumanizerAudioProcessor::changeProgramName(
    int index,
    const juce::String &newName)
{
}

// =============================================================
// PREPARE
// =============================================================

void HumanizerAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock)
{
    totalSamples = 0;

    groupStartSample = -1;

    currentGroupId = 0;

    currentHumanizedGroupId = -1;

    currentGroupTimingOffsetMs = 0.0f;

    currentNoteTimingOffsetMs = 0.0f;

    currentGroupEvents.clear();

    pendingMidiEvents.clear();

    heldNotes.clear();

    recentNotes.clear();
}

// =============================================================
// HUMANIZE SETTINGS
// =============================================================

void HumanizerAudioProcessor::setVelocityHumanize(float amount)
{
    velocityHumanizeAmount = amount;
}

void HumanizerAudioProcessor::setTimingHumanize(float amount)
{
    timingHumanizeAmount =
        juce::jlimit(0.0f, 1.0f, amount);
}

void HumanizerAudioProcessor::setStrumIntensity(float amount)
{
    strumIntensity =
        juce::jlimit(0.0f, 1.0f, amount);
}

// =============================================================
// RELEASE
// =============================================================

void HumanizerAudioProcessor::releaseResources()
{
}

// =============================================================
// BUS LAYOUT
// =============================================================

bool HumanizerAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const
{
    return true;
}

// =============================================================
// APPLY LEARNED STRUM
// =============================================================

void HumanizerAudioProcessor::applyLearnedStrumToGroup()
{
    if (currentGroupEvents.empty())
        return;

    // =====================================================
    // SINGLE NOTE -> PASS THROUGH
    // =====================================================

    if (currentGroupEvents.size() < 2)
    {
        for (const auto &event : currentGroupEvents)
        {
            pendingMidiEvents.push_back(
                {event.message,
                 event.originalSample});
        }

        // Eventuelle zurückgehaltene Note-Offs
        for (const auto &noteOff : pendingGroupNoteOffs)
        {
            pendingMidiEvents.push_back(
                {noteOff.message,
                 noteOff.originalSample});
        }

        pendingGroupNoteOffs.clear();
        currentGroupEvents.clear();

        return;
    }

    // =====================================================
    // NOTE COUNT
    // =====================================================

    const int noteCount =
        static_cast<int>(currentGroupEvents.size());

    // Unser Modell unterstützt 2-5 Noten
    if (noteCount > 5)
    {
        for (const auto &event : currentGroupEvents)
        {
            pendingMidiEvents.push_back(
                {event.message,
                 event.originalSample});
        }

        for (const auto &noteOff : pendingGroupNoteOffs)
        {
            pendingMidiEvents.push_back(
                {noteOff.message,
                 noteOff.originalSample});
        }

        pendingGroupNoteOffs.clear();
        currentGroupEvents.clear();

        return;
    }

    // =====================================================
    // SORT BY PITCH
    // =====================================================

    std::vector<PendingGroupNote> sortedByPitch =
        currentGroupEvents;

    std::sort(
        sortedByPitch.begin(),
        sortedByPitch.end(),
        [](const PendingGroupNote &a,
           const PendingGroupNote &b)
        {
            return a.message.getNoteNumber() < b.message.getNoteNumber();
        });

    // =====================================================
    // DETERMINE ORIGINAL DIRECTION
    // =====================================================

    bool ascending = true;

    int64_t earliestSample =
        sortedByPitch.front().originalSample;

    int earliestIndex = 0;

    for (int i = 1;
         i < noteCount;
         ++i)
    {
        if (sortedByPitch[i].originalSample < earliestSample)
        {
            earliestSample =
                sortedByPitch[i].originalSample;

            earliestIndex = i;
        }
    }

    // Niedrigste Note zuerst = ascending
    // Höchste Note zuerst = descending

    if (earliestIndex == noteCount - 1)
        ascending = false;

    // =====================================================
    // FIND MATCHING LEARNED PATTERNS
    // =====================================================

    auto &random =
        juce::Random::getSystemRandom();

    std::vector<int> matchingPatterns;

    for (int i = 0;
         i < learnedStrumPatternCount;
         ++i)
    {
        const auto &pattern =
            learnedStrumPatterns[i];

        if (pattern.noteCount != noteCount)
            continue;

        // Für den Test ignorieren wir die Richtung.
        // Wir wollen erstmal sicherstellen,
        // dass das gelernte Timing überhaupt angewendet wird.
        matchingPatterns.push_back(i);
    }

    // =====================================================
    // NO MATCH -> PASS THROUGH
    // =====================================================

    if (matchingPatterns.empty())
    {
        for (const auto &event : currentGroupEvents)
        {
            pendingMidiEvents.push_back(
                {event.message,
                 event.originalSample});
        }

        for (const auto &noteOff : pendingGroupNoteOffs)
        {
            pendingMidiEvents.push_back(
                {noteOff.message,
                 noteOff.originalSample});
        }

        pendingGroupNoteOffs.clear();
        currentGroupEvents.clear();

        return;
    }

    // =====================================================
    // SELECT LEARNED PATTERN
    // =====================================================

    const int selectedPatternIndex =
        matchingPatterns[random.nextInt(
            static_cast<int>(
                matchingPatterns.size()))];

    const auto &selectedPattern =
        learnedStrumPatterns[selectedPatternIndex];

    // =====================================================
    // STRUM INTENSITY
    // =====================================================

    const float intensity =
        juce::jlimit(
            0.0f,
            1.0f,
            strumIntensity);

    // =====================================================
    // APPLY STRUM
    // =====================================================
    // =====================================================
    // APPLY STRUM
    // =====================================================

    for (int pitchIndex = 0;
         pitchIndex < noteCount;
         ++pitchIndex)
    {
        const auto &event =
            sortedByPitch[pitchIndex];

        const float learnedBeatPosition =
            selectedPattern.positions[pitchIndex];

        // Kleine Variation der gesamten Strum-Breite
        const float strumVariation =
            0.85f +
            random.nextFloat() * 0.30f;

        // Kleine individuelle menschliche Abweichung
        const float noteVariationMs =
            (random.nextFloat() * 2.0f - 1.0f) * 1.5f * intensity;

        // Gelernte Position skalieren
        const float finalBeatPosition =
            learnedBeatPosition * intensity * 5.0f *  strumVariation;

        const double strumOffsetMs =
            static_cast<double>(
                finalBeatPosition) *
            beatDurationMs;

        // Gelernter Strum + kleine Abweichung
        const double finalOffsetMs =
            strumOffsetMs +
            noteVariationMs;

        const int64_t offsetSamples =
            static_cast<int64_t>(
                getSampleRate() * finalOffsetMs / 1000.0);

        int64_t targetSample =
            event.originalSample +
            offsetSamples;

        targetSample =
            juce::jmax(
                targetSample,
                totalSamples);

        // Verschobenes Note-On
        pendingMidiEvents.push_back(
            {event.message,
             targetSample});

        // Timing-Shift für das zugehörige Note-Off merken
        NoteTimingShift shift;

        shift.noteNumber =
            event.message.getNoteNumber();

        shift.noteOnOriginalSample =
            event.originalSample;

        shift.offsetSamples =
            offsetSamples;

        noteTimingShifts.push_back(shift);
    }

    // =====================================================
    // APPLY DELAYED NOTE-OFFS
    // =====================================================

    for (const auto &noteOff :
         pendingGroupNoteOffs)
    {
        const int noteNumber =
            noteOff.message.getNoteNumber();

        int64_t noteOffTargetSample =
            noteOff.originalSample;

        // Passenden Note-On-Offset suchen
        for (auto it = noteTimingShifts.begin();
             it != noteTimingShifts.end();
             ++it)
        {
            if (it->noteNumber == noteNumber)
            {
                noteOffTargetSample +=
                    it->offsetSamples;

                noteTimingShifts.erase(it);

                break;
            }
        }

        noteOffTargetSample =
            juce::jmax(
                noteOffTargetSample,
                totalSamples);

        pendingMidiEvents.push_back(
            {noteOff.message,
             noteOffTargetSample});
    }

    // =====================================================
    // CLEAN UP
    // =====================================================

    pendingGroupNoteOffs.clear();
    currentGroupEvents.clear();
}

// =============================================================
// PROCESS BLOCK
// =============================================================

void HumanizerAudioProcessor::processBlock(
    juce::AudioBuffer<float> &buffer,
    juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // =====================================================
    // BPM / BEAT DURATION
    // =====================================================

    double currentBpm = 130.0;

    if (auto *playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (position->getBpm().hasValue() &&
                *position->getBpm() > 0.0)
            {
                currentBpm = *position->getBpm();
            }
        }
    }

    beatDurationMs = 60000.0 / currentBpm;

    // =====================================================
    // BLOCK INFORMATION
    // =====================================================

    const int64_t blockStartSample = totalSamples;
    const int64_t blockEndSample =
        totalSamples + buffer.getNumSamples();

    juce::MidiBuffer outputMidi;

    // =====================================================
    // PROCESS ALREADY DELAYED EVENTS
    // =====================================================

    for (auto it = pendingMidiEvents.begin();
         it != pendingMidiEvents.end();)
    {
        if (it->targetSample < blockEndSample)
        {
            const int samplePosition =
                static_cast<int>(
                    juce::jlimit<int64_t>(
                        0,
                        buffer.getNumSamples() - 1,
                        it->targetSample - blockStartSample));

            outputMidi.addEvent(
                it->message,
                samplePosition);

            it = pendingMidiEvents.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // =====================================================
    // READ INCOMING MIDI
    // =====================================================

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        const int originalSamplePosition =
            metadata.samplePosition;

        const int64_t absoluteSample =
            totalSamples + originalSamplePosition;

        // =================================================
        // NOTE ON
        // =================================================

        if (message.isNoteOn() && !message.isNoteOff())
        {
            const int noteNumber =
                message.getNoteNumber();

            const int velocity =
                message.getVelocity();

            // ---------------------------------------------
            // GROUP DETECTION
            // ---------------------------------------------

            if (groupStartSample == -1 ||
                absoluteSample - groupStartSample > 900)
            {
                if (!currentGroupEvents.empty())
                {
                    applyLearnedStrumToGroup();
                    currentGroupEvents.clear();
                }

                groupStartSample = absoluteSample;
                currentGroupId++;
                currentGroupNotes.clear();
            }

            // ---------------------------------------------
            // STORE NOTE
            // ---------------------------------------------

            PendingGroupNote groupNote;
            groupNote.message = message;
            groupNote.originalSample = absoluteSample;

            currentGroupEvents.push_back(groupNote);

            // ---------------------------------------------
            // HELD NOTES
            // ---------------------------------------------

            if (std::find(
                    heldNotes.begin(),
                    heldNotes.end(),
                    noteNumber) == heldNotes.end())
            {
                heldNotes.push_back(noteNumber);
            }

            // ---------------------------------------------
            // RECENT NOTE
            // ---------------------------------------------

            RecentNote recent;

            recent.noteNumber = noteNumber;
            recent.velocity = velocity;
            recent.samplePosition = originalSamplePosition;
            recent.absoluteSample = absoluteSample;
            recent.noteOffSample = -1;
            recent.duration = 0;
            recent.groupId =
                static_cast<int>(currentGroupId);

            recentNotes.push_back(recent);

            if (recentNotes.size() > 100)
                recentNotes.erase(
                    recentNotes.begin());

            // ---------------------------------------------
            // NOTE + VELOCITY GUI
            // ---------------------------------------------

            const juce::String noteName =
                juce::MidiMessage::getMidiNoteName(
                    noteNumber,
                    true,
                    true,
                    4);

            juce::MessageManager::callAsync(
                [this, noteName, velocity]()
                {
                    if (auto *editor =
                            dynamic_cast<
                                HumanizerAudioProcessorEditor *>(
                                getActiveEditor()))
                    {
                        editor->changePlaceholderText(
                            noteName);

                        editor->changePlaceholderTextVelocity(
                            juce::String(velocity));
                    }
                });

            // ---------------------------------------------
            // BEAT DURATION GUI
            // ---------------------------------------------

            const double currentBeatDurationMs =
                beatDurationMs;

            juce::MessageManager::callAsync(
                [this, currentBeatDurationMs]()
                {
                    if (auto *editor =
                            dynamic_cast<
                                HumanizerAudioProcessorEditor *>(
                                getActiveEditor()))
                    {
                        editor->changeBeatDurationText(
                            juce::String(
                                currentBeatDurationMs,
                                1));
                    }
                });

            // ---------------------------------------------
            // INTERVAL
            // ---------------------------------------------

            if (recentNotes.size() >= 2)
            {
                const auto &previous =
                    recentNotes[recentNotes.size() - 2];

                const int interval =
                    noteNumber -
                    previous.noteNumber;

                juce::MessageManager::callAsync(
                    [this, interval]()
                    {
                        if (auto *editor =
                                dynamic_cast<
                                    HumanizerAudioProcessorEditor *>(
                                    getActiveEditor()))
                        {
                            editor->changeIntervalText(
                                juce::String(interval));
                        }
                    });
            }

            // ---------------------------------------------
            // TIMING DEVIATION
            // ---------------------------------------------

            if (recentNotes.size() >= 2)
            {
                const auto &previous =
                    recentNotes[recentNotes.size() - 2];

                const double deviationMs =
                    (absoluteSample -
                     previous.absoluteSample) /
                    getSampleRate() * 1000.0;

                juce::MessageManager::callAsync(
                    [this, deviationMs]()
                    {
                        if (auto *editor =
                                dynamic_cast<
                                    HumanizerAudioProcessorEditor *>(
                                    getActiveEditor()))
                        {
                            editor->changeTimingDeviationText(
                                juce::String(
                                    deviationMs,
                                    1));
                        }
                    });
            }

            // ---------------------------------------------
            // VELOCITY DIFFERENCE
            // ---------------------------------------------

            if (recentNotes.size() >= 2)
            {
                const auto &previous =
                    recentNotes[recentNotes.size() - 2];

                const int velocityDifference =
                    velocity -
                    previous.velocity;

                juce::MessageManager::callAsync(
                    [this, velocityDifference]()
                    {
                        if (auto *editor =
                                dynamic_cast<
                                    HumanizerAudioProcessorEditor *>(
                                    getActiveEditor()))
                        {
                            editor->changeVelocityDifferenceText(
                                juce::String(
                                    velocityDifference));
                        }
                    });
            }

            // ---------------------------------------------
            // RECENT NOTES
            // ---------------------------------------------

            juce::String recentNotesText;

            const int numberToShow =
                juce::jmin(
                    8,
                    static_cast<int>(
                        recentNotes.size()));

            for (int i = numberToShow;
                 i > 0;
                 --i)
            {
                const auto &n =
                    recentNotes[recentNotes.size() - i];

                recentNotesText +=
                    juce::MidiMessage::getMidiNoteName(
                        n.noteNumber,
                        true,
                        true,
                        4);

                if (i > 1)
                    recentNotesText += " ";
            }

            juce::MessageManager::callAsync(
                [this, recentNotesText]()
                {
                    if (auto *editor =
                            dynamic_cast<
                                HumanizerAudioProcessorEditor *>(
                                getActiveEditor()))
                    {
                        editor->changeRecentNotesText(
                            recentNotesText);
                    }
                });
        }

        // =================================================
        // NOTE OFF
        // =================================================

        else if (message.isNoteOff())
        {
            const int noteNumber =
                message.getNoteNumber();

            heldNotes.erase(
                std::remove(
                    heldNotes.begin(),
                    heldNotes.end(),
                    noteNumber),
                heldNotes.end());

            // =====================================================
            // Gehört diese Note noch zu einer offenen Strum-Gruppe?
            // =====================================================

            bool belongsToCurrentGroup = false;

            for (const auto &groupNote : currentGroupEvents)
            {
                if (groupNote.message.getNoteNumber() == noteNumber)
                {
                    belongsToCurrentGroup = true;
                    break;
                }
            }

            if (belongsToCurrentGroup)
            {
                PendingGroupNoteOff noteOff;

                noteOff.message = message;
                noteOff.originalSample = absoluteSample;

                pendingGroupNoteOffs.push_back(noteOff);
            }
            else
            {
                // =================================================
                // Note gehört zu keiner offenen Group
                // =================================================

                int64_t noteOffTargetSample =
                    absoluteSample;

                bool foundTimingShift = false;

                for (auto it = noteTimingShifts.begin();
                     it != noteTimingShifts.end();
                     ++it)
                {
                    if (it->noteNumber == noteNumber)
                    {
                        noteOffTargetSample +=
                            it->offsetSamples;

                        noteTimingShifts.erase(it);

                        foundTimingShift = true;
                        break;
                    }
                }

                if (!foundTimingShift)
                {
                    noteOffTargetSample =
                        absoluteSample;
                }

                noteOffTargetSample =
                    juce::jmax(
                        noteOffTargetSample,
                        totalSamples);

                pendingMidiEvents.push_back(
                    {message,
                     noteOffTargetSample});
            }
        }

        // =================================================
        // OTHER MIDI
        // =================================================

        else
        {
            outputMidi.addEvent(
                message,
                originalSamplePosition);
        }
    }

    // =====================================================
    // CLOSE GROUP
    // =====================================================

    if (groupStartSample != -1 &&
        !currentGroupEvents.empty())
    {
        if (blockEndSample -
                groupStartSample >
            900)
        {
            applyLearnedStrumToGroup();

            {
                // =====================================================
                // ZURÜCKGEHALTENE NOTE-OFFS
                // =====================================================

                for (const auto &noteOff : pendingGroupNoteOffs)
                {
                    int64_t noteOffTargetSample =
                        noteOff.originalSample;

                    const int noteNumber =
                        noteOff.message.getNoteNumber();

                    for (const auto &shift : noteTimingShifts)
                    {
                        if (shift.noteNumber == noteNumber)
                        {
                            noteOffTargetSample +=
                                shift.offsetSamples;

                            break;
                        }
                    }

                    noteOffTargetSample =
                        juce::jmax(
                            noteOffTargetSample,
                            totalSamples);

                    pendingMidiEvents.push_back(
                        {noteOff.message,
                         noteOffTargetSample});
                }

                pendingGroupNoteOffs.clear();

                currentGroupEvents.clear();
            }

            currentGroupEvents.clear();

            groupStartSample = -1;
        }
    }

    // =====================================================
    // ADD NEWLY GENERATED EVENTS
    // =====================================================

    for (auto it = pendingMidiEvents.begin();
         it != pendingMidiEvents.end();)
    {
        if (it->targetSample < blockEndSample)
        {
            const int samplePosition =
                static_cast<int>(
                    juce::jlimit<int64_t>(
                        0,
                        buffer.getNumSamples() - 1,
                        it->targetSample -
                            blockStartSample));

            outputMidi.addEvent(
                it->message,
                samplePosition);

            it = pendingMidiEvents.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // =====================================================
    // OUTPUT
    // =====================================================

    midiMessages.swapWith(outputMidi);

    totalSamples += buffer.getNumSamples();
}
bool HumanizerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *
HumanizerAudioProcessor::createEditor()
{
    return new HumanizerAudioProcessorEditor(
        *this);
}

// =============================================================
// STATE
// =============================================================

void HumanizerAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData)
{
}

void HumanizerAudioProcessor::setStateInformation(
    const void *data,
    int sizeInBytes)
{
}

// =============================================================
// PLUGIN CREATOR
// =============================================================

juce::AudioProcessor *
    JUCE_CALLTYPE
    createPluginFilter()
{
    return new HumanizerAudioProcessor();
}