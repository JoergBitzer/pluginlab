#pragma once

#include <algorithm>
#include <atomic>
#include <mutex>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/engine/ChannelAdapter.h"
#include "pluginlab/engine/FixedPlayHead.h"
#include "pluginlab/engine/FingerprintSettings.h"

// Internal header of pluginlab_engine: one instance of a plugin with what is needed to run audio through it, and the careful way of rendering with
// a setting (the delivery protocol of docs/design/W5-fingerprint.md). Shared by the fingerprint and the plugin device of the measurement units.
namespace pluginlab::engine::detail
{
constexpr double kReferenceRate = 48000.0;
constexpr int kReferenceBlock = 512;
constexpr double kTailChunkSeconds = 0.05;      // the waits until silent go on in chunks of this length
constexpr double kTailProbeRate = 48000.0;      // the impulse response that sets the hold
constexpr double kTailHoldMarginSeconds = 0.05; // the hold: the longest silent gap in that response plus this

// How long the waits of one plugin hold the silence before they stop, and their limit (shared by the instances of a bench)
struct TailWait
{
    TailWait(double limit, double hold)
        : limitSeconds(limit), holdSeconds(hold)
    {
    }

    std::atomic<double> limitSeconds;
    std::atomic<double> holdSeconds;
};
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
        m_playHead.setSampleRate(sampleRate);
        m_instance->setPlayHead(&m_playHead); // a tempo and a position for tempo-synced plugins (W11.1a)
    }

    ~Rig()
    {
        if (m_instance != nullptr)
        {
            m_instance->setPlayHead(nullptr);
        }
    }

    // the tempo the plugin sees (FixedPlayHead::kDefaultTempoBpm unless set)
    void setTempo(double tempoBpm)
    {
        m_playHead.setTempo(tempoBpm);
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
        m_playHead.reset();
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
            m_playHead.advance(count);
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

    // settleSeconds of silence, then on until the output has decayed (the poke's noise or an earlier signal must not ring into what follows:
    // a reverb, a delay, a compressor's release). The hold and the limit come from the bench (TailWait): the hold from the plugin's impulse
    // response, the limit drops to settleSeconds once a plugin's output did not decay (noise of its own).
    void settle()
    {
        double limit = m_settings.tailMaximumSeconds;
        double hold = m_settings.tailHoldSeconds;
        if (m_tailWait != nullptr)
        {
            limit = m_tailWait->limitSeconds.load();
            hold = m_tailWait->holdSeconds.load();
        }
        bool decayed = true;
        settleUntilSilent(m_settings.settleSeconds, decayed, limit, hold);
        if (! decayed && m_tailWait != nullptr)
        {
            m_tailWait->limitSeconds.store(m_settings.settleSeconds);
        }
    }

    void setTailWait(std::shared_ptr<TailWait> wait)
    {
        m_tailWait = std::move(wait);
    }

    // Silence for at least minimumSeconds, then on until the output has decayed: every sample below tailSilenceDbfs for holdSeconds (counted
    // from the start, so that a plugin that is silent at once stops after minimumSeconds; the tail of a reverb or a delay, the release of a
    // compressor), at most maximumSeconds. The silence after minimumSeconds goes in chunks of a fixed length (the same samples for every block
    // size: a free-running LFO stays in step). Returns the seconds of silence processed; decayed tells whether the output got that quiet.
    // maximumSeconds and holdSeconds < 0: the settings' tailMaximumSeconds and tailHoldSeconds.
    double settleUntilSilent(double minimumSeconds, bool& decayed, double maximumSeconds = -1.0, double holdSeconds = -1.0)
    {
        if (maximumSeconds < 0.0)
        {
            maximumSeconds = m_settings.tailMaximumSeconds;
        }
        if (holdSeconds < 0.0)
        {
            holdSeconds = m_settings.tailHoldSeconds;
        }
        const float silence = static_cast<float>(std::pow(10.0, m_settings.tailSilenceDbfs / 20.0));
        const int holdSamples = static_cast<int>(m_sampleRate * holdSeconds);
        const int minimumSamples = static_cast<int>(m_sampleRate * minimumSeconds);
        const int maximumSamples = std::max(minimumSamples, static_cast<int>(m_sampleRate * maximumSeconds));
        const int chunk = std::max(1, static_cast<int>(m_sampleRate * kTailChunkSeconds));
        int quietSamples = 0;
        int processed = 0;
        decayed = false;
        int length = minimumSamples;
        while (true)
        {
            const Signal output = process(makeSilence(length));
            processed += length;
            for (size_t sample = 0; sample < static_cast<size_t>(length); ++sample)
            {
                float largest = 0.0f;
                for (const std::vector<float>& channel : output)
                {
                    largest = std::max(largest, std::abs(channel[sample]));
                }
                if (largest < silence)
                {
                    ++quietSamples;
                }
                else
                {
                    quietSamples = 0;
                }
            }
            if (quietSamples >= holdSamples)
            {
                decayed = true;
                break;
            }
            if (processed >= maximumSamples)
            {
                break;
            }
            length = chunk;
        }
        return processed / m_sampleRate;
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

    FixedPlayHead m_playHead; // before the instance: it outlives it
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
    std::shared_ptr<TailWait> m_tailWait;
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
        std::unique_ptr<Rig> rig = std::make_unique<Rig>(m_formatManager, m_description, sampleRate, blockSize, m_settings);
        rig->setTailWait(getTailWait());
        return rig;
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
    // The waits of this plugin: the hold from its impulse response at the defaults (measured once, on the first instance): the longest silent
    // gap inside the response (a delay's echo spacing) plus a margin, tailHoldSeconds if it did not decay within twice that
    std::shared_ptr<TailWait> getTailWait() const
    {
        std::call_once(m_tailOnce, [this]
        {
            m_tailWait->holdSeconds.store(measureHold());
        });
        return m_tailWait;
    }

    double measureHold() const
    {
        Rig rig(m_formatManager, m_description, kTailProbeRate, kReferenceBlock, m_settings);
        if (! rig.isValid())
        {
            return m_settings.tailHoldSeconds;
        }
        rig.prepare();
        const int length = static_cast<int>(2.0 * m_settings.tailHoldSeconds * kTailProbeRate);
        Signal impulse = makeSilence(length);
        for (std::vector<float>& channel : impulse)
        {
            channel.front() = 1.0f;
        }
        const Signal output = rig.process(impulse);
        const float silence = static_cast<float>(std::pow(10.0, m_settings.tailSilenceDbfs / 20.0));
        int lastLoud = -1;
        int longestGap = 0;
        for (int sample = 0; sample < length; ++sample)
        {
            float largest = 0.0f;
            for (const std::vector<float>& channel : output)
            {
                largest = std::max(largest, std::abs(channel[static_cast<size_t>(sample)]));
            }
            if (largest >= silence)
            {
                longestGap = std::max(longestGap, sample - lastLoud - 1);
                lastLoud = sample;
            }
        }
        if (! rig.sawNonFinite() && lastLoud < length - static_cast<int>(m_settings.tailHoldSeconds * kTailProbeRate))
        {
            return std::min(m_settings.tailHoldSeconds, longestGap / kTailProbeRate + kTailHoldMarginSeconds);
        }
        return m_settings.tailHoldSeconds;
    }

    std::shared_ptr<TailWait> m_tailWait = std::make_shared<TailWait>(m_settings.tailMaximumSeconds, m_settings.tailHoldSeconds);
    mutable std::once_flag m_tailOnce;
};
}
