#include "pluginlab/hosting/PluginProbe.h"

#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::hosting
{
namespace
{
const juce::String kProbeArgument = "--probe";
const juce::String kStepPrefix = "STEP: ";
const juce::String kResultPrefix = "RESULT: ";
const juce::String kResultOk = "OK";
const juce::String kResultFailed = "FAILED ";
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kNumberOfNoiseBlocks = 200;
constexpr int kMaximumParametersToChange = 50;
constexpr float kNoiseLevel = 0.1f;
constexpr int kRandomSeed = 1;
constexpr int kMidiChannel = 1;
constexpr int kMidiNote = 60;
constexpr float kMidiVelocity = 0.8f;
constexpr int kMsPerSecond = 1000;

void note(const juce::File& resultFile, const juce::String& step)
{
    resultFile.appendText(kStepPrefix + step + "\n");
}

void fail(const juce::File& resultFile, const juce::String& message)
{
    resultFile.appendText(kResultPrefix + kResultFailed + message + "\n");
}

void processNoise(juce::AudioPluginInstance& instance, int numberOfBlocks, juce::Random& random)
{
    const int channels = juce::jmax(1, instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer(channels, kBlockSize);
    juce::MidiBuffer midi;
    if (instance.getPluginDescription().isInstrument)
    {
        midi.addEvent(juce::MidiMessage::noteOn(kMidiChannel, kMidiNote, kMidiVelocity), 0);
    }
    for (int block = 0; block < numberOfBlocks; ++block)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            for (int sample = 0; sample < kBlockSize; ++sample)
            {
                buffer.setSample(channel, sample, kNoiseLevel * (random.nextFloat() * 2.0f - 1.0f));
            }
        }
        instance.processBlock(buffer, midi);
        midi.clear();
    }
}
}

const juce::String& getProbeArgument()
{
    return kProbeArgument;
}

bool probePluginInProcess(juce::AudioPluginFormatManager& formatManager,
                          const juce::File& pluginFile,
                          const juce::String& pluginIdentifier,
                          const juce::File& resultFile)
{
    resultFile.deleteFile();

    note(resultFile, "scanning the file");
    const PluginScanResult scan = PluginScanner::scanFileInProcess(formatManager, pluginFile);
    const juce::PluginDescription* wanted = nullptr;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (description.createIdentifierString() == pluginIdentifier)
        {
            wanted = &description;
        }
    }
    if (wanted == nullptr)
    {
        fail(resultFile, "the plugin is not in the file (any more)");
        return false;
    }

    note(resultFile, "loading the plugin");
    juce::String error;
    std::unique_ptr<HostedPlugin> plugin = HostedPlugin::load(formatManager, *wanted, kSampleRate, kBlockSize, error);
    if (plugin == nullptr)
    {
        fail(resultFile, "cannot load: " + error);
        return false;
    }
    juce::AudioPluginInstance& instance = plugin->getInstance();
    juce::Random random(kRandomSeed);

    note(resultFile, "preparing the plugin");
    instance.setPlayConfigDetails(instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels(), kSampleRate, kBlockSize);
    instance.prepareToPlay(kSampleRate, kBlockSize);

    note(resultFile, "processing audio");
    processNoise(instance, kNumberOfNoiseBlocks, random);

    note(resultFile, "changing parameters");
    const int numberOfParameters = juce::jmin(plugin->getNumParameters(), kMaximumParametersToChange);
    for (int index = 0; index < numberOfParameters; ++index)
    {
        plugin->setParameterNormalised(index, 0.0f);
        processNoise(instance, 1, random);
        plugin->setParameterNormalised(index, 1.0f);
        processNoise(instance, 1, random);
    }

    note(resultFile, "saving and restoring the state");
    juce::MemoryBlock state;
    instance.getStateInformation(state);
    instance.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    processNoise(instance, 1, random);

    note(resultFile, "releasing the plugin");
    instance.releaseResources();
    plugin.reset();

    resultFile.appendText(kResultPrefix + kResultOk + "\n");
    return true;
}

ValidationResult probePlugin(const juce::File& scannerExecutable,
                             const juce::File& pluginFile,
                             const juce::String& pluginIdentifier,
                             int timeoutMs)
{
    ValidationResult result;
    if (! scannerExecutable.existsAsFile())
    {
        result.status = ValidationStatus::NotAvailable;
        result.message = "The scanner " + scannerExecutable.getFullPathName() + " does not exist: no quick check";
        return result;
    }

    juce::TemporaryFile resultFile(".txt");
    juce::ChildProcess child;
    const juce::StringArray arguments{scannerExecutable.getFullPathName(), kProbeArgument, pluginFile.getFullPathName(), pluginIdentifier,
                                      resultFile.getFile().getFullPathName()};
    if (! child.start(arguments, 0))
    {
        result.status = ValidationStatus::NotAvailable;
        result.message = "Cannot start " + scannerExecutable.getFullPathName();
        return result;
    }
    if (! child.waitForProcessToFinish(timeoutMs))
    {
        child.kill();
        result.status = ValidationStatus::TimedOut;
        result.message = "The quick check did not finish in " + juce::String(timeoutMs / kMsPerSecond) + " s";
        return result;
    }

    juce::String lastStep;
    juce::String outcome;
    for (const juce::String& line : juce::StringArray::fromLines(resultFile.getFile().loadFileAsString()))
    {
        if (line.startsWith(kStepPrefix))
        {
            lastStep = line.fromFirstOccurrenceOf(kStepPrefix, false, false);
        }
        if (line.startsWith(kResultPrefix))
        {
            outcome = line.fromFirstOccurrenceOf(kResultPrefix, false, false);
        }
    }

    if (outcome == kResultOk)
    {
        result.status = ValidationStatus::Passed;
        result.message = "Passed the quick check (load, process audio, change parameters, save and restore the state)";
        return result;
    }
    result.status = ValidationStatus::Failed;
    if (outcome.startsWith(kResultFailed))
    {
        result.message = "Failed the quick check: " + outcome.fromFirstOccurrenceOf(kResultFailed, false, false);
        return result;
    }
    // no result line: the process died (on POSIX a child killed by a signal reports exit code 0)
    result.message = "Failed the quick check: the plugin crashed or hung during '" + lastStep + "'";
    return result;
}
}
