#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::engine
{
// The real-time transfer function display (W10.3, docs/design/W10-plan.md 3.3): the magnitude response of every slot while the user turns
// the knobs of the playing instances. Each slot gets a measuring instance of its own (not the one that plays, so the audio is not touched);
// a setting handed in is rendered on the slot's thread through the fast render path (W9.3: the setting, settling, an impulse, its spectrum),
// at most updatesPerSecond times per second, and only when the setting changed (a knob that does not move costs nothing). The magnitude
// response is valid for LTI plugins only: when the knobs rest for classifyAfterSeconds, a second measuring instance checks the system class
// of that setting (W8.1) on another thread, so that the check never delays a curve.
//
// Threads: setSlots, setSetting and getResponse are called from one thread (the message thread); the instances are created there.
struct LiveResponseOptions
{
    double updatesPerSecond = 5.0;     // per slot (the author: 5 per changed slot)
    double classifyAfterSeconds = 1.0; // the knobs at rest this long: the system class of the setting is checked
    double lowestHz = 20.0;
    double highestHz = 20000.0;        // at most 0.45 of the sample rate
    double perOctave = 12.0;
};

struct LiveSlotResponse
{
    int generation = 0;              // counts the curves of the slot (0: none yet)
    std::vector<double> magnitudeDb; // on getFrequencies()
    double renderSeconds = 0.0;      // the time of the last render
    bool classified = false;         // a system class is known (of the last setting at rest)
    bool lti = true;
    juce::String systemClass;        // its name
    juce::String reason;             // why it is not LTI
    bool classOfCurrentSetting = false; // the class was checked for the setting of the last curve
    int classifications = 0;
    juce::String error;              // the measuring instance could not be created
};

class LiveResponseMonitor
{
public:
    LiveResponseMonitor(juce::AudioPluginFormatManager& formatManager, double sampleRate, const LiveResponseOptions& options = {});
    ~LiveResponseMonitor();

    const std::vector<double>& getFrequencies() const;
    double getSampleRate() const;
    const LiveResponseOptions& getOptions() const;

    // The slots (index = slot of the engine): a description gets measuring instances; nullopt is the dry slot (a flat curve, no instance).
    // A slot with the same plugin as before keeps its instances.
    void setSlots(const std::vector<std::optional<juce::PluginDescription>>& slots);
    int getNumSlots() const;

    // The current normalised values of the slot's playing instance; rendered on the slot's thread if they differ from the last ones
    void setSetting(int slot, const std::vector<float>& setting);

    LiveSlotResponse getResponse(int slot) const;

private:
    class SlotWorker;
    juce::AudioPluginFormatManager& m_formatManager;
    double m_sampleRate;
    LiveResponseOptions m_options;
    struct Shared;
    std::shared_ptr<Shared> m_shared;
    std::vector<std::unique_ptr<SlotWorker>> m_slots;
    std::vector<juce::String> m_identifiers;
};

// The deviation of a slot's curve from the master's: the largest and the rms of the dB difference over the frequencies where at least one of
// the two is above ignoreBelowDb (in a stop band both curves are noise)
struct LiveDeviation
{
    double largestDb = 0.0;
    double rmsDb = 0.0;
};

LiveDeviation getLiveDeviation(const std::vector<double>& masterDb, const std::vector<double>& slotDb, double ignoreBelowDb = -60.0);
}
