#pragma once

#include <functional>
#include <map>
#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "HostSettings.h"
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
    ComparePanel(pluginlab::engine::MeasurementEngine& engine, HostSettings& settings);
    ~ComparePanel() override;

    // The panel shows the slots that the page "Plugins" loads. Asks that page for the editor window of a slot.
    std::function<void(int slotIndex)> onShowEditorOfSlot;
    // Told after the list of the audio files changed (the session is kept)
    std::function<void()> onFilesChanged;
    // The user chose another engine rate: the host does it (the plugins have to be prepared again, so the slots and files are removed;
    // the host closes the editors first and puts the dry slot back)
    std::function<void(double sampleRate)> onChangeEngineRate;

    // The list of the slots changed (a plugin was loaded or unloaded on the other page)
    void slotsChanged();

    // The list of the files changed from outside (an audio list was loaded)
    void filesChanged();

    void addFile(const juce::File& file);
    void addFiles(const juce::Array<juce::File>& files);

    // Loads an audio list file / saves the audio list (as the buttons do)
    void loadAudioListFile(const juce::File& file);
    void saveAudioListFile(const juce::File& file);

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

    void chooseFiles();
    void removeSelectedFile();
    void saveAudioList();
    void loadAudioList();
    void applyFileSettings();
    void fileSelectionChanged();
    void showSlotEditor();
    void slotSelectionChanged();
    void startOrStop();
    void showDeviceSettings();
    void chooseRenderFolder();
    void changeEngineRate();
    void setStatus(const juce::String& text);

    pluginlab::engine::MeasurementEngine& m_engine;
    HostSettings& m_settings;
    juce::AudioDeviceManager m_deviceManager;
    bool m_deviceCallbackAdded = false;
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_rateMismatch{false};
    double m_deviceRate = 0.0;
    juce::AudioBuffer<float> m_monoMix;

    std::unique_ptr<RenderThread> m_renderThread;
    std::unique_ptr<juce::FileChooser> m_chooser;

    juce::TextButton m_addFileButton{"Add audio files..."};
    juce::TextButton m_removeFileButton{"Remove file"};
    juce::TextButton m_saveListButton{"Save audio list..."};
    juce::TextButton m_loadListButton{"Load audio list..."};
    juce::Label m_passesLabel;
    juce::ComboBox m_passesBox;
    juce::Label m_regionLabel;
    juce::TextEditor m_regionStart;
    juce::TextEditor m_regionEnd;
    std::unique_ptr<ui::TextTableModel> m_fileModel;
    juce::TableListBox m_fileTable;

    juce::Label m_slotsLabel;
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
