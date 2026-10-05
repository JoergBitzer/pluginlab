#include "pluginlab/engine/Fingerprint.h"

#include <algorithm>
#include <cmath>

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
constexpr double kFloorDb = -200.0;
constexpr double kTiny = 1.0e-20;
constexpr int kReferenceSeed = 7;
constexpr int kMaximumTextLength = 32;
constexpr int kContinuousSteps = 0x7fffffff; // JUCE's number of steps of a continuous parameter
constexpr int kSwitchSteps = 2;
constexpr double kNothingDb = -120.0;      // output before the peak / before the impulse below this is "none"
constexpr int kLatencyDecimals = 1;

double toDb(double value)
{
    if (value < kTiny)
    {
        return kFloorDb;
    }
    return std::max(kFloorDb, 20.0 * std::log10(value));
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

juce::String formatDb(double value)
{
    if (value <= kFloorDb)
    {
        return "identical";
    }
    return juce::String(value, 1);
}

// "relative dB re reference / absolute dBFS"
juce::String formatDifference(const Difference& difference)
{
    if (difference.relativeDb <= kFloorDb && difference.absoluteDbfs <= kFloorDb)
    {
        return "identical";
    }
    juce::String text = formatDb(difference.relativeDb) + " dB / " + formatDb(difference.absoluteDbfs) + " dBFS";
    if (difference.referenceSilent)
    {
        text += " (silent reference)";
    }
    return text;
}

std::vector<float> makeNoise(int length, double level)
{
    juce::Random random(kReferenceSeed);
    std::vector<float> noise(static_cast<size_t>(length));
    for (float& sample : noise)
    {
        sample = static_cast<float>(level) * (random.nextFloat() * 2.0f - 1.0f);
    }
    return noise;
}

// The difference of a from the reference b, over the samples from "from" to the end
Difference compare(const std::vector<float>& a, const std::vector<float>& b, size_t from, const FingerprintSettings& settings)
{
    Difference result;
    const size_t length = std::min(a.size(), b.size());
    if (from >= length)
    {
        return result;
    }
    double differenceSum = 0.0;
    double levelSum = 0.0;
    for (size_t index = from; index < length; ++index)
    {
        const double difference = static_cast<double>(a[index]) - static_cast<double>(b[index]);
        differenceSum += difference * difference;
        levelSum += static_cast<double>(b[index]) * static_cast<double>(b[index]);
    }
    const double count = static_cast<double>(length - from);
    const double differenceRms = std::sqrt(differenceSum / count);
    const double levelRms = std::sqrt(levelSum / count);
    result.absoluteDbfs = toDb(differenceRms);
    result.referenceSilent = toDb(levelRms) < settings.silentReferenceDbfs;
    if (result.referenceSilent)
    {
        result.relativeDb = result.absoluteDbfs;
        return result;
    }
    if (differenceRms < kTiny)
    {
        result.relativeDb = kFloorDb;
        return result;
    }
    result.relativeDb = std::max(kFloorDb, 20.0 * std::log10(differenceRms / levelRms));
    return result;
}

// the value a decision uses: relative, or absolute for a silent reference
double decisionValue(const Difference& difference)
{
    if (difference.referenceSilent)
    {
        return difference.absoluteDbfs;
    }
    return difference.relativeDb;
}

bool isBelow(const Difference& difference, double threshold)
{
    return decisionValue(difference) < threshold;
}

bool isAbove(const Difference& difference, double threshold)
{
    return decisionValue(difference) > threshold;
}

// A switch (on/off, a bypass) is not moved together with the others: it would undo them
bool isSwitch(const ParameterFingerprint& parameter)
{
    return parameter.numSteps == kSwitchSteps || parameter.name.containsIgnoreCase("bypass");
}

// The values a setting is first set to before it is set itself: another value for every parameter (a plugin that reads a parameter
// only when it changes cannot miss the setting)
std::vector<float> makePokeValues(const std::vector<float>& setting, const FingerprintSettings& settings)
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
        process(std::vector<float>(static_cast<size_t>(m_sampleRate * m_settings.settleSeconds), 0.0f));
    }

