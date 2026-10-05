#include "pluginlab/engine/Fingerprint.h"

#include <algorithm>
#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "pluginlab/engine/ChannelAdapter.h"
#include "pluginlab/engine/LatencyMeasurer.h"

namespace pluginlab::engine
{
namespace
{
constexpr double kRates[] = {44100.0, 48000.0, 96000.0};
constexpr double kReferenceRate = 48000.0;
constexpr int kReferenceBlock = 512;
constexpr int kNoiseLength = 12288;        // samples of noise per render; the last kCompareLength are compared
constexpr int kCompareLength = 8192;
constexpr double kSettleSeconds = 0.25;    // silence before the signal: the smoothers of the plugin (they last for a time, not for a number of samples) and filters settle
constexpr int kImpulseResponseOrder = 14;  // 16384 samples
constexpr int kBlockSizeRenderLength = 16384;
constexpr int kMaximumParameters = 64;
constexpr int kMaximumJumpedParameters = 16;
constexpr int kAxisPoints = 128;
constexpr double kAxisLowestHz = 100.0;
constexpr double kAxisHighestHz = 19800.0;
constexpr double kFeatureMinimumDb = 0.5;
constexpr float kNoiseLevel = 0.1f;
constexpr float kLowSetting = 0.25f;
constexpr float kHighSetting = 0.75f;
constexpr float kPokeDistance = 0.4f; // the other value a parameter is set to before its target (always differs from the target)
constexpr double kReactsAboveDb = -80.0;   // a change smaller than this (relative to the output) is no reaction
constexpr double kDifferentAboveDb = -60.0;
constexpr double kSameBelowDb = -80.0;
constexpr double kBlockIndependentBelowDb = -100.0;
constexpr double kFloorDb = -200.0;
constexpr double kTinyLevel = 1.0e-12;
constexpr double kRateFollowRatio = 1.5;   // a feature that moves by more than this factor between 44.1 and 96 kHz follows the rate
constexpr double kRateDifferentDb = 1.0;
constexpr int kReferenceSeed = 7;
constexpr int kMaximumTextLength = 32;
constexpr size_t kReportAxisStep = 10;
const int kBlockSizes[] = {32, 64, 128, 256, 1024, 2048, 509};

std::vector<float> makeNoise(int length)
{
    juce::Random random(kReferenceSeed);
    std::vector<float> noise(static_cast<size_t>(length));
    for (float& sample : noise)
    {
        sample = kNoiseLevel * (random.nextFloat() * 2.0f - 1.0f);
    }
    return noise;
}

std::vector<float> makeImpulse(int length)
{
    std::vector<float> impulse(static_cast<size_t>(length), 0.0f);
    impulse[0] = 1.0f;
    return impulse;
}

// the difference of two signals in dB relative to the level of the second one
double differenceDb(const std::vector<float>& first, const std::vector<float>& second, size_t from)
{
    double differenceSum = 0.0;
    double levelSum = 0.0;
    const size_t length = std::min(first.size(), second.size());
    for (size_t index = from; index < length; ++index)
    {
        const double difference = static_cast<double>(first[index]) - static_cast<double>(second[index]);
        differenceSum += difference * difference;
        levelSum += static_cast<double>(second[index]) * static_cast<double>(second[index]);
    }
    const double count = static_cast<double>(length - from);
    const double differenceRms = std::sqrt(differenceSum / count);
    const double levelRms = std::sqrt(levelSum / count);
    const double reference = std::max(levelRms, kTinyLevel);
    return std::max(kFloorDb, 20.0 * std::log10(std::max(differenceRms, kTinyLevel * kTinyLevel) / reference));
}

juce::String passText(bool passed)
{
    if (passed)
    {
        return "ok";
    }
    return "FAILS";
}

juce::String yesNo(bool value)
{
    if (value)
    {
        return "yes";
    }
    return "no";
}

double toDb(double value)
{
    return std::max(kFloorDb, 20.0 * std::log10(std::max(value, kTinyLevel * kTinyLevel)));
}

// A switch (on/off, a bypass) is not moved together with the others: it would undo them
bool isSwitch(const ParameterFingerprint& parameter)
{
    return parameter.numSteps == 2 || parameter.name.containsIgnoreCase("bypass");
}

// The values a setting is first set to before it is set itself: another value for every parameter (a plugin that reads a parameter
// only when it changes cannot miss the setting)
std::vector<float> makePokeValues(const std::vector<float>& setting)
{
    std::vector<float> other = setting;
    for (float& value : other)
    {
        if (value > kHighSetting - kLowSetting)
        {
            value -= kPokeDistance;
        }
        else
        {
            value += kPokeDistance;
        }
    }
    return other;
}

// One instance of the plugin with what is needed to run audio through it.
class Rig
{
public:
    Rig(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate, int blockSize)
        : m_sampleRate(sampleRate), m_blockSize(blockSize)
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

