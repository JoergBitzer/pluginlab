#include "ComparePanel.h"

#include "pluginlab/engine/OfflineRenderer.h"
#include "pluginlab/hosting/PluginDisplayName.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/ui/GuiFormats.h"
#include "pluginlab/ui/PluginEditorWindow.h"
#include "pluginlab/ui/TextTableModel.h"

namespace pluginlab::host
{
namespace
{
constexpr double kDefaultRate = 48000.0;
constexpr int kEngineBlockSize = 512;
constexpr int kMargin = 8;
constexpr int kButtonHeight = 28;
constexpr int kRowHeight = 22;
constexpr int kHeaderHeight = 22;
constexpr int kFileTableHeight = 130;
constexpr int kSlotTableHeight = 150;
constexpr int kStatusHeight = 24;
constexpr int kTimerMs = 500;
constexpr int kMaximumKeySlots = 9;
constexpr double kMsPerSecond = 1000.0;
constexpr int kLabelWidth = 70;
constexpr int kSmallEditorWidth = 70;
constexpr int kSliderWidth = 220;
constexpr int kFileColumnNumber = 1;
constexpr int kFileColumnName = 2;
constexpr int kFileColumnLength = 3;
constexpr int kFileColumnPasses = 4;
constexpr int kFileColumnRegion = 5;
constexpr int kSlotColumnKey = 1;
constexpr int kSlotColumnName = 2;
constexpr int kSlotColumnMeasured = 3;
constexpr int kSlotColumnReported = 4;
constexpr int kSlotColumnCompensation = 5;
constexpr int kSlotColumnAudible = 6;
const char* const kAudioFileWildcard = "*.wav;*.flac;*.aiff;*.aif;*.mp3;*.ogg";
constexpr int kSecondsDecimals = 2;
constexpr int kMaximumCrossfadeMs = 100;
constexpr int kDefaultCrossfadeMs = 10;
constexpr int kLoopForEver = 0;
constexpr int kFirstPassesId = 1;
const int kPassesChoices[] = {kLoopForEver, 1, 2, 4, 8, 16};
constexpr double kRates[] = {44100.0, 48000.0, 96000.0};
constexpr int kDeviceSelectorWidth = 500;
constexpr int kDeviceSelectorHeight = 400;
}

// Renders the file list through all slots in a thread of its own and tells the panel when it is ready.
class RenderThread : public juce::Thread
{
public:
    RenderThread(ComparePanel& owner, pluginlab::engine::MeasurementEngine& engine, const juce::File& folder)
        : juce::Thread("OfflineRender"), m_owner(owner), m_engine(engine), m_folder(folder)
    {
    }

    ~RenderThread() override
    {
        stopThread(kStopTimeoutMs);
    }

    void run() override
    {
        const pluginlab::engine::OfflineRenderResult result = pluginlab::engine::renderOffline(m_engine, m_folder);
        juce::String message = result.message;
        if (result.ok)
        {
            message += " in " + m_folder.getFullPathName();
        }
        const juce::Component::SafePointer<ComparePanel> owner(&m_owner);
        juce::MessageManager::callAsync([owner, message]
                                        {
                                            if (owner != nullptr)
                                            {
                                                owner->renderFinished(message);
                                            }
                                        });
    }

private:
    static constexpr int kStopTimeoutMs = 600000;