private:
    std::unique_ptr<juce::AudioPluginInstance> m_instance;
    double m_sampleRate;
    int m_blockSize;
    const FingerprintSettings& m_settings;
    int m_channels = 0;
    bool m_sawNonFinite = false;
    juce::String m_error;
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
    std::vector<float> render(double sampleRate, int blockSize, const std::vector<float>& setting, const std::vector<float>& input) const
    {
        const std::unique_ptr<Rig> rig = make(sampleRate, blockSize);
        if (! rig->isValid())
        {
            return {};
        }
        rig->prepare();
        rig->apply(makePokeValues(setting, m_settings));
        rig->process(makeNoise(kReferenceBlock, m_settings.noiseLevel));
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
    const FingerprintSettings& m_settings;
};

// refA, refB: the output of the settings A and B reached by a change of the parameters after prepare (what the plugin does when it is
// used the way a DAW does)
DeliveryResult judgeDelivery(const juce::String& name, const std::vector<float>& a1, const std::vector<float>& a2, const std::vector<float>& b,
                             const std::vector<float>& a3, const std::vector<float>& refA, const std::vector<float>& refB,
                             const FingerprintSettings& settings)
{
    DeliveryResult result;
    result.name = name;
    const size_t from = a1.size() - static_cast<size_t>(kCompareLength);
    result.secondA = compare(a2, a1, from, settings);
    result.thirdA = compare(a3, a1, from, settings);
    result.reaction = compare(b, a1, from, settings);
    result.toReferenceA = compare(a1, refA, from, settings);
    result.toReferenceB = compare(b, refB, from, settings);
    result.repeatable = isBelow(result.secondA, settings.sameBelowDb) && isBelow(result.thirdA, settings.sameBelowDb);
    result.reacts = isAbove(result.reaction, settings.differentAboveDb);
    result.correct = isBelow(result.toReferenceA, settings.sameBelowDb) && isBelow(result.toReferenceB, settings.sameBelowDb);
    result.passed = result.repeatable && result.reacts && result.correct;
    if (! result.reacts)
    {
        result.comment = "the other settings (B) gave the same output as A (" + formatDifference(result.reaction)
                       + "): the parameters were lost or ignored. ";
    }
    if (! result.repeatable)
    {
        result.comment += "the same settings gave other output (A again: " + formatDifference(result.thirdA) + "). ";
    }
    if (result.reacts && ! result.correct)
    {
        result.comment += "the settings that were delivered first differ from the same settings reached by a change (A: "
                        + formatDifference(result.toReferenceA) + ", B: " + formatDifference(result.toReferenceB) + "): the first delivery was lost. ";
    }
    return result;
}
}

juce::String describeSteps(int numSteps)
{
    if (numSteps >= kContinuousSteps)
    {
        return "continuous";
    }
    if (numSteps == kSwitchSteps)
    {
        return "switch";
    }
    return juce::String(numSteps);
}