    // The first channel of the output; the input goes to all channels. Remembers whether a sample was not finite.
    std::vector<float> process(const std::vector<float>& input)
    {
        std::vector<float> output;
        output.reserve(input.size());
        juce::AudioBuffer<float> buffer(m_channels, m_blockSize);
        juce::MidiBuffer midi;
        for (size_t start = 0; start < input.size(); start += static_cast<size_t>(m_blockSize))
        {
            const int count = static_cast<int>(std::min(static_cast<size_t>(m_blockSize), input.size() - start));
            juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), m_channels, count);
            for (int channel = 0; channel < m_channels; ++channel)
            {
                for (int sample = 0; sample < count; ++sample)
                {
                    block.setSample(channel, sample, input[start + static_cast<size_t>(sample)]);
                }
            }
            m_instance->processBlock(block, midi);
            for (int sample = 0; sample < count; ++sample)
            {
                const float value = block.getSample(0, sample);
                if (! std::isfinite(value))
                {
                    m_sawNonFinite = true;
                }
                output.push_back(value);
            }
        }
        return output;
    }

    bool sawNonFinite() const
    {
        return m_sawNonFinite;
    }

    void settle()
    {
        process(std::vector<float>(static_cast<size_t>(m_sampleRate * kSettleSeconds), 0.0f));
    }

private:
    std::unique_ptr<juce::AudioPluginInstance> m_instance;
    double m_sampleRate;
    int m_blockSize;
    int m_channels = 0;
    bool m_sawNonFinite = false;
    juce::String m_error;
};

// Everything needed to make instances and to render with a setting.
class Bench
{
public:
    Bench(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description)
        : m_formatManager(formatManager), m_description(description)
    {
    }

    std::unique_ptr<Rig> make(double sampleRate, int blockSize) const
    {
        return std::make_unique<Rig>(m_formatManager, m_description, sampleRate, blockSize);
    }

    // The careful way that all measurements use (most conservative first, LESSONS_LEARNED §2): a fresh instance, prepared; every parameter is
    // first set to another value and one block runs, then the setting is set; settled; then the input.
    std::vector<float> render(double sampleRate, int blockSize, const std::vector<float>& setting, const std::vector<float>& input) const
    {
        const std::unique_ptr<Rig> rig = make(sampleRate, blockSize);
        if (! rig->isValid())
        {
            return {};
        }
        rig->prepare();
        rig->apply(makePokeValues(setting));
        rig->process(makeNoise(blockSize));
        rig->apply(setting);
        rig->settle();
        return rig->process(input);
    }

    // a fresh instance, parameters set after prepare (no poke), settled, then the input
    std::vector<float> renderAfterPrepare(double sampleRate, int blockSize, const std::vector<float>& setting, const std::vector<float>& input) const
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
};