    ComparePanel& m_owner;
    pluginlab::engine::MeasurementEngine& m_engine;
    juce::File m_folder;
};

ComparePanel::ComparePanel()
{
    ui::addGuiFormats(m_formatManager);
    m_engine.prepare(kDefaultRate, kEngineBlockSize);
    m_deviceManager.initialiseWithDefaultDevices(0, pluginlab::engine::MeasurementEngine::kChannels);
    setWantsKeyboardFocus(true);

    m_fileModel = std::make_unique<ui::TextTableModel>(
        [this] { return m_engine.getNumFiles(); },
        [this](int row, int column)
        {
            const pluginlab::engine::FileInfo info = m_engine.getFileInfo(row);
            if (column == kFileColumnNumber)
            {
                return juce::String(row + 1);
            }
            if (column == kFileColumnName)
            {
                return info.name;
            }
            if (column == kFileColumnLength)
            {
                return juce::String(info.lengthSeconds, kSecondsDecimals) + " s";
            }
            if (column == kFileColumnPasses)
            {
                if (info.passes == kLoopForEver)
                {
                    return juce::String("for ever");
                }
                return juce::String(info.passes);
            }
            if (column == kFileColumnRegion)
            {
                return juce::String(info.regionStartSeconds, kSecondsDecimals) + " - " + juce::String(info.regionEndSeconds, kSecondsDecimals) + " s";
            }
            return juce::String();
        },
        [this] { fileSelectionChanged(); });
    m_fileTable.setModel(m_fileModel.get());
    m_fileTable.setRowHeight(kRowHeight);
    m_fileTable.setHeaderHeight(kHeaderHeight);
    m_fileTable.getHeader().addColumn("#", kFileColumnNumber, 30);
    m_fileTable.getHeader().addColumn("Audio file", kFileColumnName, 300);
    m_fileTable.getHeader().addColumn("Length", kFileColumnLength, 90);
    m_fileTable.getHeader().addColumn("Passes", kFileColumnPasses, 80);
    m_fileTable.getHeader().addColumn("Loop region", kFileColumnRegion, 160);

    m_slotModel = std::make_unique<ui::TextTableModel>(
        [this] { return m_engine.getNumSlots(); },
        [this](int row, int column)
        {
            const pluginlab::engine::SlotInfo info = m_engine.getSlotInfo(row);
            if (column == kSlotColumnKey)
            {
                return juce::String(row + 1);
            }
            if (column == kSlotColumnName)
            {
                return info.name;
            }
            const bool columnNeedsPlugin = column == kSlotColumnMeasured || column == kSlotColumnReported;
            if (! info.hasPlugin && columnNeedsPlugin)
            {
                return juce::String();
            }
            if (column == kSlotColumnMeasured)
            {
                return juce::String(info.measuredLatency) + " samples";
            }
            if (column == kSlotColumnReported)
            {
                return juce::String(info.reportedLatency) + " samples";
            }
            if (column == kSlotColumnCompensation)
            {
                return juce::String(info.compensation) + " samples";
            }
            if (column == kSlotColumnAudible && row == m_engine.getActiveSlot())
            {
                return juce::String("audible");
            }
            return juce::String();
        },
        [this] { slotSelectionChanged(); });
    m_slotTable.setModel(m_slotModel.get());
    m_slotTable.setRowHeight(kRowHeight);
    m_slotTable.setHeaderHeight(kHeaderHeight);
    m_slotTable.getHeader().addColumn("Key", kSlotColumnKey, 40);
    m_slotTable.getHeader().addColumn("Slot", kSlotColumnName, 260);
    m_slotTable.getHeader().addColumn("Latency measured", kSlotColumnMeasured, 130);
    m_slotTable.getHeader().addColumn("Latency reported", kSlotColumnReported, 130);
    m_slotTable.getHeader().addColumn("Delay for alignment", kSlotColumnCompensation, 140);
    m_slotTable.getHeader().addColumn("", kSlotColumnAudible, 80);

    m_passesLabel.setText("Passes", juce::dontSendNotification);
    int id = kFirstPassesId;
    for (const int passes : kPassesChoices)
    {
        if (passes == kLoopForEver)
        {
            m_passesBox.addItem("for ever", id);
        }
        else
        {
            m_passesBox.addItem(juce::String(passes), id);
        }
        ++id;
    }
    m_passesBox.setSelectedId(kFirstPassesId + 1, juce::dontSendNotification); // 1 pass
    m_regionLabel.setText("Loop region (s)", juce::dontSendNotification);
    m_regionStart.setText("0", false);
    m_regionEnd.setText("0", false);
    m_regionStart.setTooltip("Start of the loop region in seconds (0 and 0: the whole file)");
    m_regionEnd.setTooltip("End of the loop region in seconds");
    m_regionStart.onReturnKey = [this] { applyFileSettings(); };
    m_regionEnd.onReturnKey = [this] { applyFileSettings(); };
    m_passesBox.onChange = [this] { applyFileSettings(); };

    m_addFileButton.onClick = [this] { chooseFile(); };
    m_removeFileButton.onClick = [this] { removeSelectedFile(); };
    m_addDryButton.onClick = [this] { addDrySlot(); };
    m_addPluginButton.onClick = [this] { addSlotFromSelectedPlugin(); };
    m_removeSlotButton.onClick = [this] { removeSelectedSlot(); };
    m_slotEditorButton.onClick = [this] { showSlotEditor(); };
    m_playButton.onClick = [this] { startOrStop(); };
    m_rewindButton.onClick = [this] { m_engine.restart(); };
    m_deviceButton.onClick = [this] { showDeviceSettings(); };
    m_renderButton.onClick = [this] { chooseRenderFolder(); };

    m_rateLabel.setText("Engine rate", juce::dontSendNotification);
    int rateId = 1;
    for (const double rate : kRates)
    {
        m_rateBox.addItem(juce::String(rate, 0) + " Hz", rateId);
        if (rate == kDefaultRate)
        {
            m_rateBox.setSelectedId(rateId, juce::dontSendNotification);
        }
        ++rateId;
    }
    m_rateBox.setTooltip("The sample rate of the engine and of all plugins. Changing it removes the files and slots.");
    m_rateBox.onChange = [this] { changeEngineRate(); };

    m_crossfadeLabel.setText("Crossfade (ms)", juce::dontSendNotification);
    m_crossfadeSlider.setRange(0.0, kMaximumCrossfadeMs, 1.0);
    m_crossfadeSlider.setValue(kDefaultCrossfadeMs, juce::dontSendNotification);
    m_crossfadeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_crossfadeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, kSmallEditorWidth, kButtonHeight);
    m_crossfadeSlider.onValueChange = [this] { m_engine.setCrossfadeMs(static_cast<float>(m_crossfadeSlider.getValue())); };