PluginFingerprint measureFingerprint(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                                     const FingerprintSettings& settings)
{
    PluginFingerprint fingerprint;
    fingerprint.description = description;
    fingerprint.settings = settings;
    const Bench bench(formatManager, description, settings);
    const float low = static_cast<float>(settings.lowSetting);
    const float high = static_cast<float>(settings.highSetting);

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
    fingerprint.numberOfParameters = parameters.size();
    for (int index = 0; index < parameters.size() && index < settings.maximumParameters; ++index)
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
    const std::vector<float> noise = makeNoise(kNoiseLength, settings.noiseLevel);
    const std::vector<float> baseline = bench.render(kReferenceRate, kReferenceBlock, defaults, noise);
    const size_t compareFrom = noise.size() - static_cast<size_t>(kCompareLength);
    // Pass 1 at the defaults. Pass 2: a parameter can have no effect at the defaults (the frequency of an EQ whose gain is 0 dB), so the
    // others are tried again with the parameters that reacted moved to the high position.
    const auto scan = [&](const std::vector<float>& base, const std::vector<float>& reference, bool onlyUnreacted, const juce::String& context)
    {
        for (ParameterFingerprint& entry : fingerprint.parameters)
        {
            if (onlyUnreacted && entry.changesTheAudio)
            {
                continue;
            }
            Difference largest;
            for (const float value : {low, high})
            {
                std::vector<float> setting = base;
                setting[static_cast<size_t>(entry.index)] = value;
                const std::vector<float> output = bench.render(kReferenceRate, kReferenceBlock, setting, noise);
                const Difference change = compare(output, reference, compareFrom, settings);
                if (decisionValue(change) > decisionValue(largest))
                {
                    largest = change;
                }
            }
            if (decisionValue(largest) > decisionValue(entry.change) || entry.measuredWith.isEmpty())
            {
                entry.change = largest;
                entry.measuredWith = context;
            }
            entry.changesTheAudio = isAbove(entry.change, settings.reactsAboveDb);
        }
    };
    scan(defaults, baseline, false, "defaults");
    std::vector<float> withReacting = defaults;
    for (const ParameterFingerprint& entry : fingerprint.parameters)
    {
        if (entry.changesTheAudio && ! isSwitch(entry))
        {
            withReacting[static_cast<size_t>(entry.index)] = high;
        }
    }
    if (withReacting != defaults)
    {
        scan(withReacting, bench.render(kReferenceRate, kReferenceBlock, withReacting, noise), true,
             "the others at " + juce::String(settings.highSetting, 2));
    }
    for (const ParameterFingerprint& entry : fingerprint.parameters)
    {
        if (entry.changesTheAudio)
        {
            fingerprint.reactingParameters.push_back(entry.index);
        }
    }

    // The setting B: the parameters that change the audio, moved to the high position one after the other. One that would undo the others
    // (a bypass switch) is left out: B must differ from A.
    std::vector<float> settingB = defaults;
    for (const int index : fingerprint.reactingParameters)
    {
        if (isSwitch(fingerprint.parameters[static_cast<size_t>(index)]))
        {
            continue;
        }
        std::vector<float> candidate = settingB;
        candidate[static_cast<size_t>(index)] = high;
        const std::vector<float> output = bench.render(kReferenceRate, kReferenceBlock, candidate, noise);
        if (isAbove(compare(output, baseline, compareFrom, settings), settings.differentAboveDb))
        {
            settingB = candidate;
        }
    }

    // ---- latency at the three sample rates (default parameters, delayed impulse) ----
    LatencyOptions latencyOptions;
    latencyOptions.preDelaySamples = settings.impulsePreDelaySamples;
    latencyOptions.observeSeconds = settings.latencyObserveSeconds;
    latencyOptions.minimumPeak = settings.latencyMinimumPeak;
    for (const double rate : kRates)
    {
        RateFingerprint entry;
        entry.sampleRate = rate;
        const std::unique_ptr<Rig> rig = bench.make(rate, kReferenceBlock);
        const LatencyResult latency = measureLatency(rig->get(), rate, kReferenceBlock, rig->getChannels(), latencyOptions);
        entry.reportedAfterPrepare = latency.reportedSamples;
        entry.reportedAfterAudio = latency.reportedAfterAudio;
        entry.outputFound = latency.found;
        entry.measuredLatency = latency.measuredSamples;
        entry.outputBeforePeakDb = latency.outputBeforePeakDb;
        entry.outputBeforeImpulseDbfs = latency.outputBeforeImpulseDbfs;
        fingerprint.rates.push_back(entry);
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
        rig->settle(); // a plugin that smooths its parameters needs the time, as after every other change in the measurement
        const std::vector<float> b = rig->process(noise);
        rig->apply(defaults);
        rig->settle();
        const std::vector<float> a3 = rig->process(noise);
        referenceA = a3; // a setting reached by a change
        referenceB = b;
        fingerprint.delivery.push_back(judgeDelivery("one instance, parameters set after prepare (stream)", a1, a2, b, a3, referenceA, referenceB, settings));
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
                                                 freshBefore(settingB), freshBefore(defaults), referenceA, referenceB, settings));
    fingerprint.delivery.push_back(judgeDelivery(
        "new instance per render, parameters set after prepare",
        bench.renderAfterPrepare(kReferenceRate, kReferenceBlock, defaults, noise), bench.renderAfterPrepare(kReferenceRate, kReferenceBlock, defaults, noise),
        bench.renderAfterPrepare(kReferenceRate, kReferenceBlock, settingB, noise), bench.renderAfterPrepare(kReferenceRate, kReferenceBlock, defaults, noise),
        referenceA, referenceB, settings));
    const auto freshPoked = [&](const std::vector<float>& setting) { return bench.render(kReferenceRate, kReferenceBlock, setting, noise); };
    fingerprint.delivery.push_back(judgeDelivery("new instance per render, after prepare every parameter first set to another value, then the target",
                                                 freshPoked(defaults), freshPoked(defaults), freshPoked(settingB), freshPoked(defaults), referenceA,
                                                 referenceB, settings));
    for (auto delivery = fingerprint.delivery.rbegin(); delivery != fingerprint.delivery.rend(); ++delivery)
    {
        if (delivery->passed)
        {
            fingerprint.recommendedDelivery = delivery->name; // (the last in the list is the most careful)
            break;
        }
    }

    // ---- block size independence: a long render, decided on its end (the steady state) ----
    const int blockRenderLength = static_cast<int>(kReferenceRate * settings.blockRenderSeconds);
    const size_t steadyFrom = static_cast<size_t>(blockRenderLength - static_cast<int>(kReferenceRate * settings.blockCompareSeconds));
    const std::vector<float> longNoise = makeNoise(blockRenderLength, settings.noiseLevel);
    const std::vector<float> reference = bench.render(kReferenceRate, kReferenceBlock, settingB, longNoise);
    for (const int blockSize : settings.blockSizes)
    {
        BlockSizeResult result;
        result.blockSize = blockSize;
        const std::vector<float> output = bench.render(kReferenceRate, blockSize, settingB, longNoise);
        result.steadyState = compare(output, reference, steadyFrom, settings);
        result.whole = compare(output, reference, 0, settings);
        if (! isBelow(result.steadyState, settings.blockIndependentBelowDb))
        {
            fingerprint.blockSizeIndependent = false;
        }
        fingerprint.blockSizes.push_back(result);
    }

    // ---- determinism ----
    {
        const std::vector<float> one = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        const std::vector<float> two = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        const Difference difference = compare(one, two, 0, settings);
        fingerprint.deterministic = ! one.empty() && difference.absoluteDbfs <= kFloorDb;
    }

    // ---- robustness: every reacting parameter to both ends and back, then B again ----
    {
        const std::vector<float> reachedB = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
        rig->prepare();
        rig->apply(settingB);
        rig->settle();
        rig->process(noise);
        const std::vector<float> oneBlock = makeNoise(kReferenceBlock, settings.noiseLevel);
        int jumped = 0;
        for (const int index : fingerprint.reactingParameters)
        {
            if (jumped++ >= settings.maximumJumpedParameters)
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
                                     && isBelow(compare(afterwards, reachedB, compareFrom, settings), settings.sameBelowDb);
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

    // ---- what is noteworthy (with the numbers) ----
    for (const RateFingerprint& rate : fingerprint.rates)
    {
        const juce::String at = "Latency at " + juce::String(rate.sampleRate, 0) + " Hz: ";
        if (! rate.outputFound)
        {
            fingerprint.findings.push_back(at + "nothing came out for the impulse (at the default parameters): no latency measured");
            continue;
        }
        if (rate.reportedAfterAudio != rate.measuredLatency)
        {
            fingerprint.findings.push_back(at + "the plugin reports " + juce::String(rate.reportedAfterAudio) + " samples, measured "
                                           + juce::String(rate.measuredLatency) + " samples");
        }
        if (rate.reportedAfterAudio != rate.reportedAfterPrepare)
        {
            fingerprint.findings.push_back(at + "the reported latency changed after audio (" + juce::String(rate.reportedAfterPrepare) + " after prepare, "
                                           + juce::String(rate.reportedAfterAudio) + " later)");
        }
        if (rate.outputBeforeImpulseDbfs > kNothingDb)
        {
            fingerprint.findings.push_back(at + "output before the impulse arrived (" + juce::String(rate.outputBeforeImpulseDbfs, kLatencyDecimals)
                                           + " dBFS): the plugin makes signal of its own");
        }
    }
    if (fingerprint.reactingParameters.empty())
    {
        fingerprint.findings.push_back("No parameter changed the audio (an instrument, a pure analyser, parameters that act only together, or parameters "
                                       "that are not read after prepare)");
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
        const BlockSizeResult* worst = &fingerprint.blockSizes.front();
        for (const BlockSizeResult& block : fingerprint.blockSizes)
        {
            if (decisionValue(block.steadyState) > decisionValue(worst->steadyState))
            {
                worst = &block;
            }
        }
        fingerprint.findings.push_back("The output depends on the block size also after " + juce::String(settings.blockRenderSeconds, 2)
                                       + " s (largest at block size " + juce::String(worst->blockSize) + ": " + formatDifference(worst->steadyState) + ")");
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

std::vector<SummaryItem> summarize(const PluginFingerprint& fingerprint)
{
    std::vector<SummaryItem> items;
    const auto add = [&items](const juce::String& key, const juce::String& test, const juce::String& result, const juce::String& detail, bool good)
    {
        SummaryItem item;
        item.key = key;
        item.test = test;
        item.result = result;
        item.detail = detail;
        item.good = good;
        items.push_back(item);
    };

    add("loads", "loads and runs with mono or stereo", yesNo(fingerprint.loaded), fingerprint.message, fingerprint.loaded);
    if (! fingerprint.loaded)
    {
        return items;
    }
    juce::String channels;
    if (fingerprint.supportsMono)
    {
        channels = "mono";
    }
    if (fingerprint.supportsStereo)
    {
        if (channels.isNotEmpty())
        {
            channels += ", ";
        }
        channels += "stereo";
    }
    add("channels", "channel layouts (main bus in = out)", channels, {}, true);
    add("parameters", "parameters / changing the audio", juce::String(fingerprint.numberOfParameters) + " / " + juce::String(static_cast<int>(fingerprint.reactingParameters.size())),
        {}, ! fingerprint.reactingParameters.empty());

    // latency: the reference rate in the cell, all rates in the detail
    const RateFingerprint* reference = nullptr;
    bool latencyAgrees = true;
    bool somethingCameOut = true;
    bool preRinging = false;
    bool ownSignal = false;
    juce::String allRates;
    for (const RateFingerprint& rate : fingerprint.rates)
    {
        if (rate.sampleRate == kReferenceRate)
        {
            reference = &rate;
        }
        somethingCameOut = somethingCameOut && rate.outputFound;
        latencyAgrees = latencyAgrees && rate.outputFound && rate.reportedAfterAudio == rate.measuredLatency;
        preRinging = preRinging || (rate.outputFound && rate.outputBeforePeakDb > kNothingDb);
        ownSignal = ownSignal || rate.outputBeforeImpulseDbfs > kNothingDb;
        if (allRates.isNotEmpty())
        {
            allRates += ", ";
        }
        allRates += juce::String(rate.sampleRate / 1000.0, 1) + " kHz: " + juce::String(rate.reportedAfterAudio) + " / ";
        if (rate.outputFound)
        {
            allRates += juce::String(rate.measuredLatency);
        }
        else
        {
            allRates += "-";
        }
    }
    juce::String latencyCell = "not measured";
    if (reference != nullptr && reference->outputFound)
    {
        latencyCell = juce::String(reference->reportedAfterAudio) + " / " + juce::String(reference->measuredLatency);
    }
    else if (reference != nullptr)
    {
        latencyCell = juce::String(reference->reportedAfterAudio) + " / nothing came out";
    }
    add("latency", "latency at 48 kHz, reported / measured (samples)", latencyCell, allRates, somethingCameOut && latencyAgrees);
    add("latencyAgrees", "reported latency = measured at all rates", yesNo(latencyAgrees), allRates, latencyAgrees);
    add("outputBeforePeak", "output before the peak of the impulse response", yesNo(preRinging), {}, true);
    add("ownSignal", "output before the impulse (signal of its own)", yesNo(ownSignal), {}, ! ownSignal);

    int passedWays = 0;
    for (const DeliveryResult& delivery : fingerprint.delivery)
    {
        if (delivery.passed)
        {
            ++passedWays;
        }
    }
    juce::String deliveryCell = "none";
    if (fingerprint.recommendedDelivery.isNotEmpty())
    {
        deliveryCell = juce::String(passedWays) + " of " + juce::String(static_cast<int>(fingerprint.delivery.size())) + " ways";
    }
    add("delivery", "delivery of parameters (A, A, B, A): ways that work", deliveryCell, fingerprint.recommendedDelivery,
        passedWays == static_cast<int>(fingerprint.delivery.size()));

    juce::String worstBlock;
    double worstValue = kFloorDb;
    for (const BlockSizeResult& block : fingerprint.blockSizes)
    {
        if (decisionValue(block.steadyState) > worstValue || worstBlock.isEmpty())
        {
            worstValue = decisionValue(block.steadyState);
            worstBlock = "largest at " + juce::String(block.blockSize) + ": " + formatDifference(block.steadyState);
        }
    }
    add("blockSizes", "block size independent (steady state)", yesNo(fingerprint.blockSizeIndependent), worstBlock, fingerprint.blockSizeIndependent);
    add("deterministic", "deterministic (two instances, bit exact)", yesNo(fingerprint.deterministic), {}, fingerprint.deterministic);
    add("finite", "output stays finite after parameter jumps", yesNo(fingerprint.outputStaysFinite), {}, fingerprint.outputStaysFinite);
    add("recovers", "recovers from parameter jumps", yesNo(fingerprint.recoversFromJumps), {}, fingerprint.recoversFromJumps);
    juce::String idle;
    if (! fingerprint.silenceStaysSilent)
    {
        idle = "peak " + juce::String(fingerprint.idleLevelDb, 1) + " dBFS";
    }
    add("silence", "digital silence in gives digital silence out", yesNo(fingerprint.silenceStaysSilent), idle, fingerprint.silenceStaysSilent);
    return items;
}

juce::String createSummaryJson(const PluginFingerprint& fingerprint)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("plugin", fingerprint.description.name);
    root->setProperty("identifier", fingerprint.description.createIdentifierString());
    root->setProperty("measured", juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M"));
    juce::Array<juce::var> items;
    for (const SummaryItem& item : summarize(fingerprint))
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("key", item.key);
        object->setProperty("test", item.test);
        object->setProperty("result", item.result);
        object->setProperty("detail", item.detail);
        object->setProperty("good", item.good);
        items.add(juce::var(object.release()));
    }
    root->setProperty("summary", items);
    return juce::JSON::toString(juce::var(root.release()));
}

std::vector<SummaryItem> parseSummaryJson(const juce::String& json)
{
    std::vector<SummaryItem> items;
    const juce::var parsed = juce::JSON::parse(json);
    const juce::Array<juce::var>* list = parsed.getProperty("summary", {}).getArray();
    if (list == nullptr)
    {
        return items;
    }
    for (const juce::var& entry : *list)
    {
        SummaryItem item;
        item.key = entry.getProperty("key", {}).toString();
        item.test = entry.getProperty("test", {}).toString();
        item.result = entry.getProperty("result", {}).toString();
        item.detail = entry.getProperty("detail", {}).toString();
        item.good = static_cast<bool>(entry.getProperty("good", true));
        items.push_back(item);
    }
    return items;
}

juce::String createReport(const PluginFingerprint& fingerprint)
{
    const FingerprintSettings& settings = fingerprint.settings;
    juce::String text;
    text << "# Fingerprint: " << fingerprint.description.name << "\n\n";
    text << "- file: `" << fingerprint.description.fileOrIdentifier << "`\n";
    text << "- format: " << fingerprint.description.pluginFormatName << ", manufacturer: " << fingerprint.description.manufacturerName
         << ", version: " << fingerprint.description.version << "\n";
    text << "- measured: " << juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M") << "\n";
    if (fingerprint.settingsWarning.isNotEmpty())
    {
        text << "- settings: " << fingerprint.settingsWarning << "\n";
    }
    if (! fingerprint.loaded)
    {
        text << "- **not measured**: " << fingerprint.message << "\n";
        return text;
    }
    text << "- channels: mono " << yesNo(fingerprint.supportsMono) << ", stereo " << yesNo(fingerprint.supportsStereo) << "\n\n";


    text << "## Summary\n| test | result | detail |\n|---|---|---|\n";
    for (const SummaryItem& item : summarize(fingerprint))
    {
        juce::String result = item.result;
        if (! item.good)
        {
            result = "**" + result + "**";
        }
        text << "| " << item.test << " | " << result << " | " << item.detail << " |\n";
    }
    text << "\nBold: worth a look (see the findings and the details below).\n\n";

    text << "How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB "
            "(0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in "
            "dBFS. \"identical\": bit exact. For a silent reference only the absolute value counts.\n\n";
    text << "## Findings\n";
    if (fingerprint.findings.empty())
    {
        text << "- nothing unusual found\n";
    }
    for (const juce::String& finding : fingerprint.findings)
    {
        text << "- " << finding << "\n";
    }

    text << "\n## Parameters\n";
    text << "Noise (peak " << juce::String(settings.noiseLevel, 2) << ") through the plugin with each parameter at " << juce::String(settings.lowSetting, 2) << " and "
         << juce::String(settings.highSetting, 2) << " of its range, against the plugin at the base setting named in the last column; the larger change is shown. "
         << "\"no\": below " << juce::String(settings.reactsAboveDb, 0) << " dB in both passes. " << fingerprint.numberOfParameters << " parameters";
    if (fingerprint.numberOfParameters > static_cast<int>(fingerprint.parameters.size()))
    {
        text << ", the first " << static_cast<int>(fingerprint.parameters.size()) << " examined";
    }
    text << ".\n\n| no. | name | min | default | max | steps | automatable | changes the audio | measured with |\n|---|---|---|---|---|---|---|---|---|\n";
    for (const ParameterFingerprint& parameter : fingerprint.parameters)
    {
        text << "| " << parameter.index << " | " << parameter.name << " | " << parameter.textAtMinimum << " | " << parameter.textAtDefault << " | "
             << parameter.textAtMaximum << " | " << describeSteps(parameter.numSteps) << " | " << yesNo(parameter.automatable) << " | ";
        if (parameter.changesTheAudio)
        {
            text << formatDifference(parameter.change) << " | " << parameter.measuredWith;
        }
        else
        {
            text << "no | ";
        }
        text << " |\n";
    }

    text << "\n## Latency at three sample rates\n";
    text << "An impulse (1.0 on all channels) after " << settings.impulsePreDelaySamples << " samples of silence, the plugin at its default parameters, "
         << juce::String(settings.latencyObserveSeconds, 2) << " s watched. Measured = position of the largest output sample after the impulse. Reported = "
         << "getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative "
         << "to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.\n\n";
    text << "| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |\n|---|---|---|---|---|---|\n";
    for (const RateFingerprint& rate : fingerprint.rates)
    {
        text << "| " << juce::String(rate.sampleRate, 0) << " Hz | " << rate.reportedAfterPrepare << " | " << rate.reportedAfterAudio << " | ";
        if (rate.outputFound)
        {
            text << rate.measuredLatency;
        }
        else
        {
            text << "nothing came out";
        }
        text << " | ";
        if (rate.outputFound && rate.outputBeforePeakDb > kNothingDb)
        {
            text << juce::String(rate.outputBeforePeakDb, kLatencyDecimals) << " dB";
        }
        else
        {
            text << "none";
        }
        text << " | ";
        if (rate.outputBeforeImpulseDbfs > kNothingDb)
        {
            text << juce::String(rate.outputBeforeImpulseDbfs, kLatencyDecimals) << " dBFS";
        }
        else
        {
            text << "none";
        }
        text << " |\n";
    }

    text << "\n## Delivery of parameters (A, A, B, A)\n";
    text << "Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at "
         << juce::String(settings.highSetting, 2) << "), A. Repeatable: A, A, A the same (below " << juce::String(settings.sameBelowDb, 0) << " dB). Reacts: B differs from A "
         << "(above " << juce::String(settings.differentAboveDb, 0) << " dB). As after a change: A and B equal the outputs of the same settings reached by a change of the "
         << "parameters in the first way.\n\n";
    text << "| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |\n|---|---|---|---|---|---|---|---|\n";
    for (const DeliveryResult& delivery : fingerprint.delivery)
    {
        text << "| " << delivery.name << " | " << yesNo(delivery.repeatable) << " | " << yesNo(delivery.reacts) << " | " << yesNo(delivery.correct) << " | "
             << passText(delivery.passed) << " | " << formatDifference(delivery.secondA) << " ; " << formatDifference(delivery.thirdA) << " | "
             << formatDifference(delivery.reaction) << " | " << formatDifference(delivery.toReferenceA) << " ; " << formatDifference(delivery.toReferenceB) << " |\n";
    }
    if (fingerprint.recommendedDelivery.isNotEmpty())
    {
        text << "\nMost careful way that works: " << fingerprint.recommendedDelivery << ".\n";
    }
    else
    {
        text << "\nNo way of delivering the parameters passed the test.\n";
    }

    text << "\n## Block sizes\n";
    text << juce::String(settings.blockRenderSeconds, 2) << " s of noise through the plugin (setting B) per block size, against block size " << kReferenceBlock
         << ". Steady state: the last " << juce::String(settings.blockCompareSeconds, 2) << " s (decides; below " << juce::String(settings.blockIndependentBelowDb, 0)
         << " dB = independent). Whole: the full render (a plugin that smooths its parameters per block differs here, but not in the steady state).\n\n";
    text << "| block size | steady state | whole |\n|---|---|---|\n";
    for (const BlockSizeResult& block : fingerprint.blockSizes)
    {
        text << "| " << block.blockSize << " | " << formatDifference(block.steadyState) << " | " << formatDifference(block.whole) << " |\n";
    }

    text << "\n## Other\n";
    text << "- deterministic (two instances, the same noise, bit exact): " << yesNo(fingerprint.deterministic) << "\n";
    text << "- output stays finite (no NaN or infinity in the jump test): " << yesNo(fingerprint.outputStaysFinite) << "\n";
    text << "- recovers from parameter jumps (every parameter that changes the audio to 0 and 1 and back, then the output of setting B again): "
         << yesNo(fingerprint.recoversFromJumps) << "\n";
    text << "- digital silence in gives digital silence out: " << yesNo(fingerprint.silenceStaysSilent);
    if (! fingerprint.silenceStaysSilent)
    {
        text << " (peak " << juce::String(fingerprint.idleLevelDb, 1) << " dBFS)";
    }
    text << "\n";

    text << "\n## Settings used\n```\n" << settings.toJson() << "\n```\n";
    return text;
}
}