// The magnitude response in dB on the common axis from an impulse response
std::vector<double> responseOnAxis(const std::vector<float>& impulseResponse, double sampleRate, const std::vector<double>& axis)
{
    const int size = 1 << kImpulseResponseOrder;
    juce::dsp::FFT fft(kImpulseResponseOrder);
    std::vector<float> data(static_cast<size_t>(2 * size), 0.0f);
    for (size_t index = 0; index < impulseResponse.size() && index < static_cast<size_t>(size); ++index)
    {
        data[index] = impulseResponse[index];
    }
    fft.performFrequencyOnlyForwardTransform(data.data());

    std::vector<double> response;
    for (const double frequency : axis)
    {
        const double position = frequency / (sampleRate / static_cast<double>(size));
        const int lower = static_cast<int>(position);
        const double fraction = position - lower;
        const double magnitude = (1.0 - fraction) * data[static_cast<size_t>(lower)] + fraction * data[static_cast<size_t>(lower + 1)];
        response.push_back(toDb(magnitude));
    }
    return response;
}

std::vector<double> makeAxis()
{
    std::vector<double> axis;
    const double ratio = std::pow(kAxisHighestHz / kAxisLowestHz, 1.0 / (kAxisPoints - 1));
    double frequency = kAxisLowestHz;
    for (int index = 0; index < kAxisPoints; ++index)
    {
        axis.push_back(frequency);
        frequency *= ratio;
    }
    return axis;
}

// refA, refB: the output of the settings A and B reached by a change of the parameters after prepare (what the plugin does when it is
// used the way a DAW does)
DeliveryResult judgeDelivery(const juce::String& name, const std::vector<float>& a1, const std::vector<float>& a2, const std::vector<float>& b,
                             const std::vector<float>& a3, const std::vector<float>& refA, const std::vector<float>& refB)
{
    DeliveryResult result;
    result.name = name;
    const size_t from = a1.size() - static_cast<size_t>(kCompareLength);
    const double repeatA2 = differenceDb(a2, a1, from);
    const double repeatA3 = differenceDb(a3, a1, from);
    const double reaction = differenceDb(b, a1, from);
    const double toReferenceA = differenceDb(a1, refA, from);
    const double toReferenceB = differenceDb(b, refB, from);
    result.repeatable = repeatA2 < kSameBelowDb && repeatA3 < kSameBelowDb;
    result.reacts = reaction > kDifferentAboveDb;
    result.correct = toReferenceA < kSameBelowDb && toReferenceB < kSameBelowDb;
    result.passed = result.repeatable && result.reacts && result.correct;
    result.numbers = "A again " + juce::String(repeatA2, 1) + " / " + juce::String(repeatA3, 1) + " dB, B against A " + juce::String(reaction, 1)
                   + " dB, A and B against the references " + juce::String(toReferenceA, 1) + " / " + juce::String(toReferenceB, 1) + " dB";
    if (! result.reacts)
    {
        result.comment = "the other settings (B) gave the same output as A (" + juce::String(reaction, 1) + " dB): the parameters were lost or ignored. ";
    }
    if (! result.repeatable)
    {
        result.comment += "the same settings gave other output (A again: " + juce::String(repeatA3, 1) + " dB). ";
    }
    if (result.reacts && ! result.correct)
    {
        result.comment += "the settings that were delivered first differ from the same settings reached by a change (A: " + juce::String(toReferenceA, 1)
                        + " dB, B: " + juce::String(toReferenceB, 1) + " dB): the first delivery was lost. ";
    }
    return result;
}
}

