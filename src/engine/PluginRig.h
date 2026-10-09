#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/engine/ChannelAdapter.h"
#include "pluginlab/engine/FingerprintSettings.h"

// Internal header of pluginlab_engine: one instance of a plugin with what is needed to run audio through it, and the careful way of rendering with
// a setting (the delivery protocol of docs/design/W5-fingerprint.md). Shared by the fingerprint and the plugin device of the measurement units.
namespace pluginlab::engine::detail
{
constexpr double kReferenceRate = 48000.0;
constexpr int kReferenceBlock = 512;
constexpr int kReferenceSeed = 7;
constexpr int kSecondSeed = 11;      // the second channel of the noise with L != R
constexpr double kMillisecondsPerSecond = 1000.0;

inline std::vector<float> makeNoise(int length, double level, int seed)
{
    juce::Random random(seed);
    std::vector<float> noise(static_cast<size_t>(length));
    for (float& sample : noise)
    {
        sample = static_cast<float>(level) * (random.nextFloat() * 2.0f - 1.0f);
    }
    return noise;
}

// A signal with one vector per channel. A plugin with more channels than the signal gets the last channel again.
using Signal = std::vector<std::vector<float>>;

// The same noise on every channel (L = R)
inline Signal makeSameNoise(int length, double level)
{
    return Signal{makeNoise(length, level, kReferenceSeed)};
}

// Different, uncorrelated noise on the first two channels (L != R)
inline Signal makeDifferentNoise(int length, double level)
{
    return Signal{makeNoise(length, level, kReferenceSeed), makeNoise(length, level, kSecondSeed)};
}

inline Signal makeSilence(int length)
{
    return Signal{std::vector<float>(static_cast<size_t>(length), 0.0f)};
}

inline size_t getLength(const Signal& signal)
{
    if (signal.empty())
    {
        return 0;
    }
    return signal.front().size();
}

// The values a setting is first set to before it is set itself: another value for every parameter (a plugin that reads a parameter
// only when it changes cannot miss the setting)
inline std::vector<float> makePokeValues(const std::vector<float>& setting, const FingerprintSettings& settings)
{
    std::vector<float> other = setting;
    const float middle = 0.5f;
    for (float& value : other)
    {
        if (value > middle)
        {
            value -= static_cast<float>(settings.pokeDistance);
        }
        else
        {
            value += static_cast<float>(settings.pokeDistance);
        }
    }
    return other;
}

// One instance of the plugin with what is needed to run audio through it.
class Rig
{
public:
    Rig(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate, int blockSize,
        const FingerprintSettings& settings)
        : m_sampleRate(sampleRate), m_blockSize(blockSize), m_settings(settings)
    {
        juce::String error;
        m_instance = formatManager.createPluginInstance(description, sampleRate, blockSize, error);
        if (m_instance == nullptr)
        {
            m_error = error;
            return;
        }
        m_channels = ChannelAdapter::chooseLayout(*m_instance, 2);
    }

    bool isValid() const
    {
        return m_instance != nullptr && m_channels > 0;
    }

    const juce::String& getError() const
    {
        return m_error;
    }

    juce::AudioPluginInstance& get()
    {
        return *m_instance;
    }

    int getChannels() const
    {
        return m_channels;
    }

    void prepare()
    {
        m_instance->setPlayConfigDetails(m_channels, m_channels, m_sampleRate, m_blockSize);
        m_instance->prepareToPlay(m_sampleRate, m_blockSize);
    }

    // The offline flag (AudioProcessor::setNonRealtime, VST3 processMode kOffline): before prepare()
    void setOffline(bool offline)
    {
        m_instance->setNonRealtime(offline);
    }

    // From now on every block is followed by the message loop running until the wall-clock time equals the audio time (real-time pace)
    void startPacing()
    {
        m_paced = true;
        m_pacingStartMs = juce::Time::getMillisecondCounterHiRes();
        m_pacedSamples = 0;
    }

    void apply(const std::vector<float>& setting)
    {
        const juce::Array<juce::AudioProcessorParameter*>& parameters = m_instance->getParameters();
        for (int index = 0; index < parameters.size() && index < static_cast<int>(setting.size()); ++index)
        {
            parameters[index]->setValue(setting[static_cast<size_t>(index)]);
        }
    }

    void set(int index, float value)
    {
        const juce::Array<juce::AudioProcessorParameter*>& parameters = m_instance->getParameters();
        if (index >= 0 && index < parameters.size())
        {
            parameters[index]->setValue(value);
        }
    }