    for (juce::Component* component : std::initializer_list<juce::Component*>{
             &m_addFileButton, &m_removeFileButton, &m_passesLabel, &m_passesBox, &m_regionLabel, &m_regionStart, &m_regionEnd, &m_fileTable,
             &m_addDryButton, &m_addPluginButton, &m_removeSlotButton, &m_slotEditorButton, &m_slotTable, &m_parameters, &m_playButton,
             &m_rewindButton, &m_deviceButton, &m_renderButton, &m_rateLabel, &m_rateBox, &m_crossfadeLabel, &m_crossfadeSlider,
             &m_statusLabel})
    {
        addAndMakeVisible(component);
    }
    setStatus("Add audio files and slots (a dry slot and plugins), then press Play. Keys 1 ... 9 switch the audible slot.");
    startTimer(kTimerMs);
}

ComparePanel::~ComparePanel()
{
    stopTimer();
    stopPlaying();
    m_renderThread.reset();
    m_parameters.setPlugin(nullptr);
    closeSlotWindows(); // the editors before the plugins
    m_fileTable.setModel(nullptr);
    m_slotTable.setModel(nullptr);
    m_engine.clearSlots();
}

void ComparePanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);

    juce::Rectangle<int> row = area.removeFromTop(kButtonHeight);
    m_addFileButton.setBounds(row.removeFromLeft(150));
    row.removeFromLeft(kMargin);
    m_removeFileButton.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(kMargin * 3);
    m_passesLabel.setBounds(row.removeFromLeft(kLabelWidth));
    m_passesBox.setBounds(row.removeFromLeft(kSmallEditorWidth + 20));
    row.removeFromLeft(kMargin * 3);
    m_regionLabel.setBounds(row.removeFromLeft(kLabelWidth + 40));
    m_regionStart.setBounds(row.removeFromLeft(kSmallEditorWidth));
    row.removeFromLeft(kMargin);
    m_regionEnd.setBounds(row.removeFromLeft(kSmallEditorWidth));
    area.removeFromTop(kMargin);
    m_fileTable.setBounds(area.removeFromTop(kFileTableHeight));
    area.removeFromTop(kMargin);

    row = area.removeFromTop(kButtonHeight);
    m_addDryButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(kMargin);
    m_addPluginButton.setBounds(row.removeFromLeft(330));
    row.removeFromLeft(kMargin);
    m_removeSlotButton.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(kMargin);
    m_slotEditorButton.setBounds(row.removeFromLeft(110));
    area.removeFromTop(kMargin);
    m_slotTable.setBounds(area.removeFromTop(kSlotTableHeight));
    area.removeFromTop(kMargin);

    row = area.removeFromTop(kButtonHeight);
    m_playButton.setBounds(row.removeFromLeft(90));
    row.removeFromLeft(kMargin);
    m_rewindButton.setBounds(row.removeFromLeft(90));
    row.removeFromLeft(kMargin);
    m_deviceButton.setBounds(row.removeFromLeft(130));
    row.removeFromLeft(kMargin);
    m_renderButton.setBounds(row.removeFromLeft(150));
    row.removeFromLeft(kMargin * 3);
    m_rateLabel.setBounds(row.removeFromLeft(kLabelWidth));
    m_rateBox.setBounds(row.removeFromLeft(100));
    row.removeFromLeft(kMargin * 3);
    m_crossfadeLabel.setBounds(row.removeFromLeft(kLabelWidth + 30));
    m_crossfadeSlider.setBounds(row.removeFromLeft(kSliderWidth));
    area.removeFromTop(kMargin);

    m_statusLabel.setBounds(area.removeFromBottom(kStatusHeight));
    m_parameters.setBounds(area);
}