PluginFingerprint measureFingerprint(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description)
{
    PluginFingerprint fingerprint;
    fingerprint.description = description;
    const Bench bench(formatManager, description);

    const std::unique_ptr<Rig> first = bench.make(kReferenceRate, kReferenceBlock);
    if (! first->isValid())
    {
        fingerprint.message = first->getError();
        if (fingerprint.message.isEmpty())
        {
            fingerprint.message = "the plugin runs neither with mono nor with stereo audio";
        }
        return fingerprint;
    }
    fingerprint.loaded = true;

    // ---- layouts and parameters ----
    for (const bool stereo : {false, true})
    {
        juce::AudioProcessor::BusesLayout layout;
        juce::AudioChannelSet set = juce::AudioChannelSet::mono();
        if (stereo)
        {
            set = juce::AudioChannelSet::stereo();
        }
        layout.inputBuses.add(set);
        layout.outputBuses.add(set);
        if (first->get().checkBusesLayoutSupported(layout))
        {
            if (stereo)
            {
                fingerprint.supportsStereo = true;
            }
            else
            {
                fingerprint.supportsMono = true;
            }
        }
    }

    std::vector<float> defaults;
    const juce::Array<juce::AudioProcessorParameter*>& parameters = first->get().getParameters();
    for (int index = 0; index < parameters.size() && index < kMaximumParameters; ++index)
    {
        juce::AudioProcessorParameter& parameter = *parameters[index];
        ParameterFingerprint entry;
        entry.index = index;
        entry.name = parameter.getName(kMaximumTextLength);
        entry.textAtMinimum = parameter.getText(0.0f, kMaximumTextLength);
        entry.textAtDefault = parameter.getText(parameter.getDefaultValue(), kMaximumTextLength);
        entry.textAtMaximum = parameter.getText(1.0f, kMaximumTextLength);
        entry.numSteps = parameter.getNumSteps();
        entry.automatable = parameter.isAutomatable();
        fingerprint.parameters.push_back(entry);
        defaults.push_back(parameter.getDefaultValue());
    }

    // ---- which parameters change the audio ----
    const std::vector<float> noise = makeNoise(kNoiseLength);
    const std::vector<float> baseline = bench.render(kReferenceRate, kReferenceBlock, defaults, noise);
    const size_t compareFrom = noise.size() - static_cast<size_t>(kCompareLength);
    // Pass 1 at the defaults. Pass 2: a parameter can have no effect at the defaults (the frequency of an EQ whose gain is 0 dB), so the
    // others are tried again with the parameters that reacted moved to 0.75.
    const auto scan = [&](const std::vector<float>& base, const std::vector<float>& reference, bool onlyUnreacted)
    {
        for (ParameterFingerprint& entry : fingerprint.parameters)
        {
            if (onlyUnreacted && entry.changesTheAudio)
            {
                continue;
            }
            for (const float value : {kLowSetting, kHighSetting})
            {
                std::vector<float> setting = base;
                setting[static_cast<size_t>(entry.index)] = value;
                const std::vector<float> output = bench.render(kReferenceRate, kReferenceBlock, setting, noise);
                entry.changeDb = std::max(entry.changeDb, differenceDb(output, reference, compareFrom));
            }
            entry.changesTheAudio = entry.changeDb > kReactsAboveDb;
        }
    };
    scan(defaults, baseline, false);
    std::vector<float> withReacting = defaults;
    for (const ParameterFingerprint& entry : fingerprint.parameters)
    {
        if (entry.changesTheAudio && ! isSwitch(entry))
        {
            withReacting[static_cast<size_t>(entry.index)] = kHighSetting;
        }
    }
    if (withReacting != defaults)
    {
        scan(withReacting, bench.render(kReferenceRate, kReferenceBlock, withReacting, noise), true);
    }
    for (const ParameterFingerprint& entry : fingerprint.parameters)
    {
        if (entry.changesTheAudio)
        {
            fingerprint.reactingParameters.push_back(entry.index);
        }
    }

    // The setting B: the parameters that change the audio, moved to 0.75 one after the other. One that would undo the others (a bypass
    // switch) is left out: B must differ from A.
    std::vector<float> settingB = defaults;
    for (const int index : fingerprint.reactingParameters)
    {
        if (isSwitch(fingerprint.parameters[static_cast<size_t>(index)]))
        {
            continue;
        }
        std::vector<float> candidate = settingB;
        candidate[static_cast<size_t>(index)] = kHighSetting;
        const std::vector<float> output = bench.render(kReferenceRate, kReferenceBlock, candidate, noise);
        if (differenceDb(output, baseline, compareFrom) > kDifferentAboveDb)
        {
            settingB = candidate;
        }
    }

    // ---- latency and response at the three sample rates ----
    fingerprint.axisFrequenciesHz = makeAxis();
    const std::vector<float> impulse = makeImpulse(1 << kImpulseResponseOrder);
    for (const double rate : kRates)
    {
        RateFingerprint entry;
        entry.sampleRate = rate;
        {
            const std::unique_ptr<Rig> rig = bench.make(rate, kReferenceBlock);
            const LatencyResult latency = measureLatency(rig->get(), rate, kReferenceBlock, rig->getChannels());
            entry.reportedLatency = latency.reportedSamples;
            entry.measuredLatency = latency.measuredSamples;
        }
        const std::vector<float> impulseResponse = bench.render(rate, kReferenceBlock, settingB, impulse);
        entry.responseDb = responseOnAxis(impulseResponse, rate, fingerprint.axisFrequenciesHz);
        double largest = 0.0;
        for (size_t index = 0; index < entry.responseDb.size(); ++index)
        {
            if (std::abs(entry.responseDb[index]) > std::abs(largest))
            {
                largest = entry.responseDb[index];
                entry.featureFrequencyHz = fingerprint.axisFrequenciesHz[index];
            }
        }
        entry.featureGainDb = largest;
        entry.hasFeature = std::abs(largest) >= kFeatureMinimumDb;
        fingerprint.rates.push_back(entry);
    }
    for (size_t first = 0; first < fingerprint.rates.size(); ++first)
    {
        for (size_t second = first + 1; second < fingerprint.rates.size(); ++second)
        {
            for (size_t index = 0; index < fingerprint.axisFrequenciesHz.size(); ++index)
            {
                const double difference = std::abs(fingerprint.rates[first].responseDb[index] - fingerprint.rates[second].responseDb[index]);
                fingerprint.largestRateDifferenceDb = std::max(fingerprint.largestRateDifferenceDb, difference);
            }
        }
    }
    const RateFingerprint& lowest = fingerprint.rates.front();
    const RateFingerprint& highest = fingerprint.rates.back();
    fingerprint.rateRatio = highest.sampleRate / lowest.sampleRate;
    if (lowest.hasFeature && highest.hasFeature)
    {
        fingerprint.featureRatio = highest.featureFrequencyHz / lowest.featureFrequencyHz;
        fingerprint.responseFollowsSampleRate = fingerprint.featureRatio > kRateFollowRatio;
    }

    // ---- delivery: A, A, B, A in four ways ----
    std::vector<float> referenceA;
    std::vector<float> referenceB;
    {
        const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
        rig->prepare();
        rig->settle();
        rig->apply(defaults);
        const std::vector<float> a1 = rig->process(noise);
        const std::vector<float> a2 = rig->process(noise);
        rig->apply(settingB);
        const std::vector<float> b = rig->process(noise);
        rig->apply(defaults);
        const std::vector<float> a3 = rig->process(noise);
        referenceA = a3; // a setting reached by a change
        referenceB = b;
        fingerprint.delivery.push_back(judgeDelivery("one instance, parameters set after prepare (stream)", a1, a2, b, a3, referenceA, referenceB));
    }
    const auto freshBefore = [&](const std::vector<float>& setting)
    {
        const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
        rig->apply(setting); // before prepareToPlay
        rig->prepare();
        rig->settle();
        return rig->process(noise);
    };
    fingerprint.delivery.push_back(judgeDelivery("new instance per render, parameters set before prepare", freshBefore(defaults), freshBefore(defaults),
                                                 freshBefore(settingB), freshBefore(defaults), referenceA, referenceB));
    fingerprint.delivery.push_back(judgeDelivery(
        "new instance per render, parameters set after prepare",
        bench.renderAfterPrepare(kReferenceRate, kReferenceBlock, defaults, noise), bench.renderAfterPrepare(kReferenceRate, kReferenceBlock, defaults, noise),
        bench.renderAfterPrepare(kReferenceRate, kReferenceBlock, settingB, noise), bench.renderAfterPrepare(kReferenceRate, kReferenceBlock, defaults, noise),
        referenceA, referenceB));
    // the most careful way: after prepare every parameter is first set to another value, one block runs, then the target is set (a plugin
    // that reads a parameter only when it changes cannot miss the target)
    const auto freshPoked = [&](const std::vector<float>& setting) { return bench.render(kReferenceRate, kReferenceBlock, setting, noise); };
    fingerprint.delivery.push_back(judgeDelivery("new instance per render, after prepare every parameter first set to another value, then the target",
                                                 freshPoked(defaults), freshPoked(defaults), freshPoked(settingB), freshPoked(defaults), referenceA,
                                                 referenceB));
    for (auto delivery = fingerprint.delivery.rbegin(); delivery != fingerprint.delivery.rend(); ++delivery)
    {
        if (delivery->passed)
        {
            fingerprint.recommendedDelivery = delivery->name; // (the last in the list is the most careful)
            break;
        }
    }

    // ---- block size independence (setting B, the reference block size against the others) ----
    const std::vector<float> longNoise = makeNoise(kBlockSizeRenderLength);
    const std::vector<float> reference = bench.render(kReferenceRate, kReferenceBlock, settingB, longNoise);
    for (const int blockSize : kBlockSizes)
    {
        BlockSizeResult result;
        result.blockSize = blockSize;
        const std::vector<float> output = bench.render(kReferenceRate, blockSize, settingB, longNoise);
        result.differenceDb = differenceDb(output, reference, 0);
        if (result.differenceDb > kBlockIndependentBelowDb)
        {
            fingerprint.blockSizeIndependent = false;
        }
        fingerprint.blockSizes.push_back(result);
    }

    // ---- determinism ----
    {
        const std::vector<float> one = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        const std::vector<float> two = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        fingerprint.deterministic = ! one.empty() && differenceDb(one, two, 0) <= kFloorDb + 1.0;
    }

    // ---- robustness: every reacting parameter to both ends and back, then B again ----
    {
        const std::vector<float> reachedB = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
        rig->prepare();
        rig->apply(settingB);
        rig->settle();
        rig->process(noise);
        const std::vector<float> oneBlock = makeNoise(kReferenceBlock);
        int jumped = 0;
        for (const int index : fingerprint.reactingParameters)
        {
            if (jumped++ >= kMaximumJumpedParameters)
            {
                break;
            }
            rig->set(index, 0.0f);
            rig->process(oneBlock);
            rig->set(index, 1.0f);
            rig->process(oneBlock);
        }
        rig->apply(settingB);
        rig->settle();
        rig->process(noise); // time to come back
        const std::vector<float> afterwards = rig->process(noise);
        fingerprint.outputStaysFinite = ! rig->sawNonFinite();
        fingerprint.recoversFromJumps = fingerprint.outputStaysFinite && ! reachedB.empty()
                                     && differenceDb(afterwards, reachedB, noise.size() - static_cast<size_t>(kCompareLength)) < kSameBelowDb;
    }

    // ---- silence ----
    {
        const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
        rig->prepare();
        rig->apply(settingB);
        rig->settle();
        const std::vector<float> output = rig->process(std::vector<float>(static_cast<size_t>(kCompareLength), 0.0f));
        float peak = 0.0f;
        for (const float sample : output)
        {
            peak = std::max(peak, std::abs(sample));
        }
        fingerprint.silenceStaysSilent = peak == 0.0f;
        fingerprint.idleLevelDb = toDb(peak);
    }

    // ---- what is noteworthy ----
    for (const RateFingerprint& rate : fingerprint.rates)
    {
        if (rate.reportedLatency != rate.measuredLatency)
        {
            fingerprint.findings.push_back("Latency at " + juce::String(rate.sampleRate, 0) + " Hz: the plugin reports " + juce::String(rate.reportedLatency)
                                           + " samples, measured " + juce::String(rate.measuredLatency) + " samples");
        }
    }
    if (fingerprint.reactingParameters.empty())
    {
        fingerprint.findings.push_back("No parameter changed the audio (an instrument, a pure analyser, or the parameters are not read after prepare)");
    }
    if (fingerprint.responseFollowsSampleRate)
    {
        fingerprint.findings.push_back("The response moves with the sample rate: the feature is at " + juce::String(fingerprint.rates.front().featureFrequencyHz, 0)
                                       + " Hz at " + juce::String(fingerprint.rates.front().sampleRate, 0) + " Hz and at "
                                       + juce::String(fingerprint.rates.back().featureFrequencyHz, 0) + " Hz at " + juce::String(fingerprint.rates.back().sampleRate, 0)
                                       + " Hz (factor " + juce::String(fingerprint.featureRatio, 3) + ", the rates differ by " + juce::String(fingerprint.rateRatio, 3)
                                       + "): the filter is designed for one sample rate");
    }
    else if (fingerprint.largestRateDifferenceDb > kRateDifferentDb)
    {
        fingerprint.findings.push_back("The response differs by up to " + juce::String(fingerprint.largestRateDifferenceDb, 2)
                                       + " dB between the sample rates (the usual cramping near the Nyquist frequency of a bilinear design is below about 1 dB up to 10 kHz)");
    }
    for (const DeliveryResult& delivery : fingerprint.delivery)
    {
        if (! delivery.passed && ! fingerprint.reactingParameters.empty())
        {
            fingerprint.findings.push_back("Delivery '" + delivery.name + "' fails: " + delivery.comment);
        }
    }
    if (! fingerprint.blockSizeIndependent)
    {
        fingerprint.findings.push_back("The output depends on the block size");
    }
    if (! fingerprint.deterministic)
    {
        fingerprint.findings.push_back("Two instances with the same input give different output");
    }
    if (! fingerprint.outputStaysFinite)
    {
        fingerprint.findings.push_back("The output contained NaN or infinity after parameter jumps");
    }
    else if (! fingerprint.recoversFromJumps)
    {
        fingerprint.findings.push_back("After parameter jumps to both ends the output does not come back to what it was");
    }
    if (! fingerprint.silenceStaysSilent)
    {
        fingerprint.findings.push_back("Digital silence in does not give digital silence out (peak " + juce::String(fingerprint.idleLevelDb, 1) + " dBFS)");
    }
    return fingerprint;
}

