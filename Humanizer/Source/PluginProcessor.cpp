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

void HumanizerAudioProcessor::releaseResources()
{
}

bool HumanizerAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return true;
}

void HumanizerAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::MidiBuffer newMidiMessages;

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        // Note wurde gedrückt
        if (message.isNoteOn())
        {
            int velocity = message.getVelocity();

            int noteNumber = message.getNoteNumber();

            // -------------------------
            // NOTE + VELOCITY ANALYSE
            // -------------------------

            recentNotes.push_back({
                noteNumber,
                velocity,
                metadata.samplePosition
            });

            if (recentNotes.size() > 100)
                recentNotes.erase(recentNotes.begin());

            // -------------------------
            // INTERVALL ANALYSE
            // -------------------------

            if (recentNotes.size() >= 2)
            {
                auto& previous =
                    recentNotes[recentNotes.size() - 2];

                auto& current =
                    recentNotes.back();

                int interval =
                    current.noteNumber - previous.noteNumber;

                DBG("Interval: "
                    << interval
                    << " semitones");

                int velocityDifference =
                    current.velocity - previous.velocity;

                DBG("Velocity difference: "
                    << velocityDifference);

                juce::MessageManager::callAsync(
                    [this, interval]()
                    {
                        if (auto* editor =
                            dynamic_cast<HumanizerAudioProcessorEditor*>(
                                getActiveEditor()))
                        {
                            editor->changeIntervalText(
                                juce::String(interval));
                        }
                    });
            }

            // -------------------------
            // VELOCITY HUMANIZER TEST
            // -------------------------

            int newVelocity = velocity ;

            // Velocity darf nur 1-127 sein
            newVelocity = juce::jlimit(
                1,
                127,
                newVelocity);

            // Veränderte Velocity setzen
            message.setVelocity(newVelocity);

            // -------------------------
            // NOTE NAME ANZEIGEN
            // -------------------------

            juce::String noteName =
                juce::MidiMessage::getMidiNoteName(
                    noteNumber,
                    true,
                    true,
                    3);

            juce::MessageManager::callAsync(
                [this, noteName]()
                {
                    if (auto* editor =
                        dynamic_cast<HumanizerAudioProcessorEditor*>(
                            getActiveEditor()))
                    {
                        editor->changePlaceholderText(
                            noteName);
                    }
                });

            // -------------------------
            // VELOCITY ANZEIGEN
            // -------------------------

            juce::MessageManager::callAsync(
                [this, newVelocity]()
                {
                    if (auto* editor =
                        dynamic_cast<HumanizerAudioProcessorEditor*>(
                            getActiveEditor()))
                    {
                        editor->changePlaceholderTextVelocity(
                            juce::String(newVelocity));
                    }
                });
        }

        // MIDI Event wieder in den neuen Buffer schreiben
        newMidiMessages.addEvent(
            message,
            metadata.samplePosition);
    }

    // Alten MIDI-Buffer durch unseren veränderten ersetzen
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