bool ComparePanel::keyPressed(const juce::KeyPress& key)
{
    const juce::juce_wchar character = key.getTextCharacter();
    if (character >= '1' && character < '1' + kMaximumKeySlots)
    {
        const int slot = static_cast<int>(character - '1');
        if (slot < m_engine.getNumSlots())
        {
            m_slotTable.selectRow(slot);
            return true;
        }
    }
    return false;
}

void ComparePanel::setStatus(const juce::String& text)
{
    m_statusLabel.setText(text, juce::dontSendNotification);
}

juce::String ComparePanel::describeEngine() const
{
    const int latency = m_engine.getLatencyOfEngine();
    juce::String text = "Engine " + juce::String(m_engine.getSampleRate(), 0) + " Hz, latency of the slowest slot " + juce::String(latency)
                      + " samples (" + juce::String(latency * kMsPerSecond / m_engine.getSampleRate(), 1) + " ms), all slots are aligned to it.";
    juce::AudioIODevice* device = m_deviceManager.getCurrentAudioDevice();
    if (device != nullptr && m_playing)
    {
        text += " Playing on " + device->getName() + ", xruns: " + juce::String(device->getXRunCount());
    }
    return text;
}

void ComparePanel::timerCallback()
{
    if (m_rateMismatch.exchange(false))
    {
        stopPlaying();
        setStatus("The audio device runs at " + juce::String(m_deviceRate, 0) + " Hz, the engine at " + juce::String(m_engine.getSampleRate(), 0)
                  + " Hz: set the same rate in the audio device settings or change the engine rate.");
    }
    else if (m_playing && m_renderThread == nullptr)
    {
        setStatus(describeEngine());
    }
    m_slotTable.repaint();
}

// ---- files ----

void ComparePanel::addFile(const juce::File& file)
{
    juce::String error;
    if (! m_engine.addFile(file, 1, error))
    {
        setStatus(error);
        return;
    }
    m_fileTable.updateContent();
    m_fileTable.selectRow(m_engine.getNumFiles() - 1);
    setStatus("Added " + file.getFileName());
}

void ComparePanel::chooseFile()
{
    m_chooser = std::make_unique<juce::FileChooser>("Audio file", juce::File(), kAudioFileWildcard);
    m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                           [this](const juce::FileChooser& chooser)
                           {
                               if (chooser.getResult() != juce::File())
                               {
                                   addFile(chooser.getResult());
                               }
                           });
}

void ComparePanel::removeSelectedFile()
{
    const int row = m_fileTable.getSelectedRow();
    if (row < 0)
    {
        return;
    }
    m_engine.removeFile(row);
    m_fileTable.updateContent();
    m_fileTable.deselectAllRows();
}

void ComparePanel::fileSelectionChanged()
{
    const int row = m_fileTable.getSelectedRow();
    if (row < 0)
    {
        return;
    }
    const pluginlab::engine::FileInfo info = m_engine.getFileInfo(row);
    int id = kFirstPassesId;
    for (const int passes : kPassesChoices)
    {
        if (passes == info.passes)
        {
            m_passesBox.setSelectedId(id, juce::dontSendNotification);
        }
        ++id;
    }
    m_regionStart.setText(juce::String(info.regionStartSeconds, kSecondsDecimals), false);
    m_regionEnd.setText(juce::String(info.regionEndSeconds, kSecondsDecimals), false);
}

void ComparePanel::applyFileSettings()
{
    const int row = m_fileTable.getSelectedRow();
    if (row < 0)
    {
        return;
    }
    const int index = m_passesBox.getSelectedId() - kFirstPassesId;
    if (index >= 0 && index < static_cast<int>(sizeof(kPassesChoices) / sizeof(kPassesChoices[0])))
    {
        m_engine.setFilePasses(row, kPassesChoices[index]);
    }
    m_engine.setFileRegionSeconds(row, m_regionStart.getText().getDoubleValue(), m_regionEnd.getText().getDoubleValue());
    m_fileTable.repaint();
    fileSelectionChanged();
}

