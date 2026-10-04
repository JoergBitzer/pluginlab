#pragma once

#include <functional>
#include <map>
#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/engine/MeasurementEngine.h"
#include "pluginlab/ui/ParameterTableComponent.h"

namespace pluginlab::ui
{
class TextTableModel;
class PluginEditorWindow;
}

namespace pluginlab::host
{
class RenderThread;

// The second page of the host (W4): audio files with loops, plugin slots that run at the same time on the same audio, one audible
// slot (select a row or press 1 ... 9, the switch is a short crossfade), latency per slot (measured and reported), play through the
// audio device, and "Render to files" (one WAV per slot).
class ComparePanel : public juce::Component, private juce::AudioIODeviceCallback, private juce::Timer
{
public:
    ComparePanel();
    ~ComparePanel() override;

    // The panel asks for the description of the plugin that is selected on the Plugins page (to put another instance of it into a slot).
    std::function<bool(juce::PluginDescription&)> getSelectedPluginDescription;

    // For manual tests and screenshots (command line): adds a file and the dry slot
    void addFile(const juce::File& file);
    void addDrySlot();
    bool addSlotFromFile(const juce::File& pluginFile);

    // Starts the playback (as the Play button) and gives the text that tells what is going on (device, xruns).
    void startPlaying();
    void stopPlaying();
    juce::String describeEngine() const;
    bool isPlaying() const;

    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // called by the render thread (message thread)
    void renderFinished(const juce::String& message);

private:
    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
                                          int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void timerCallback() override;

    void chooseFile();
    void removeSelectedFile();
    void applyFileSettings();
    void fileSelectionChanged();
    void addSlotFromSelectedPlugin();
    void removeSelectedSlot();
    void showSlotEditor();
    void slotSelectionChanged();
    void startOrStop();
    void showDeviceSettings();
    void chooseRenderFolder();
    void changeEngineRate();
    void clearEverything();
    void closeSlotWindows();
    void setStatus(const juce::String& text);

    juce::AudioPluginFormatManager m_formatManager;
    pluginlab::engine::MeasurementEngine m_engine;
    juce::AudioDeviceManager m_deviceManager;
    bool m_deviceCallbackAdded = false;
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_rateMismatch{false};
    double m_deviceRate = 0.0;
    juce::AudioBuffer<float> m_monoMix;

    std::map<int, std::unique_ptr<ui::PluginEditorWindow>> m_slotWindows;
    std::unique_ptr<RenderThread> m_renderThread;
    std::unique_ptr<juce::FileChooser> m_chooser;

    juce::TextButton m_addFileButton{"Add audio file..."};
    juce::TextButton m_removeFileButton{"Remove file"};
    juce::Label m_passesLabel;
    juce::ComboBox m_passesBox;
    juce::Label m_regionLabel;
    juce::TextEditor m_regionStart;
    juce::TextEditor m_regionEnd;
    std::unique_ptr<ui::TextTableModel> m_fileModel;
    juce::TableListBox m_fileTable;

    juce::TextButton m_addDryButton{"Add dry slot"};
    juce::TextButton m_addPluginButton{"Add the plugin selected on the Plugins page"};
    juce::TextButton m_removeSlotButton{"Remove slot"};
    juce::TextButton m_slotEditorButton{"Show editor"};
    std::unique_ptr<ui::TextTableModel> m_slotModel;
    juce::TableListBox m_slotTable;
    ui::ParameterTableComponent m_parameters;

    juce::TextButton m_playButton{"Play"};
    juce::TextButton m_rewindButton{"Rewind"};
    juce::TextButton m_deviceButton{"Audio device..."};
    juce::TextButton m_renderButton{"Render to files..."};
    juce::Label m_rateLabel;
    juce::ComboBox m_rateBox;
    juce::Label m_crossfadeLabel;
    juce::Slider m_crossfadeSlider;
    juce::Label m_statusLabel;
};
}
