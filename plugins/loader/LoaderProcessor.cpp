#include "LoaderProcessor.h"

#include "CrashLog.h"
#include "LoaderEditor.h"
#include "LoaderLog.h"
#include "pluginlab/hosting/LoaderState.h"
#include "pluginlab/ui/GuiFormats.h"

namespace
{
constexpr int kNumberOfPrograms = 1;
constexpr int kOnlyProgramIndex = 0;
constexpr double kDefaultSampleRate = 44100.0;
constexpr int kDefaultBlockSize = 512;
}

LoaderProcessor::LoaderProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      m_sampleRate(kDefaultSampleRate),
      m_blockSize(kDefaultBlockSize)
{
    pluginlab::ui::addGuiFormats(m_formatManager);
    loaderlog::installCrashHandler();
    loaderlog::write("loader created");
}

LoaderProcessor::~LoaderProcessor()
{
    loaderlog::write("loader deleted");
    const juce::ScopedLock lock(m_hostedLock);
    m_hosted.reset();
}

const juce::String LoaderProcessor::getName() const
{
    return "PluginLabLoader";
}

void LoaderProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    loaderlog::write("prepareToPlay: " + juce::String(sampleRate) + " Hz, " + juce::String(samplesPerBlock) + " samples, "
                     + juce::String(getTotalNumInputChannels()) + " in / " + juce::String(getTotalNumOutputChannels()) + " out");
    m_firstBlockLogged = false;
    m_sampleRate = sampleRate;
    m_blockSize = samplesPerBlock;
    const juce::ScopedLock lock(m_hostedLock);
    configureHostedPlugin();
}

void LoaderProcessor::releaseResources()
{
    loaderlog::write("releaseResources");
    const juce::ScopedLock lock(m_hostedLock);
    if (m_hosted != nullptr)
    {
        m_hosted->getInstance().releaseResources();
    }
}

// Caller holds the lock. The loaded plugin gets the channel layout and the sample rate / block size of the loader.
void LoaderProcessor::configureHostedPlugin()
{
    if (m_hosted == nullptr)
    {
        return;
    }
    juce::AudioPluginInstance& instance = m_hosted->getInstance();
    instance.setPlayConfigDetails(m_hostedChannels, m_hostedChannels, m_sampleRate, m_blockSize);
    m_hostedBuffer.setSize(m_hostedChannels, m_blockSize);
    instance.prepareToPlay(m_sampleRate, m_blockSize);
}

// The loaded plugin runs with the channel layout of the loader if it can; a mono plugin in a stereo loader (or the other way round)
// runs with its own layout and processAdapted() converts. Returns false if the plugin supports neither mono nor stereo.
bool LoaderProcessor::chooseHostedLayout(juce::AudioPluginInstance& instance, int& channels) const
{
    const int loaderChannels = getTotalNumInputChannels();
    const std::vector<juce::AudioChannelSet> candidates = {
        juce::AudioChannelSet::canonicalChannelSet(loaderChannels), juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono()};
    for (const juce::AudioChannelSet& candidate : candidates)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(candidate);
        layout.outputBuses.add(candidate);
        if (instance.checkBusesLayoutSupported(layout) && instance.setBusesLayout(layout))
        {
            channels = candidate.size();
            return true;
        }
    }
    return false;
}

// Audio of the loader (C channels) through a plugin with H channels: H = 1 gets the mean of all channels and its output goes to all
// channels; H = 2 in a mono loader gets the signal on both channels and its left output is the result.
void LoaderProcessor::processAdapted(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int samples = buffer.getNumSamples();
    const int loaderChannels = buffer.getNumChannels();
    if (samples > m_hostedBuffer.getNumSamples())
    {
        return; // a block bigger than announced: pass the audio through
    }
    juce::AudioBuffer<float> hostedBlock(m_hostedBuffer.getArrayOfWritePointers(), m_hostedChannels, samples);
    for (int hostedChannel = 0; hostedChannel < m_hostedChannels; ++hostedChannel)
    {
        hostedBlock.clear(hostedChannel, 0, samples);
        for (int channel = 0; channel < loaderChannels; ++channel)
        {
            const bool contributes = m_hostedChannels == 1 || loaderChannels == 1 || channel == hostedChannel;
            if (contributes)
            {
                hostedBlock.addFrom(hostedChannel, 0, buffer, channel, 0, samples);
            }
        }
        if (m_hostedChannels == 1)
        {
            hostedBlock.applyGain(hostedChannel, 0, samples, 1.0f / static_cast<float>(loaderChannels));
        }
    }

    m_hosted->getInstance().processBlock(hostedBlock, midiMessages);

    for (int channel = 0; channel < loaderChannels; ++channel)
    {
        const int hostedChannel = juce::jmin(channel, m_hostedChannels - 1);
        buffer.copyFrom(channel, 0, hostedBlock, hostedChannel, 0, samples);
    }
}

bool LoaderProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const juce::AudioChannelSet& outputSet = layouts.getMainOutputChannelSet();
    const bool outputIsMonoOrStereo = outputSet == juce::AudioChannelSet::mono() || outputSet == juce::AudioChannelSet::stereo();
    const bool inputMatchesOutput = layouts.getMainInputChannelSet() == outputSet;
    return outputIsMonoOrStereo && inputMatchesOutput;
}

void LoaderProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (! m_firstBlockLogged)
    {
        m_firstBlockLogged = true; // once per prepareToPlay, to see in the log that the host processes and with which buffer
        loaderlog::write("first processBlock: " + juce::String(buffer.getNumChannels()) + " channels, " + juce::String(buffer.getNumSamples()) + " samples");
    }

    const juce::ScopedTryLock lock(m_hostedLock);
    if (! lock.isLocked() || m_hosted == nullptr)
    {
        return; // pass-through: the audio stays in the buffer unchanged
    }
    m_hosted->getInstance().setPlayHead(getPlayHead()); // tempo and position of the DAW for plugins that use them
    if (m_hostedChannels == buffer.getNumChannels())
    {
        m_hosted->getInstance().processBlock(buffer, midiMessages);
        return;
    }
    processAdapted(buffer, midiMessages);
}

juce::AudioProcessorEditor* LoaderProcessor::createEditor()
{
    return new LoaderEditor(*this);
}

bool LoaderProcessor::hasEditor() const
{
    return true;
}

bool LoaderProcessor::acceptsMidi() const
{
    return false;
}

bool LoaderProcessor::producesMidi() const
{
    return false;
}

double LoaderProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LoaderProcessor::getNumPrograms()
{
    return kNumberOfPrograms;
}

int LoaderProcessor::getCurrentProgram()
{
    return kOnlyProgramIndex;
}

void LoaderProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String LoaderProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void LoaderProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

pluginlab::hosting::HostedPlugin* LoaderProcessor::getHostedPlugin()
{
    return m_hosted.get();
}

bool LoaderProcessor::loadPlugin(const juce::PluginDescription& description, juce::String& errorMessage)
{
    loaderlog::write("load: " + description.name + " (" + description.fileOrIdentifier + ")");
    if (description.isInstrument)
    {
        errorMessage = "Instruments are not supported by the loader yet (effects only): " + description.name;
        return false;
    }
    std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin =
        pluginlab::hosting::HostedPlugin::load(m_formatManager, description, m_sampleRate, m_blockSize, errorMessage);
    if (plugin == nullptr)
    {
        return false;
    }

    int hostedChannels = 0;
    if (! chooseHostedLayout(plugin->getInstance(), hostedChannels))
    {
        errorMessage = description.name + " runs neither with mono nor with stereo audio (the loader supports only these)";
        return false;
    }

    if (onBeforeHostedPluginChanged)
    {
        onBeforeHostedPluginChanged(); // the editor of the old plugin goes first
    }
    {
        const juce::ScopedLock lock(m_hostedLock);
        if (m_hosted != nullptr)
        {
            m_hosted->getInstance().releaseResources();
        }
        m_hosted = std::move(plugin);
        m_hostedChannels = hostedChannels;
        configureHostedPlugin();
    }
    const int latency = m_hosted->getInstance().getLatencySamples();
    loaderlog::write("load: " + juce::String(hostedChannels) + " channel(s), latency " + juce::String(latency) + " samples");
    setLatencySamples(latency);
    if (onHostedPluginChanged)
    {
        onHostedPluginChanged();
    }
    return true;
}

void LoaderProcessor::unloadPlugin()
{
    if (onBeforeHostedPluginChanged)
    {
        onBeforeHostedPluginChanged();
    }
    {
        const juce::ScopedLock lock(m_hostedLock);
        m_hosted.reset();
    }
    setLatencySamples(0);
    if (onHostedPluginChanged)
    {
        onHostedPluginChanged();
    }
}

void LoaderProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock(m_hostedLock);
    if (m_hosted == nullptr)
    {
        return;
    }
    loaderlog::write("save: asking " + m_hosted->getDescription().name + " for its state");
    juce::MemoryBlock hostedState;
    m_hosted->getInstance().getStateInformation(hostedState);
    loaderlog::write("save: " + juce::String(hostedState.getSize()) + " bytes of plugin state");
    destData = pluginlab::hosting::createLoaderState(m_hosted->getDescription(), hostedState);
}

void LoaderProcessor::restoreState(const juce::MemoryBlock& state)
{
    loaderlog::write("restore: start, " + juce::String(state.getSize()) + " bytes");
    juce::PluginDescription description;
    juce::MemoryBlock hostedState;
    if (! pluginlab::hosting::parseLoaderState(state.getData(), static_cast<int>(state.getSize()), description, hostedState))
    {
        loaderlog::write("restore: the saved state is not a loader state (" + juce::String(state.getSize()) + " bytes)");
        return;
    }
    juce::String error;
    if (! loadPlugin(description, error))
    {
        // the plugin is gone or does not load: the loader stays empty
        loaderlog::write("restore: cannot load " + description.fileOrIdentifier + ": " + error);
        return;
    }
    loaderlog::write("restore: loaded " + description.name + ", giving it " + juce::String(hostedState.getSize()) + " bytes of state");
    if (hostedState.getSize() > 0)
    {
        const juce::ScopedLock lock(m_hostedLock);
        m_hosted->getInstance().setStateInformation(hostedState.getData(), static_cast<int>(hostedState.getSize()));
    }
    loaderlog::write("restore: done");
}

void LoaderProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Loading needs the message thread. Some hosts call this from another thread: then it is done there, a moment later.
    const juce::MemoryBlock state(data, static_cast<size_t>(sizeInBytes));
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        restoreState(state);
        return;
    }
    loaderlog::write("restore: called from another thread, restoring later on the message thread");
    const juce::WeakReference<LoaderProcessor> self(this);
    juce::MessageManager::callAsync(
        [self, state]
        {
            if (self != nullptr) // the host may have deleted the loader in the meantime
            {
                self->restoreState(state);
            }
        });
}

// the entry point that the plugin wrappers call
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LoaderProcessor();
}
