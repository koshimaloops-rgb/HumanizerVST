#include "PluginProcessor.h"
#include "PluginEditor.h"

HumanizerAudioProcessor::HumanizerAudioProcessor()
{
}

HumanizerAudioProcessor::~HumanizerAudioProcessor()
{
}

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

void HumanizerAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void HumanizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
}
void HumanizerAudioProcessor::setVelocityHumanize(float amount)
{
    velocityHumanizeAmount = amount;
}

void HumanizerAudioProcessor::releaseResources()
{
}

bool HumanizerAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return true;
}

void HumanizerAudioProcessor::processBlock(
    juce::AudioBuffer<float> &buffer,
    juce::MidiBuffer &midiMessages)
{
    juce::MidiBuffer newMidiMessages;

    // Anfang dieses Audio-Blocks
    int64_t blockStartSample = totalSamples;

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        // =====================================================
        // NOTE ON
        // =====================================================

        if (message.isNoteOn())
        {
            DBG("Humanize: " << velocityHumanizeAmount);

            int velocity = message.getVelocity();
            int noteNumber = message.getNoteNumber();

            // -------------------------------------------------
            // NOTE ALS GEHALTEN SPEICHERN
            // -------------------------------------------------

            if (std::find(
                    heldNotes.begin(),
                    heldNotes.end(),
                    noteNumber) == heldNotes.end())
            {
                heldNotes.push_back(noteNumber);
            }

            // -------------------------------------------------
            // ABSOLUTE ZEIT DER NOTE
            // -------------------------------------------------

            int64_t currentSample =
                blockStartSample + metadata.samplePosition;

            // -------------------------------------------------
            // NEUE GRUPPE?
            // -------------------------------------------------

            if (groupStartSample == -1 ||
                currentSample - groupStartSample > 2400)
            {
                groupStartSample = currentSample;

                currentGroupId++;

                currentGroupNotes = "";
            }

            // -------------------------------------------------
            // NOTE ZUR AKTUELLEN GRUPPE HINZUFÜGEN
            // -------------------------------------------------

            const char *groupNoteNames[] =
                {
                    "C", "C#", "D", "D#", "E", "F",
                    "F#", "G", "G#", "A", "A#", "B"};

            int groupOctave = noteNumber / 12;

            currentGroupNotes +=
                juce::String(groupNoteNames[noteNumber % 12]) + juce::String(groupOctave) + " ";

            // -------------------------------------------------
            // GRUPPE ANZEIGEN
            // -------------------------------------------------

            juce::String groupText =
                "Group " + juce::String(currentGroupId) + ": " + currentGroupNotes;

            juce::MessageManager::callAsync(
                [this, groupText]()
                {
                    if (auto *editor =
                            dynamic_cast<HumanizerAudioProcessorEditor *>(
                                getActiveEditor()))
                    {
                        editor->changeRecentNotesText(
                            groupText);
                    }
                });

            // -------------------------------------------------
            // RECENT NOTES SPEICHERN
            // -------------------------------------------------

            recentNotes.push_back({noteNumber,
                                   velocity,
                                   metadata.samplePosition,
                                   currentSample,
                                   currentGroupId});

            if (recentNotes.size() > 100)
            {
                recentNotes.erase(
                    recentNotes.begin());
            }

            // -------------------------------------------------
            // GRUPPENDAUER
            // -------------------------------------------------

            int64_t groupDuration =
                currentSample - groupStartSample;

            DBG(
                "Group "
                << currentGroupId
                << " | Note "
                << noteNumber
                << " | Duration "
                << groupDuration
                << " samples");

            // -------------------------------------------------
            // INTERVALL
            // -------------------------------------------------

            if (recentNotes.size() >= 2)
            {
                auto &previous =
                    recentNotes[recentNotes.size() - 2];

                auto &current =
                    recentNotes.back();
                int64_t timeBetweenNotes =
                    current.absoluteSample - previous.absoluteSample;
                juce::MessageManager::callAsync(
                    [this, timeBetweenNotes]()
                    {
                        if (auto *editor =
                                dynamic_cast<HumanizerAudioProcessorEditor *>(
                                    getActiveEditor()))
                        {
                            
                            editor->changeTimeBetweenText(
                                juce::String(timeBetweenNotes));
                        }
                    });

                DBG(
                    "Time between notes: "
                    << timeBetweenNotes
                    << " samples");

                int interval =
                    current.noteNumber - previous.noteNumber;

                DBG(
                    "Interval: "
                    << interval
                    << " semitones");

                int velocityDifference =
                    current.velocity - previous.velocity;

                DBG(
                    "Velocity difference: "
                    << velocityDifference);

                juce::MessageManager::callAsync(
                    [this, interval]()
                    {
                        if (auto *editor =
                                dynamic_cast<HumanizerAudioProcessorEditor *>(
                                    getActiveEditor()))
                        {
                            editor->changeIntervalText(
                                juce::String(interval));
                        }
                    });
            }

            // -------------------------------------------------
            // VELOCITY
            // -------------------------------------------------

            int newVelocity = velocity;

            newVelocity = juce::jlimit(
                1,
                127,
                newVelocity);

            message.setVelocity(newVelocity);

            // -------------------------------------------------
            // NOTE NAME
            // -------------------------------------------------

            const char *noteNames[] =
                {
                    "C", "C#", "D", "D#", "E", "F",
                    "F#", "G", "G#", "A", "A#", "B"};

            int octave =
                (noteNumber / 12) - 1;

            juce::String noteName =
                juce::String(
                    noteNames[noteNumber % 12]) +
                juce::String(octave);

            juce::MessageManager::callAsync(
                [this, noteName]()
                {
                    if (auto *editor =
                            dynamic_cast<HumanizerAudioProcessorEditor *>(
                                getActiveEditor()))
                    {
                        editor->changePlaceholderText(
                            noteName);
                    }
                });

            // -------------------------------------------------
            // VELOCITY ANZEIGE
            // -------------------------------------------------

            juce::MessageManager::callAsync(
                [this, newVelocity]()
                {
                    if (auto *editor =
                            dynamic_cast<HumanizerAudioProcessorEditor *>(
                                getActiveEditor()))
                    {
                        editor->changePlaceholderTextVelocity(
                            juce::String(newVelocity));
                    }
                });
        }

        // =====================================================
        // NOTE OFF
        // =====================================================

        if (message.isNoteOff())
        {
            int noteNumber = message.getNoteNumber();

            int64_t noteOffSample =
                blockStartSample + metadata.samplePosition;

            // passende Note in recentNotes suchen
            for (auto it = recentNotes.rbegin();
                 it != recentNotes.rend();
                 ++it)
            {
                if (it->noteNumber == noteNumber &&
                    it->noteOffSample == -1)
                {
                    it->noteOffSample = noteOffSample;

                    it->duration =
                        it->noteOffSample - it->absoluteSample;
                    int64_t duration = it->duration;

                    juce::MessageManager::callAsync(
                        [this, duration]()
                        {
                            if (auto *editor =
                                    dynamic_cast<HumanizerAudioProcessorEditor *>(
                                        getActiveEditor()))
                            {
                                editor->changeDurationText(
                                    juce::String(duration));
                            }
                        });

                    DBG(
                        "NOTE "
                        << noteNumber
                        << " | Duration: "
                        << it->duration
                        << " samples");

                    break;
                }
            }

            heldNotes.erase(
                std::remove(
                    heldNotes.begin(),
                    heldNotes.end(),
                    noteNumber),
                heldNotes.end());
        }

        // =====================================================
        // MIDI EVENT WEITERSCHREIBEN
        // =====================================================

        if (message.isNoteOn())
        {
            DBG(
                "OUTPUT VELOCITY: "
                << message.getVelocity());
        }

        newMidiMessages.addEvent(
            message,
            metadata.samplePosition);
    }

    // =========================================================
    // GESAMTZEIT FORTSCHREIBEN
    // =========================================================

    totalSamples += buffer.getNumSamples();

    // =========================================================
    // MIDI BUFFER ERSETZEN
    // =========================================================

    midiMessages.swapWith(newMidiMessages);
}
// VERHALTEN

bool HumanizerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *HumanizerAudioProcessor::createEditor()
{
    return new HumanizerAudioProcessorEditor(*this);
}

void HumanizerAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
}

void HumanizerAudioProcessor::setStateInformation(
    const void *data,
    int sizeInBytes)
{
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new HumanizerAudioProcessor();
}