    // All main-bus output channels. Input channel c of the plugin gets channel c of the signal (or its last channel); further channels of
    // the buffer (a side chain that stays on) get silence. Remembers whether an output sample was not finite.
    Signal process(const Signal& input)
    {
        const size_t length = getLength(input);
        Signal output(static_cast<size_t>(m_channels));
        for (std::vector<float>& channel : output)
        {
            channel.reserve(length);
        }
        const int bufferChannels = std::max(m_channels, ChannelAdapter::getProcessingChannels(*m_instance));
        juce::AudioBuffer<float> buffer(bufferChannels, m_blockSize);
        juce::MidiBuffer midi;
        for (size_t start = 0; start < length; start += static_cast<size_t>(m_blockSize))
        {
            const int count = static_cast<int>(std::min(static_cast<size_t>(m_blockSize), length - start));
            juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), bufferChannels, count);
            block.clear();
            for (int channel = 0; channel < m_channels; ++channel)
            {
                const std::vector<float>& source = input[std::min(static_cast<size_t>(channel), input.size() - 1)];
                for (int sample = 0; sample < count; ++sample)
                {
                    block.setSample(channel, sample, source[start + static_cast<size_t>(sample)]);
                }
            }
            m_instance->processBlock(block, midi);
            for (int channel = 0; channel < m_channels; ++channel)
            {
                for (int sample = 0; sample < count; ++sample)
                {
                    const float value = block.getSample(channel, sample);
                    if (! std::isfinite(value))
                    {
                        m_sawNonFinite = true;
                    }
                    output[static_cast<size_t>(channel)].push_back(value);
                }
            }
            if (m_paced)
            {
                waitForAudioTime(count);
            }
        }
        return output;
    }

    bool sawNonFinite() const
    {
        return m_sawNonFinite;
    }

    void settleFor(double seconds)
    {
        process(makeSilence(static_cast<int>(m_sampleRate * seconds)));
    }

    void settle()
    {
        process(makeSilence(static_cast<int>(m_sampleRate * m_settings.settleSeconds)));
    }

private:
    // Lets the message loop run (timers, async updates of the plugin) until the wall clock has caught up with the audio processed since
    // startPacing(); without a message thread (or without modal loops) the thread just waits
    void waitForAudioTime(int samples)
    {
        m_pacedSamples += samples;
        const double targetMs = m_pacingStartMs + kMillisecondsPerSecond * static_cast<double>(m_pacedSamples) / m_sampleRate;
        juce::MessageManager* messages = juce::MessageManager::getInstanceWithoutCreating();
        for (double now = juce::Time::getMillisecondCounterHiRes(); now < targetMs; now = juce::Time::getMillisecondCounterHiRes())
        {
            const int remainingMs = std::max(1, static_cast<int>(targetMs - now));
#if JUCE_MODAL_LOOPS_PERMITTED
            if (messages != nullptr && messages->isThisTheMessageThread())
            {
                messages->runDispatchLoopUntil(remainingMs);
                continue;
            }
#endif
            juce::ignoreUnused(messages);
            juce::Thread::sleep(remainingMs);
        }
    }

    std::unique_ptr<juce::AudioPluginInstance> m_instance;
    double m_sampleRate;
    int m_blockSize;
    const FingerprintSettings& m_settings;
    int m_channels = 0;
    bool m_sawNonFinite = false;
    bool m_paced = false;
    double m_pacingStartMs = 0.0;
    juce::int64 m_pacedSamples = 0;
    juce::String m_error;
};

// How a render runs: as fast as possible (all measurements), with the offline flag, or at real-time pace with the message loop running
enum class RenderMode
{
    Fast,
    Offline,
    Paced
};

// Everything needed to make instances and to render with a setting.
class Bench
{
public:
    Bench(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const FingerprintSettings& settings)
        : m_formatManager(formatManager), m_description(description), m_settings(settings)
    {
    }

    std::unique_ptr<Rig> make(double sampleRate, int blockSize) const
    {
        return std::make_unique<Rig>(m_formatManager, m_description, sampleRate, blockSize, m_settings);
    }

    // The careful way that all measurements use (most conservative first, LESSONS_LEARNED §2): a fresh instance, prepared; every parameter is
    // first set to another value and one block of noise of the reference length runs (the same input history for every block size), then
    // the setting is set; settled; then the input.
    Signal render(double sampleRate, int blockSize, const std::vector<float>& setting, const Signal& input, RenderMode mode = RenderMode::Fast) const
    {
        const std::unique_ptr<Rig> rig = make(sampleRate, blockSize);
        if (! rig->isValid())
        {
            return {};
        }
        if (mode == RenderMode::Offline)
        {
            rig->setOffline(true);
        }
        rig->prepare();
        if (mode == RenderMode::Paced)
        {
            rig->startPacing();
        }
        rig->apply(makePokeValues(setting, m_settings));
        rig->process(makeDifferentNoise(kReferenceBlock, m_settings.noiseLevel));
        rig->apply(setting);
        rig->settle();
        return rig->process(input);
    }

    // a fresh instance, parameters set after prepare (no poke), settled, then the input
    Signal renderAfterPrepare(double sampleRate, int blockSize, const std::vector<float>& setting, const Signal& input) const
    {
        const std::unique_ptr<Rig> rig = make(sampleRate, blockSize);
        if (! rig->isValid())
        {
            return {};
        }
        rig->prepare();
        rig->apply(setting);
        rig->settle();
        return rig->process(input);
    }

private:
    juce::AudioPluginFormatManager& m_formatManager;
    juce::PluginDescription m_description;
    const FingerprintSettings& m_settings;
};
}
