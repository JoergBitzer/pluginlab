#include "BenchmarkProcessor.h"

#include "BenchmarkEditor.h"
#include "PageServices.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/ui/GuiFormats.h"

namespace
{
constexpr double kDefaultRate = 48000.0;
constexpr int kDefaultBlockSize = 512;
const juce::String kDryName = "dry (no plugin)";
const juce::String kStateTag = "JadeBenchmark";
const juce::String kSlotTag = "Slot";
constexpr int kStateVersion = 1;
}

BenchmarkProcessor::BenchmarkProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    pluginlab::ui::addGuiFormats(m_formatManager);
    pluginlab::host::setInsideDaw(true);
    pluginlab::host::setHelperExecutable(getBundledHelper());
    m_sampleRate = kDefaultRate;
    m_blockSize = kDefaultBlockSize;
    m_engine.prepare(m_sampleRate, m_blockSize);
    juce::String error;
    m_engine.addSlot(nullptr, kDryName, error); // always there: the reference without a plugin
}

BenchmarkProcessor::~BenchmarkProcessor() = default;

juce::File BenchmarkProcessor::getBundledHelper()
{
    // currentExecutableFile is the plugin binary when this code runs in a plugin; the bundle carries PluginLabHost next to it
    const juce::File folder = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
#if JUCE_WINDOWS
    return folder.getChildFile("PluginLabHost.exe");
#else
    return folder.getChildFile("PluginLabHost");
#endif
}

const juce::String BenchmarkProcessor::getName() const
{
    return "Jade Benchmark";
}

void BenchmarkProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (juce::approximatelyEqual(sampleRate, m_sampleRate) && samplesPerBlock <= m_blockSize)
    {
        return; // the slots are prepared for this already
    }
    // another rate or larger blocks: the engine is prepared again, and the slots are loaded again with their states
    const juce::MemoryBlock state = createState();
    m_sampleRate = sampleRate;
    m_blockSize = std::max(samplesPerBlock, kDefaultBlockSize);
    m_engine.prepare(m_sampleRate, m_blockSize);
    juce::String error;
    m_engine.addSlot(nullptr, kDryName, error);
    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        restoreState(state);
        return;
    }
    juce::WeakReference<BenchmarkProcessor> self(this);
    juce::MessageManager::callAsync([self, state] {
        if (self != nullptr)
        {
            self->restoreState(state);
        }
    });
}

void BenchmarkProcessor::releaseResources()
{
}

bool BenchmarkProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const juce::AudioChannelSet& output = layouts.getMainOutputChannelSet();
    const bool monoOrStereo = output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
    return monoOrStereo && layouts.getMainInputChannelSet() == output;
}

void BenchmarkProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    m_engine.processExternal(buffer); // every slot processes the track; the audible one comes out
}

juce::AudioProcessorEditor* BenchmarkProcessor::createEditor()
{
    return new BenchmarkEditor(*this);
}

bool BenchmarkProcessor::hasEditor() const
{
    return true;
}

bool BenchmarkProcessor::acceptsMidi() const
{
    return false;
}

bool BenchmarkProcessor::producesMidi() const
{
    return false;
}

double BenchmarkProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int BenchmarkProcessor::getNumPrograms()
{
    return 1;
}

int BenchmarkProcessor::getCurrentProgram()
{
    return 0;
}

void BenchmarkProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String BenchmarkProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void BenchmarkProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void BenchmarkProcessor::slotsChanged()
{
    setLatencySamples(m_engine.getLatencyOfEngine()); // all slots are delayed to the slowest one; the DAW compensates that
}

juce::MemoryBlock BenchmarkProcessor::createState()
{
    juce::XmlElement root(kStateTag);
    root.setAttribute("version", kStateVersion);
    root.setAttribute("activeSlot", m_engine.getActiveSlot());
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        pluginlab::hosting::HostedPlugin* plugin = m_engine.getPlugin(slot);
        if (plugin == nullptr)
        {
            continue;
        }
        auto* entry = root.createNewChildElement(kSlotTag);
        entry->setAttribute("name", m_engine.getSlotInfo(slot).name);
        entry->addChildElement(plugin->getDescription().createXml().release());
        juce::MemoryBlock pluginState;
        plugin->getInstance().getStateInformation(pluginState);
        entry->setAttribute("state", pluginState.toBase64Encoding());
    }
    juce::MemoryBlock block;
    copyXmlToBinary(root, block);
    return block;
}

void BenchmarkProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    destData = createState();
}

void BenchmarkProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const juce::MemoryBlock state(data, static_cast<size_t>(sizeInBytes));
    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        restoreState(state);
        return;
    }
    juce::WeakReference<BenchmarkProcessor> self(this);
    juce::MessageManager::callAsync([self, state] {
        if (self != nullptr)
        {
            self->restoreState(state);
        }
    });
}

void BenchmarkProcessor::restoreState(const juce::MemoryBlock& state)
{
    const std::unique_ptr<juce::XmlElement> root = getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    if (root == nullptr || ! root->hasTagName(kStateTag))
    {
        return;
    }
    // the dry reference first, then the plugins with their states (set before the slot measures its latency)
    m_engine.clearSlots();
    juce::String error;
    m_engine.addSlot(nullptr, kDryName, error);
    for (const juce::XmlElement* entry : root->getChildWithTagNameIterator(kSlotTag))
    {
        juce::PluginDescription description;
        const juce::XmlElement* descriptionXml = entry->getChildByName("PLUGIN");
        if (descriptionXml == nullptr || ! description.loadFromXml(*descriptionXml))
        {
            continue;
        }
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin =
            pluginlab::hosting::HostedPlugin::load(m_formatManager, description, m_sampleRate, m_blockSize, error);
        if (plugin == nullptr)
        {
            continue; // a plugin that is gone from this machine: its slot is left out
        }
        juce::MemoryBlock pluginState;
        if (pluginState.fromBase64Encoding(entry->getStringAttribute("state")) && pluginState.getSize() > 0)
        {
            plugin->getInstance().setStateInformation(pluginState.getData(), static_cast<int>(pluginState.getSize()));
        }
        m_engine.addSlot(std::move(plugin), entry->getStringAttribute("name", description.name), error);
    }
    m_engine.setActiveSlot(juce::jlimit(0, m_engine.getNumSlots() - 1, root->getIntAttribute("activeSlot", 0)));
    slotsChanged();
    if (onSlotsRestored)
    {
        onSlotsRestored();
    }
}
