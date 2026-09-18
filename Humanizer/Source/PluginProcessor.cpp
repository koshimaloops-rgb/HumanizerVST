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

void HumanizerAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

void HumanizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
}

void HumanizerAudioProcessor::releaseResources()
{
}

bool HumanizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return true;
}

void HumanizerAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)

    for(const auto metadata : midiMessages) {

    auto message = metadata.getMessage();
    
    // Check if a MIDI key was pressed down
    if (message.isNoteOn())
    {
        int noteNumber = message.getNoteNumber();
        // Turn the number (e.g. 60) into a readable name (e.g. "C4")
        juce::String noteName = juce::MidiMessage::getMidiNoteName (noteNumber, true, true, 3);

        // Safely send the text to your UI placeholder
        juce::MessageManager::callAsync ([this, noteName]()
        {
            if (auto* editor = dynamic_cast<HumanizerAudioProcessorEditor*>(getActiveEditor()))
            {
                editor->changePlaceholderText (noteName);
            }
        });
    }
    }

//VERHALTEN

bool HumanizerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* HumanizerAudioProcessor::createEditor()
{
    return new HumanizerAudioProcessorEditor(*this);
}

void HumanizerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
}

void HumanizerAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HumanizerAudioProcessor();
}