// ---- slots ----

void ComparePanel::addDrySlot()
{
    juce::String error;
    if (m_engine.addSlot(nullptr, "dry (no plugin)", error) < 0)
    {
        setStatus(error);
        return;
    }
    m_slotTable.updateContent();
    m_slotTable.selectRow(m_engine.getNumSlots() - 1);
}

bool ComparePanel::addSlotFromFile(const juce::File& pluginFile)
{
    juce::AudioPluginFormatManager manager;
    ui::addGuiFormats(manager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(manager, pluginFile);
    if (scan.descriptions.isEmpty())
    {
        setStatus("No plugin in " + pluginFile.getFullPathName() + ": " + scan.message);
        return false;
    }
    juce::String error;
    std::unique_ptr<hosting::HostedPlugin> plugin =
        hosting::HostedPlugin::load(m_formatManager, scan.descriptions[0], m_engine.getSampleRate(), kEngineBlockSize, error);
    if (plugin == nullptr || m_engine.addSlot(std::move(plugin), hosting::getDisplayName(scan.descriptions[0]), error) < 0)
    {
        setStatus(error);
        return false;
    }
    m_slotTable.updateContent();
    m_slotTable.selectRow(m_engine.getNumSlots() - 1);
    return true;
}

void ComparePanel::addSlotFromSelectedPlugin()
{
    juce::PluginDescription description;
    if (! getSelectedPluginDescription || ! getSelectedPluginDescription(description))
    {
        setStatus("Load a plugin on the Plugins page and select it there first.");
        return;
    }
    juce::String error;
    std::unique_ptr<hosting::HostedPlugin> plugin =
        hosting::HostedPlugin::load(m_formatManager, description, m_engine.getSampleRate(), kEngineBlockSize, error);
    if (plugin == nullptr)
    {
        setStatus("Cannot load " + hosting::getDisplayName(description) + ": " + error);
        return;
    }
    if (m_engine.addSlot(std::move(plugin), hosting::getDisplayName(description), error) < 0)
    {
        setStatus(error);
        return;
    }
    m_slotTable.updateContent();
    m_slotTable.selectRow(m_engine.getNumSlots() - 1);
    setStatus("Added " + hosting::getDisplayName(description) + ". " + describeEngine());
}

void ComparePanel::removeSelectedSlot()
{
    const int row = m_slotTable.getSelectedRow();
    if (row < 0)
    {
        return;
    }
    m_parameters.setPlugin(nullptr);
    closeSlotWindows(); // slot numbers change: the windows of all slots go
    m_engine.removeSlot(row);
    m_slotTable.updateContent();
    m_slotTable.deselectAllRows();
}

void ComparePanel::closeSlotWindows()
{
    m_slotWindows.clear();
}

void ComparePanel::slotSelectionChanged()
{
    const int row = m_slotTable.getSelectedRow();
    if (row < 0)
    {
        m_parameters.setPlugin(nullptr);
        return;
    }
    m_engine.setActiveSlot(row);
    m_parameters.setPlugin(m_engine.getPlugin(row));
}

void ComparePanel::showSlotEditor()
{
    const int row = m_slotTable.getSelectedRow();
    hosting::HostedPlugin* plugin = m_engine.getPlugin(row);
    if (plugin == nullptr)
    {
        setStatus("Select a slot with a plugin first.");
        return;
    }
    const auto existing = m_slotWindows.find(row);
    if (existing != m_slotWindows.end())
    {
        existing->second->toFront(true);
        return;
    }
    const juce::Component::SafePointer<ComparePanel> self(this);
    m_slotWindows[row] = std::make_unique<ui::PluginEditorWindow>(
        plugin->getInstance(), hosting::getDisplayName(plugin->getDescription()),
        [self, row]
        {
            juce::MessageManager::callAsync([self, row]
                                            {
                                                if (self != nullptr)
                                                {
                                                    self->m_slotWindows.erase(row);
                                                }
                                            });
        });
}

void ComparePanel::changeEngineRate()
{
    const int index = m_rateBox.getSelectedId() - 1;
    if (index < 0 || index >= static_cast<int>(sizeof(kRates) / sizeof(kRates[0])))
    {
        return;
    }
    stopPlaying();
    clearEverything();
    m_engine.prepare(kRates[index], kEngineBlockSize);
    setStatus("Engine rate " + juce::String(kRates[index], 0) + " Hz: add the files and slots again.");
}

void ComparePanel::clearEverything()
{
    m_parameters.setPlugin(nullptr);
    closeSlotWindows();
    m_engine.clearSlots();
    m_engine.clearFiles();
    m_slotTable.updateContent();
    m_fileTable.updateContent();
}

// ---- audio ----

bool ComparePanel::isPlaying() const
{
    return m_playing;
}

void ComparePanel::startOrStop()
{
    if (m_playing)
    {
        stopPlaying();
        setStatus("Stopped.");
        return;
    }
    startPlaying();
}

void ComparePanel::startPlaying()
{
    if (m_renderThread != nullptr)
    {
        setStatus("The rendering is not finished.");
        return;
    }
    if (m_engine.getNumSlots() == 0 || m_engine.getNumFiles() == 0)
    {
        setStatus("Add at least one audio file and one slot first.");
        return;
    }
    juce::AudioDeviceManager::AudioDeviceSetup setup = m_deviceManager.getAudioDeviceSetup();
    setup.sampleRate = m_engine.getSampleRate();
    m_deviceManager.setAudioDeviceSetup(setup, true); // asks for the rate of the engine; the device may refuse
    m_deviceManager.addAudioCallback(this);
    m_deviceCallbackAdded = true;
    m_playing = true;
    m_playButton.setButtonText("Stop");
}

void ComparePanel::stopPlaying()
{
    if (m_deviceCallbackAdded)
    {
        m_deviceManager.removeAudioCallback(this);
        m_deviceCallbackAdded = false;
    }
    m_playing = false;
    m_playButton.setButtonText("Play");
}

void ComparePanel::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    m_deviceRate = device->getCurrentSampleRate();
    if (std::abs(m_deviceRate - m_engine.getSampleRate()) > 1.0)
    {
        m_rateMismatch = true; // the timer stops the playback (not allowed here)
    }
    m_monoMix.setSize(pluginlab::engine::MeasurementEngine::kChannels, device->getCurrentBufferSizeSamples() * 2);
}