juce::String createReport(const PluginFingerprint& fingerprint)
{
    juce::String text;
    text << "# Fingerprint: " << fingerprint.description.name << "\n\n";
    text << "- file: `" << fingerprint.description.fileOrIdentifier << "`\n";
    text << "- format: " << fingerprint.description.pluginFormatName << ", manufacturer: " << fingerprint.description.manufacturerName
         << ", version: " << fingerprint.description.version << "\n";
    if (! fingerprint.loaded)
    {
        text << "- **not measured**: " << fingerprint.message << "\n";
        return text;
    }
    text << "- channels: mono " << yesNo(fingerprint.supportsMono) << ", stereo " << yesNo(fingerprint.supportsStereo) << "\n\n";

    text << "## Findings\n";
    if (fingerprint.findings.empty())
    {
        text << "- nothing unusual found\n";
    }
    for (const juce::String& finding : fingerprint.findings)
    {
        text << "- " << finding << "\n";
    }

    text << "\n## Parameters\n| no. | name | min | default | max | steps | automatable | changes the audio (dB) |\n|---|---|---|---|---|---|---|---|\n";
    for (const ParameterFingerprint& parameter : fingerprint.parameters)
    {
        text << "| " << parameter.index << " | " << parameter.name << " | " << parameter.textAtMinimum << " | " << parameter.textAtDefault << " | "
             << parameter.textAtMaximum << " | " << parameter.numSteps << " | " << yesNo(parameter.automatable) << " | ";
        if (parameter.changesTheAudio)
        {
            text << juce::String(parameter.changeDb, 1);
        }
        else
        {
            text << "no";
        }
        text << " |\n";
    }

    text << "\n## Sample rates (the parameters that change the audio at 0.75)\n| rate | latency reported | latency measured | feature frequency | feature gain |\n|---|---|---|---|---|\n";
    for (const RateFingerprint& rate : fingerprint.rates)
    {
        text << "| " << juce::String(rate.sampleRate, 0) << " Hz | " << rate.reportedLatency << " | " << rate.measuredLatency << " | ";
        if (rate.hasFeature)
        {
            text << juce::String(rate.featureFrequencyHz, 0) << " Hz | " << juce::String(rate.featureGainDb, 2) << " dB";
        }
        else
        {
            text << "none | none";
        }
        text << " |\n";
    }
    text << "\nLargest difference of the response between the rates (100 Hz - 19.8 kHz): " << juce::String(fingerprint.largestRateDifferenceDb, 2) << " dB.\n";
    if (fingerprint.featureRatio > 0.0)
    {
        text << "Feature frequency at the highest rate over the lowest: " << juce::String(fingerprint.featureRatio, 3) << " (the rates: "
             << juce::String(fingerprint.rateRatio, 3) << ").\n";
    }

    text << "\n## Response at the setting B (dB; every ~10th point of the axis)\n| frequency |";
    for (const RateFingerprint& rate : fingerprint.rates)
    {
        text << " " << juce::String(rate.sampleRate, 0) << " Hz |";
    }
    text << "\n|---|";
    for (size_t index = 0; index < fingerprint.rates.size(); ++index)
    {
        text << "---|";
    }
    text << "\n";
    for (size_t point = 0; point < fingerprint.axisFrequenciesHz.size(); point += kReportAxisStep)
    {
        text << "| " << juce::String(fingerprint.axisFrequenciesHz[point], 0) << " Hz |";
        for (const RateFingerprint& rate : fingerprint.rates)
        {
            text << " " << juce::String(rate.responseDb[point], 2) << " |";
        }
        text << "\n";
    }

    text << "\n## Delivery of parameters (A, A, B, A)\n| way | repeatable | reacts | as after a change | result | differences |\n|---|---|---|---|---|---|\n";
    for (const DeliveryResult& delivery : fingerprint.delivery)
    {
        text << "| " << delivery.name << " | " << yesNo(delivery.repeatable) << " | " << yesNo(delivery.reacts) << " | " << yesNo(delivery.correct) << " | "
             << passText(delivery.passed) << " | " << delivery.numbers << " |\n";
    }

    if (fingerprint.recommendedDelivery.isNotEmpty())
    {
        text << "\nMost careful way that works: " << fingerprint.recommendedDelivery << ".\n";
    }
    else
    {
        text << "\nNo way of delivering the parameters passed the test.\n";
    }

    text << "\n## Block sizes (difference to block size " << kReferenceBlock << ")\n| block size | difference |\n|---|---|\n";
    for (const BlockSizeResult& block : fingerprint.blockSizes)
    {
        text << "| " << block.blockSize << " | " << juce::String(block.differenceDb, 1) << " dB |\n";
    }

    text << "\n## Other\n- deterministic: " << yesNo(fingerprint.deterministic) << "\n- output stays finite: " << yesNo(fingerprint.outputStaysFinite)
         << "\n- recovers from parameter jumps: " << yesNo(fingerprint.recoversFromJumps) << "\n- digital silence in gives digital silence out: "
         << yesNo(fingerprint.silenceStaysSilent) << "\n";
    return text;
}
}