void ComparePanel::audioDeviceStopped()
{
}

void ComparePanel::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);
    constexpr int kChannels = pluginlab::engine::MeasurementEngine::kChannels;
    if (numOutputChannels <= 0 || m_rateMismatch)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
        }
        return;
    }
    if (numOutputChannels >= kChannels)
    {
        juce::AudioBuffer<float> output(outputChannelData, kChannels, numSamples);
        m_engine.processBlock(output);
        for (int channel = kChannels; channel < numOutputChannels; ++channel)
        {
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
        }
        return;
    }
    if (numSamples > m_monoMix.getNumSamples())
    {
        juce::FloatVectorOperations::clear(outputChannelData[0], numSamples);
        return;
    }
    juce::AudioBuffer<float> mix(m_monoMix.getArrayOfWritePointers(), kChannels, numSamples);
    m_engine.processBlock(mix);
    juce::FloatVectorOperations::copy(outputChannelData[0], mix.getReadPointer(0), numSamples);
}

void ComparePanel::showDeviceSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(m_deviceManager, 0, 0, 0, pluginlab::engine::MeasurementEngine::kChannels,
                                                            false, false, true, false);
    selector->setSize(kDeviceSelectorWidth, kDeviceSelectorHeight);
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector);
    options.dialogTitle = "Audio device";
    options.componentToCentreAround = this;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

// ---- rendering ----

void ComparePanel::chooseRenderFolder()
{
    if (m_engine.getNumSlots() == 0 || m_engine.getNumFiles() == 0)
    {
        setStatus("Add at least one audio file and one slot first.");
        return;
    }
    m_chooser = std::make_unique<juce::FileChooser>("Folder for the rendered files");
    m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                           [this](const juce::FileChooser& chooser)
                           {
                               if (chooser.getResult() == juce::File())
                               {
                                   return;
                               }
                               stopPlaying();
                               setStatus("Rendering ...");
                               m_renderButton.setEnabled(false);
                               m_playButton.setEnabled(false);
                               m_renderThread = std::make_unique<RenderThread>(*this, m_engine, chooser.getResult());
                               m_renderThread->startThread();
                           });
}

void ComparePanel::renderFinished(const juce::String& message)
{
    m_renderThread.reset();
    m_renderButton.setEnabled(true);
    m_playButton.setEnabled(true);
    setStatus(message);
}
}
