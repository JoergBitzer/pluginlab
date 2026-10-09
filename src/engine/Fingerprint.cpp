#include "pluginlab/engine/Fingerprint.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/engine/ChannelAdapter.h"
#include "pluginlab/engine/LatencyMeasurer.h"
#include "PluginRig.h"

namespace pluginlab::engine
{
namespace
{
using detail::Bench;
using detail::getLength;
using detail::makeDifferentNoise;
using detail::makeNoise;
using detail::makePokeValues;
using detail::makeSameNoise;
using detail::makeSilence;
using detail::RenderMode;
using detail::Rig;
using detail::Signal;
using detail::kReferenceRate;
using detail::kReferenceBlock;
using detail::kReferenceSeed;
using detail::kSecondSeed;
using detail::kMillisecondsPerSecond;

constexpr double kRates[] = {44100.0, 48000.0, 96000.0};
constexpr int kNoiseLength = 12288;        // samples of noise per render; the last kCompareLength are compared
constexpr int kCompareLength = 8192;
constexpr double kFloorDb = -200.0;
constexpr double kTiny = 1.0e-20;
constexpr int kMaximumTextLength = 32;
constexpr int kContinuousSteps = 0x7fffffff; // JUCE's number of steps of a continuous parameter
constexpr int kSwitchSteps = 2;
constexpr double kNothingDb = -120.0;      // output before the peak / before the impulse below this is "none"
constexpr int kLatencyDecimals = 1;
constexpr double kNotReached = -1.0;
constexpr double kBaselineMarginDb = 10.0;      // paced / offline count as different only this far above the difference of two fast renders
constexpr double kTimingToleranceMs = 100.0;    // the parameter change reaches B fast and paced within this of each other
constexpr double kLongSegmentSeconds = 1.0;     // the long run is compared in segments of this length

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
    if (difference.channel > 0)
    {
        text += " (channel " + juce::String(difference.channel + 1) + ")";
    }
    return text;
}

// The difference of a from the reference b in one channel, over the samples from "from" to the end
Difference compareChannel(const std::vector<float>& a, const std::vector<float>& b, size_t from, const FingerprintSettings& settings)
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

// The difference of a from the reference b: every channel is compared, the channel with the largest difference is the result
Difference compare(const Signal& a, const Signal& b, size_t from, const FingerprintSettings& settings)
{
    Difference largest;
    bool first = true;
    const size_t channels = std::min(a.size(), b.size());
    for (size_t channel = 0; channel < channels; ++channel)
    {
        Difference difference = compareChannel(a[channel], b[channel], from, settings);
        difference.channel = static_cast<int>(channel);
        if (first || decisionValue(difference) > decisionValue(largest))
        {
            largest = difference;
            first = false;
        }
    }
    return largest;
}

// the level of the silent channel relative to the driven one; "nothing" (the floor) if the silent channel stays exactly silent
double couplingDb(double silentDbfs, double drivenDbfs)
{
    if (silentDbfs <= kFloorDb)
    {
        return kFloorDb;
    }
    return silentDbfs - drivenDbfs;
}

// a level for the report: "silent" for exact silence
juce::String formatLevel(double dbfs)
{
    if (dbfs <= kFloorDb)
    {
        return "silent";
    }
    return juce::String(dbfs, 1) + " dBFS";
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

// a switch or a choice (a parameter with a number of steps), not a continuous one
bool isDiscrete(const ParameterFingerprint& parameter)
{
    return parameter.numSteps < kContinuousSteps;
}

bool isBypass(const ParameterFingerprint& parameter)
{
    return parameter.name.containsIgnoreCase("bypass");
}

// The base for parameters that act only together (all bands of an EQ off by default): every switch away from its default, every choice one step
// on. A bypass stays as it is.
std::vector<float> makeFlippedSetting(const std::vector<float>& defaults, const std::vector<ParameterFingerprint>& parameters)
{
    std::vector<float> flipped = defaults;
    const float middle = 0.5f;
    for (const ParameterFingerprint& parameter : parameters)
    {
        float& value = flipped[static_cast<size_t>(parameter.index)];
        if (isBypass(parameter) || ! isDiscrete(parameter) || parameter.numSteps < kSwitchSteps)
        {
            continue;
        }
        if (parameter.numSteps == kSwitchSteps)
        {
            if (value < middle)
            {
                value = 1.0f;
            }
            else
            {
                value = 0.0f;
            }
            continue;
        }
        const float step = 1.0f / static_cast<float>(parameter.numSteps - 1);
        if (value + step <= 1.0f)
        {
            value += step;
        }
        else
        {
            value -= step;
        }
    }
    return flipped;
}

// "12 ms" or "not within the render"
juce::String describeReach(double milliseconds)
{
    if (milliseconds < 0.0)
    {
        return "not within the render";
    }
    return juce::String(milliseconds, 0) + " ms";
}

// "fast: 0 ms, real-time pace: 21 ms"
juce::String describeChangeTiming(const RealTimeFingerprint& realTime)
{
    return "fast: " + describeReach(realTime.changeReachedFastMs) + ", real-time pace: " + describeReach(realTime.changeReachedPacedMs);
}

// The samples [from, to) of a signal
Signal slice(const Signal& signal, size_t from, size_t to)
{
    Signal part;
    for (const std::vector<float>& channel : signal)
    {
        const size_t end = std::min(to, channel.size());
        const size_t begin = std::min(from, end);
        part.emplace_back(channel.begin() + static_cast<std::ptrdiff_t>(begin), channel.begin() + static_cast<std::ptrdiff_t>(end));
    }
    return part;
}

// Noise with the setting A, in the middle all parameters change to B; returns the time (ms after the change) from which on every block equals
// the reference (the output of B from the start, rendered at real-time pace; below differentAboveDb), or kNotReached
double measureChangeTime(const Bench& bench, const std::vector<float>& settingA, const std::vector<float>& settingB, const Signal& input,
                         const Signal& referenceB, bool paced, const FingerprintSettings& settings)
{
    const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
    if (! rig->isValid())
    {
        return kNotReached;
    }
    rig->prepare();
    if (paced)
    {
        rig->startPacing();
    }
    rig->apply(makePokeValues(settingA, settings));
    rig->process(makeDifferentNoise(kReferenceBlock, settings.noiseLevel));
    rig->apply(settingA);
    rig->settle();
    const size_t length = getLength(input);
    const size_t half = length / 2 / kReferenceBlock * kReferenceBlock;
    rig->process(slice(input, 0, half));
    rig->apply(settingB);
    const Signal after = rig->process(slice(input, half, length));
    const Signal wanted = slice(referenceB, half, length);
    // the last block that still differs; B is reached with the block after it
    size_t reached = 0;
    for (size_t start = 0; start < getLength(after); start += kReferenceBlock)
    {
        const size_t end = start + kReferenceBlock;
        if (isAbove(compare(slice(after, start, end), slice(wanted, start, end), 0, settings), settings.differentAboveDb))
        {
            reached = end;
        }
    }
    if (reached >= getLength(after))
    {
        return kNotReached;
    }
    return kMillisecondsPerSecond * static_cast<double>(reached) / kReferenceRate;
}

// The real-time behaviour (docs/design/W5c-real-time-behaviour.md): offline flag, real-time pace, a parameter change in real time, a long run
RealTimeFingerprint measureRealTime(const Bench& bench, const std::vector<float>& settingA, const std::vector<float>& scannedB,
                                    const std::vector<ParameterFingerprint>& parameters, int channels, const FingerprintSettings& settings)
{
    RealTimeFingerprint result;
    // B as the scan found it; if no parameter changed the audio in the fast scan (a plugin that applies its parameters by a timer, for
    // example), every continuous parameter at the high position
    std::vector<float> settingB = scannedB;
    if (settingB == settingA)
    {
        for (const ParameterFingerprint& parameter : parameters)
        {
            const size_t index = static_cast<size_t>(parameter.index);
            if (index < settingB.size() && ! isDiscrete(parameter))
            {
                settingB[index] = static_cast<float>(settings.highSetting);
            }
        }
        result.ownSettingB = true;
    }
    const int length = static_cast<int>(settings.realTimeSeconds * kReferenceRate);
    Signal input = makeDifferentNoise(length, settings.noiseLevel);
    if (channels < 2)
    {
        input = makeSameNoise(length, settings.noiseLevel);
    }
    const Signal fast = bench.render(kReferenceRate, kReferenceBlock, settingB, input);
    if (fast.empty())
    {
        return result;
    }
    result.measured = true;
    result.baseline = compare(bench.render(kReferenceRate, kReferenceBlock, settingB, input), fast, 0, settings);
    const double limit = std::max(settings.sameBelowDb, decisionValue(result.baseline) + kBaselineMarginDb);

    result.offlineDifference = compare(bench.render(kReferenceRate, kReferenceBlock, settingB, input, RenderMode::Offline), fast, 0, settings);
    result.sameOffline = decisionValue(result.offlineDifference) <= limit;

    if (! settings.realTimeTests)
    {
        return result;
    }
    result.pacedMeasured = true;
    const Signal paced = bench.render(kReferenceRate, kReferenceBlock, settingB, input, RenderMode::Paced);
    result.pacedDifference = compare(paced, fast, 0, settings);
    result.sameWhenPaced = decisionValue(result.pacedDifference) <= limit;

    // the change A -> B can only be timed if two renders of B agree (below differentAboveDb)
    result.changeJudged = ! isAbove(result.baseline, settings.differentAboveDb);
    if (result.changeJudged)
    {
        // the reference is B as it is heard: rendered at real-time pace
        result.changeReachedFastMs = measureChangeTime(bench, settingA, settingB, input, paced, false, settings);
        result.changeReachedPacedMs = measureChangeTime(bench, settingA, settingB, input, paced, true, settings);
        const bool fastReached = result.changeReachedFastMs >= 0.0;
        const bool pacedReached = result.changeReachedPacedMs >= 0.0;
        result.changeTimingAlike = fastReached == pacedReached;
        if (fastReached && pacedReached)
        {
            result.changeTimingAlike = std::abs(result.changeReachedFastMs - result.changeReachedPacedMs) <= kTimingToleranceMs;
        }
    }

    if (settings.longRealTimeSeconds > 0.0)
    {
        result.longMeasured = true;
        const Signal longInput = makeDifferentNoise(static_cast<int>(settings.longRealTimeSeconds * kReferenceRate), settings.noiseLevel);
        const Signal longFast = bench.render(kReferenceRate, kReferenceBlock, settingA, longInput);
        const Signal longPaced = bench.render(kReferenceRate, kReferenceBlock, settingA, longInput, RenderMode::Paced);
        const size_t segment = static_cast<size_t>(kLongSegmentSeconds * kReferenceRate);
        for (size_t start = 0; start < getLength(longFast); start += segment)
        {
            const Difference difference = compare(slice(longPaced, start, start + segment), slice(longFast, start, start + segment), 0, settings);
            if (decisionValue(difference) > limit)
            {
                result.longDifferentSeconds.push_back(static_cast<double>(start) / kReferenceRate);
            }
        }
    }
    return result;
}

// refA, refB: the output of the settings A and B reached by a change of the parameters after prepare (what the plugin does when it is
// used the way a DAW does)
DeliveryResult judgeDelivery(const juce::String& name, const Signal& a1, const Signal& a2, const Signal& b, const Signal& a3, const Signal& refA,
                             const Signal& refB, const FingerprintSettings& settings)
{
    DeliveryResult result;
    result.name = name;
    const size_t from = getLength(a1) - static_cast<size_t>(kCompareLength);
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

// the main-bus layouts that are asked for (decision of the author, W5b: the common ones in the wild)
const juce::String kMonoName = "mono";
const juce::String kStereoName = "stereo";

std::vector<LayoutFingerprint> checkLayouts(juce::AudioPluginInstance& instance)
{
    struct Candidate
    {
        juce::String name;
        juce::AudioChannelSet input;
        juce::AudioChannelSet output;
    };
    const std::vector<Candidate> candidates = {
        {kMonoName, juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono()},
        {kStereoName, juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()},
        {"mono in, stereo out", juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()},
        {"LCR", juce::AudioChannelSet::createLCR(), juce::AudioChannelSet::createLCR()},
        {"quad", juce::AudioChannelSet::quadraphonic(), juce::AudioChannelSet::quadraphonic()},
        {"5.1", juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1()},
        {"7.1", juce::AudioChannelSet::create7point1(), juce::AudioChannelSet::create7point1()},
        {"ambisonics 1st order", juce::AudioChannelSet::ambisonic(1), juce::AudioChannelSet::ambisonic(1)}};
    std::vector<LayoutFingerprint> results;
    const juce::AudioProcessor::BusesLayout base = instance.getBusesLayout();
    for (const Candidate& candidate : candidates)
    {
        LayoutFingerprint result;
        result.name = candidate.name;
        if (! base.inputBuses.isEmpty() && ! base.outputBuses.isEmpty())
        {
            for (const bool keepOtherBuses : {false, true})
            {
                juce::AudioProcessor::BusesLayout layout = base;
                for (int bus = 0; bus < layout.inputBuses.size(); ++bus)
                {
                    if (bus == 0)
                    {
                        layout.inputBuses.getReference(bus) = candidate.input;
                    }
                    else if (! keepOtherBuses)
                    {
                        layout.inputBuses.getReference(bus) = juce::AudioChannelSet::disabled();
                    }
                }
                for (int bus = 0; bus < layout.outputBuses.size(); ++bus)
                {
                    if (bus == 0)
                    {
                        layout.outputBuses.getReference(bus) = candidate.output;
                    }
                    else if (! keepOtherBuses)
                    {
                        layout.outputBuses.getReference(bus) = juce::AudioChannelSet::disabled();
                    }
                }
                result.accepted = result.accepted || instance.checkBusesLayoutSupported(layout);
            }
        }
        results.push_back(result);
    }
    return results;
}
}

namespace pluginlab::engine
{
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

    // ---- buses and layouts, from an instance as it is created ----
    {
        juce::String error;
        const std::unique_ptr<juce::AudioPluginInstance> raw = formatManager.createPluginInstance(description, kReferenceRate, kReferenceBlock, error);
        if (raw != nullptr)
        {
            for (const bool isInput : {true, false})
            {
                for (int index = 0; index < raw->getBusCount(isInput); ++index)
                {
                    const juce::AudioProcessor::Bus* bus = raw->getBus(isInput, index);
                    BusFingerprint entry;
                    entry.isInput = isInput;
                    entry.index = index;
                    entry.name = bus->getName();
                    entry.defaultLayout = bus->getCurrentLayout().getDescription();
                    if (bus->getCurrentLayout().isDisabled())
                    {
                        entry.defaultLayout = "disabled";
                    }
                    fingerprint.buses.push_back(entry);
                }
            }
            fingerprint.hasSideChain = raw->getBusCount(true) > 1;
            fingerprint.acceptsMidi = raw->acceptsMidi();
            fingerprint.producesMidi = raw->producesMidi();
            fingerprint.layouts = checkLayouts(*raw);
            for (const LayoutFingerprint& layout : fingerprint.layouts)
            {
                if (layout.name == kMonoName && layout.accepted)
                {
                    fingerprint.supportsMono = true;
                }
                if (layout.name == kStereoName && layout.accepted)
                {
                    fingerprint.supportsStereo = true;
                }
            }
        }
    }
    fingerprint.isInstrument = description.isInstrument;
    fingerprint.measuredChannels = first->getChannels();

    // ---- parameters ----
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

    // ---- which parameters change the audio: with L = R, and (more than one channel) with L != R ----
    const bool twoSignals = fingerprint.measuredChannels > 1;
    const Signal sameNoise = makeSameNoise(kNoiseLength, settings.noiseLevel);
    const Signal noise = makeDifferentNoise(kNoiseLength, settings.noiseLevel); // the signal of all other measurements
    const size_t compareFrom = static_cast<size_t>(kNoiseLength - kCompareLength);
    const Signal baseline = bench.render(kReferenceRate, kReferenceBlock, defaults, noise);
    // Pass 1 at the defaults. Pass 2: a parameter can have no effect at the defaults (the frequency of an EQ whose gain is 0 dB), so the
    // others are tried again with the parameters that reacted moved to the high position.
    const auto scan = [&](const std::vector<float>& base, bool onlyUnreacted, const juce::String& context)
    {
        const Signal referenceSame = bench.render(kReferenceRate, kReferenceBlock, base, sameNoise);
        Signal referenceDifferent;
        if (twoSignals)
        {
            referenceDifferent = bench.render(kReferenceRate, kReferenceBlock, base, noise);
        }
        for (ParameterFingerprint& entry : fingerprint.parameters)
        {
            if (onlyUnreacted && entry.changesTheAudio)
            {
                continue;
            }
            Difference largestSame;
            Difference largestDifferent;
            for (const float value : {low, high})
            {
                std::vector<float> setting = base;
                setting[static_cast<size_t>(entry.index)] = value;
                const Difference same = compare(bench.render(kReferenceRate, kReferenceBlock, setting, sameNoise), referenceSame, compareFrom, settings);
                if (decisionValue(same) > decisionValue(largestSame))
                {
                    largestSame = same;
                }
                if (twoSignals)
                {
                    const Difference different = compare(bench.render(kReferenceRate, kReferenceBlock, setting, noise), referenceDifferent, compareFrom, settings);
                    if (decisionValue(different) > decisionValue(largestDifferent))
                    {
                        largestDifferent = different;
                    }
                }
            }
            Difference largest = largestSame;
            if (decisionValue(largestDifferent) > decisionValue(largest))
            {
                largest = largestDifferent;
            }
            if (decisionValue(largest) > decisionValue(entry.change) || entry.measuredWith.isEmpty())
            {
                entry.change = largest;
                entry.changeSame = largestSame;
                entry.changeDifferent = largestDifferent;
                entry.measuredWith = context;
            }
            entry.changesTheAudio = isAbove(entry.change, settings.reactsAboveDb);
        }
    };
    const auto anyReacts = [&fingerprint]
    {
        for (const ParameterFingerprint& entry : fingerprint.parameters)
        {
            if (entry.changesTheAudio)
            {
                return true;
            }
        }
        return false;
    };
    // pass 1 from a base, pass 2 with the parameters that reacted moved to the high position
    const auto runPasses = [&](const std::vector<float>& base, const juce::String& baseText)
    {
        scan(base, false, baseText);
        std::vector<float> moved = base;
        for (const ParameterFingerprint& entry : fingerprint.parameters)
        {
            if (entry.changesTheAudio && ! isSwitch(entry))
            {
                moved[static_cast<size_t>(entry.index)] = high;
            }
        }
        if (moved != base)
        {
            scan(moved, true, baseText + ", the others at " + juce::String(settings.highSetting, 2));
        }
    };
    std::vector<float> scanBase = defaults;
    fingerprint.scanBase = "the defaults";
    runPasses(defaults, "defaults");
    // nothing at the defaults: parameters that act only together (a band switch and its gain): the switches flipped
    if (! anyReacts())
    {
        const std::vector<float> flipped = makeFlippedSetting(defaults, fingerprint.parameters);
        if (flipped != defaults)
        {
            runPasses(flipped, "switches flipped");
            if (anyReacts())
            {
                scanBase = flipped;
                fingerprint.scanBase = "every switch away from its default and every choice one step on (nothing changed the audio at the defaults)";
            }
        }
    }
    // still nothing: one switch together with one continuous parameter
    if (! anyReacts())
    {
        int pairs = 0;
        for (const ParameterFingerprint& switchEntry : fingerprint.parameters)
        {
            if (! isSwitch(switchEntry) || isBypass(switchEntry))
            {
                continue;
            }
            std::vector<float> base = defaults;
            base[static_cast<size_t>(switchEntry.index)] = 1.0f - base[static_cast<size_t>(switchEntry.index)];
            const Signal reference = bench.render(kReferenceRate, kReferenceBlock, base, noise);
            for (ParameterFingerprint& entry : fingerprint.parameters)
            {
                if (isDiscrete(entry) || pairs >= settings.maximumPairs)
                {
                    continue;
                }
                ++pairs;
                for (const float value : {low, high})
                {
                    std::vector<float> setting = base;
                    setting[static_cast<size_t>(entry.index)] = value;
                    const Difference change = compare(bench.render(kReferenceRate, kReferenceBlock, setting, noise), reference, compareFrom, settings);
                    if (isAbove(change, settings.reactsAboveDb) && decisionValue(change) > decisionValue(entry.change))
                    {
                        entry.change = change;
                        entry.changeDifferent = change;
                        entry.changesTheAudio = true;
                        entry.measuredWith = "with " + switchEntry.name + " flipped";
                    }
                }
            }
            if (anyReacts())
            {
                scanBase = base;
                fingerprint.scanBase = switchEntry.name + " flipped (no single parameter changed the audio, a switch together with a parameter did)";
                break;
            }
        }
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
    std::vector<float> settingB = scanBase;
    for (const int index : fingerprint.reactingParameters)
    {
        if (isSwitch(fingerprint.parameters[static_cast<size_t>(index)]))
        {
            continue;
        }
        std::vector<float> candidate = settingB;
        candidate[static_cast<size_t>(index)] = high;
        const Signal output = bench.render(kReferenceRate, kReferenceBlock, candidate, noise);
        if (isAbove(compare(output, baseline, compareFrom, settings), settings.differentAboveDb))
        {
            settingB = candidate;
        }
    }

    for (const ParameterFingerprint& entry : fingerprint.parameters)
    {
        const size_t index = static_cast<size_t>(entry.index);
        if (settingB[index] == defaults[index])
        {
            continue;
        }
        SettingEntry setting;
        setting.index = entry.index;
        setting.name = entry.name;
        setting.valueA = defaults[index];
        setting.textA = parameters[entry.index]->getText(defaults[index], kMaximumTextLength);
        setting.valueB = settingB[index];
        setting.textB = parameters[entry.index]->getText(settingB[index], kMaximumTextLength);
        fingerprint.settingB.push_back(setting);
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
    Signal referenceA;
    Signal referenceB;
    {
        const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
        rig->prepare();
        rig->settle();
        rig->apply(defaults);
        const Signal a1 = rig->process(noise);
        const Signal a2 = rig->process(noise);
        rig->apply(settingB);
        rig->settle(); // a plugin that smooths its parameters needs the time, as after every other change in the measurement
        const Signal b = rig->process(noise);
        rig->apply(defaults);
        rig->settle();
        const Signal a3 = rig->process(noise);
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
    fingerprint.streamReacts = fingerprint.delivery.front().reacts;
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
    const Signal longNoise = makeDifferentNoise(blockRenderLength, settings.noiseLevel);
    const Signal reference = bench.render(kReferenceRate, kReferenceBlock, settingB, longNoise);
    for (const int blockSize : settings.blockSizes)
    {
        BlockSizeResult result;
        result.blockSize = blockSize;
        const Signal output = bench.render(kReferenceRate, blockSize, settingB, longNoise);
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
        const Signal one = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        const Signal two = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        const Difference difference = compare(one, two, 0, settings);
        fingerprint.deterministic = ! one.empty() && difference.absoluteDbfs <= kFloorDb;
    }

    // ---- time invariance: the same noise twice through one instance, with silence between; settled for a long time first, so that a slow
    // parameter smoothing does not count as time-varying. The first output is also the reference for "settles in time". ----
    {
        const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
        rig->prepare();
        rig->apply(makePokeValues(settingB, settings));
        rig->process(makeDifferentNoise(kReferenceBlock, settings.noiseLevel));
        rig->apply(settingB);
        rig->settleFor(settings.longSettleSeconds);
        const Signal first = rig->process(noise);
        const Signal afterShortSettle = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        fingerprint.settleDifference = compare(afterShortSettle, first, compareFrom, settings);
        fingerprint.settlesInTime = isBelow(fingerprint.settleDifference, settings.sameBelowDb);
        rig->process(makeSilence(static_cast<int>(kReferenceRate * settings.timeInvarianceGapSeconds)));
        const Signal second = rig->process(noise);
        fingerprint.timeInvarianceDifference = compare(second, first, compareFrom, settings);
        fingerprint.timeInvariant = isBelow(fingerprint.timeInvarianceDifference, settings.sameBelowDb);
    }

    // ---- robustness: the reacting parameters to both ends and back, then B again; continuous ones and switches/choices separately ----
    {
        const Signal reachedB = bench.render(kReferenceRate, kReferenceBlock, settingB, noise);
        const Signal oneBlock = makeDifferentNoise(kReferenceBlock, settings.noiseLevel);
        // returns false if the output did not come back; finite is false after a NaN or infinity
        const auto jumpTest = [&](bool discrete, bool& finite)
        {
            const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
            rig->prepare();
            rig->apply(settingB);
            rig->settle();
            rig->process(noise);
            int jumped = 0;
            for (const int index : fingerprint.reactingParameters)
            {
                if (isDiscrete(fingerprint.parameters[static_cast<size_t>(index)]) != discrete)
                {
                    continue;
                }
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
            const Signal afterwards = rig->process(noise);
            finite = ! rig->sawNonFinite();
            return finite && ! reachedB.empty() && isBelow(compare(afterwards, reachedB, compareFrom, settings), settings.sameBelowDb);
        };
        bool continuousFinite = true;
        bool discreteFinite = true;
        fingerprint.recoversContinuous = jumpTest(false, continuousFinite);
        for (const int index : fingerprint.reactingParameters)
        {
            fingerprint.discreteJumped = fingerprint.discreteJumped || isDiscrete(fingerprint.parameters[static_cast<size_t>(index)]);
        }
        if (fingerprint.discreteJumped)
        {
            fingerprint.recoversDiscrete = jumpTest(true, discreteFinite);
        }
        fingerprint.outputStaysFinite = continuousFinite && discreteFinite;
        fingerprint.recoversFromJumps = fingerprint.recoversContinuous && fingerprint.recoversDiscrete;
    }

    // ---- silence ----
    {
        const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
        rig->prepare();
        rig->apply(settingB);
        rig->settle();
        const Signal output = rig->process(makeSilence(kCompareLength));
        float peak = 0.0f;
        for (const std::vector<float>& channel : output)
        {
            for (const float sample : channel)
            {
                peak = std::max(peak, std::abs(sample));
            }
        }
        fingerprint.silenceStaysSilent = peak == 0.0f || toDb(peak) < settings.silenceBelowDbfs;
        fingerprint.idleLevelDb = toDb(peak);
    }

    // ---- channel coupling (two channels): one input driven, the other silent, at the setting B ----
    if (fingerprint.measuredChannels == 2)
    {
        const std::vector<float> driven = makeNoise(kNoiseLength, settings.noiseLevel, kReferenceSeed);
        const std::vector<float> quiet(static_cast<size_t>(kNoiseLength), 0.0f);
        const Signal leftOnly = bench.render(kReferenceRate, kReferenceBlock, settingB, Signal{driven, quiet});
        const Signal rightOnly = bench.render(kReferenceRate, kReferenceBlock, settingB, Signal{quiet, driven});
        if (leftOnly.size() == 2 && rightOnly.size() == 2)
        {
            // the silent output against the driven one: "relative" = how loud the other channel is, relative to the driven channel
            fingerprint.couplingLeftToRight = compareChannel(leftOnly[1], quiet, compareFrom, settings);
            const Difference leftLevel = compareChannel(leftOnly[0], quiet, compareFrom, settings);
            fingerprint.couplingLeftToRight.relativeDb = couplingDb(fingerprint.couplingLeftToRight.absoluteDbfs, leftLevel.absoluteDbfs);
            fingerprint.couplingLeftToRight.referenceSilent = false;
            fingerprint.couplingLeftToRight.channel = 1;
            fingerprint.couplingRightToLeft = compareChannel(rightOnly[0], quiet, compareFrom, settings);
            const Difference rightLevel = compareChannel(rightOnly[1], quiet, compareFrom, settings);
            fingerprint.couplingRightToLeft.relativeDb = couplingDb(fingerprint.couplingRightToLeft.absoluteDbfs, rightLevel.absoluteDbfs);
            fingerprint.couplingRightToLeft.referenceSilent = false;
            fingerprint.couplingRightToLeft.channel = 0;
            fingerprint.couplingMeasured = true;
            fingerprint.channelsIndependent = fingerprint.couplingLeftToRight.relativeDb < settings.couplingBelowDb
                                           && fingerprint.couplingRightToLeft.relativeDb < settings.couplingBelowDb;
        }
    }

    // ---- real-time behaviour: offline flag, real-time pace, a parameter change in real time (W5c) ----
    fingerprint.realTime = measureRealTime(bench, scanBase, settingB, fingerprint.parameters, fingerprint.measuredChannels, settings);

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
    juce::String expected;
    if (! fingerprint.timeInvariant)
    {
        expected = " (expected for a time-varying plugin)";
        fingerprint.findings.push_back("The plugin is time-varying: the same noise twice through one instance, with " + juce::String(settings.timeInvarianceGapSeconds, 2)
                                       + " s of silence between, gives different output (" + formatDifference(fingerprint.timeInvarianceDifference)
                                       + "): an LFO, a random element, dither, a noise generator or a slow envelope. Repeatability, determinism, recovery and "
                                         "silence cannot be judged as for a time-invariant plugin.");
    }
    if (fingerprint.timeInvariant && ! fingerprint.settlesInTime)
    {
        expected = " (possibly because of the slow settling)";
        fingerprint.findings.push_back("After a parameter change the plugin needs longer than " + juce::String(settings.settleSeconds, 2) + " s to settle: the output "
                                       "then differs from the output after " + juce::String(settings.longSettleSeconds, 2) + " s ("
                                       + formatDifference(fingerprint.settleDifference) + "), a slow parameter smoothing or envelope. Tests that follow a change "
                                         "(delivery, recovery) can fail because of it; a larger settleSeconds in the settings shows whether they then pass.");
    }
    if (! fingerprint.streamReacts && ! fingerprint.reactingParameters.empty())
    {
        fingerprint.findings.push_back("In the stream way the setting B did not change the output: the plugin ignores parameter changes while it runs; the "
                                       "references of the other ways of delivery are therefore not meaningful");
    }
    for (const DeliveryResult& delivery : fingerprint.delivery)
    {
        if (! delivery.passed && ! fingerprint.reactingParameters.empty())
        {
            fingerprint.findings.push_back("Delivery '" + delivery.name + "' fails: " + delivery.comment + expected);
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
        fingerprint.findings.push_back("Two instances with the same input give different output" + expected);
    }
    if (! fingerprint.outputStaysFinite)
    {
        fingerprint.findings.push_back("The output contained NaN or infinity after parameter jumps");
    }
    else if (! fingerprint.recoversFromJumps)
    {
        juce::String group = "the continuous parameters";
        if (fingerprint.recoversContinuous)
        {
            group = "the switches and choices";
        }
        else if (! fingerprint.recoversDiscrete)
        {
            group = "the continuous parameters and the switches and choices";
        }
        fingerprint.findings.push_back("After jumps of " + group + " to both ends the output does not come back to what it was" + expected);
    }
    if (! fingerprint.silenceStaysSilent)
    {
        fingerprint.findings.push_back("Digital silence in does not give digital silence out (peak " + juce::String(fingerprint.idleLevelDb, 1) + " dBFS)" + expected);
    }
    const RealTimeFingerprint& realTime = fingerprint.realTime;
    if (realTime.measured && ! realTime.sameOffline)
    {
        fingerprint.findings.push_back("The output differs when the host renders offline (offline flag; " + formatDifference(realTime.offlineDifference)
                                       + "): the plugin switches its algorithm or quality for offline rendering, so an offline render (bounce) is not what is "
                                         "heard while playing. All other measurements use real-time mode.");
    }
    if (realTime.pacedMeasured && ! realTime.sameWhenPaced)
    {
        fingerprint.findings.push_back("The output at real-time pace (the message loop running between the blocks) differs from the fast render ("
                                       + formatDifference(realTime.pacedDifference) + "): the plugin depends on wall-clock time or on its message "
                                         "thread (timers, background work). The fast measurements may not show what is heard.");
    }
    if (realTime.changeJudged && ! realTime.changeTimingAlike)
    {
        fingerprint.findings.push_back("A parameter change reaches the audio differently fast and at real-time pace (" + describeChangeTiming(realTime)
                                       + "): the plugin applies parameter changes on its message thread (a timer, a listener) or on a background thread; "
                                         "the measurements that follow a change in a fast render are therefore not meaningful.");
    }
    if (! realTime.longDifferentSeconds.empty())
    {
        juce::StringArray seconds;
        for (const double start : realTime.longDifferentSeconds)
        {
            seconds.add(juce::String(start, 0));
        }
        fingerprint.findings.push_back("In the long run at real-time pace the output differs from the fast render in the 1 s segments starting at "
                                       + seconds.joinIntoString(", ") + " s (demo noise, a wall-clock event?)");
    }
    return fingerprint;
}

// "the other channel stays silent" or "-6.0 dB re the driven channel (-29.0 dBFS)"
juce::String describeCoupling(const Difference& coupling)
{
    if (coupling.absoluteDbfs <= kFloorDb)
    {
        return "silent";
    }
    return juce::String(coupling.relativeDb, 1) + " dB re the driven channel (" + formatLevel(coupling.absoluteDbfs) + ")";
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
    juce::String layouts;
    for (const LayoutFingerprint& layout : fingerprint.layouts)
    {
        if (! layout.accepted)
        {
            continue;
        }
        if (layouts.isNotEmpty())
        {
            layouts += ", ";
        }
        layouts += layout.name;
    }
    add("layouts", "main-bus layouts accepted", layouts, "measured with " + juce::String(fingerprint.measuredChannels) + " channel(s)", true);
    juce::String sideChainDetail;
    for (const BusFingerprint& bus : fingerprint.buses)
    {
        if (bus.isInput && bus.index > 0)
        {
            if (sideChainDetail.isNotEmpty())
            {
                sideChainDetail += ", ";
            }
            sideChainDetail += bus.name + " (" + bus.defaultLayout + ")";
        }
    }
    add("sideChain", "side-chain input (more than one input bus)", yesNo(fingerprint.hasSideChain), sideChainDetail, true);
    juce::String midi = "none";
    if (fingerprint.acceptsMidi && fingerprint.producesMidi)
    {
        midi = "in and out";
    }
    else if (fingerprint.acceptsMidi)
    {
        midi = "in";
    }
    else if (fingerprint.producesMidi)
    {
        midi = "out";
    }
    add("midi", "MIDI", midi, {}, true);
    if (fingerprint.couplingMeasured)
    {
        add("coupling", "channels independent (one input driven, the other silent)", yesNo(fingerprint.channelsIndependent),
            "L to R: " + describeCoupling(fingerprint.couplingLeftToRight) + ", R to L: " + describeCoupling(fingerprint.couplingRightToLeft),
            true);
    }
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
    juce::String deliveryDetail = fingerprint.recommendedDelivery;
    if (! fingerprint.timeInvariant)
    {
        deliveryDetail = "time-varying: the repeatability of A cannot be judged";
    }
    add("delivery", "delivery of parameters (A, A, B, A): ways that work", deliveryCell, deliveryDetail,
        passedWays == static_cast<int>(fingerprint.delivery.size()) || ! fingerprint.timeInvariant);

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
    const bool timeVarying = ! fingerprint.timeInvariant;
    juce::String expectedNote;
    if (timeVarying)
    {
        expectedNote = "expected for a time-varying plugin";
    }
    juce::String invariance = "yes";
    if (timeVarying)
    {
        invariance = "no (time-varying)";
    }
    add("timeInvariant", "time-invariant (the same noise twice through one instance)", invariance, formatDifference(fingerprint.timeInvarianceDifference), true);
    add("settles", "settles within " + juce::String(fingerprint.settings.settleSeconds, 2) + " s after a parameter change", yesNo(fingerprint.settlesInTime),
        formatDifference(fingerprint.settleDifference), fingerprint.settlesInTime || timeVarying);
    add("blockSizes", "block size independent (steady state)", yesNo(fingerprint.blockSizeIndependent), worstBlock, fingerprint.blockSizeIndependent);
    add("deterministic", "deterministic (two instances, bit exact)", yesNo(fingerprint.deterministic), expectedNote, fingerprint.deterministic || timeVarying);
    add("finite", "output stays finite after parameter jumps", yesNo(fingerprint.outputStaysFinite), {}, fingerprint.outputStaysFinite);
    juce::String recoveryDetail = "continuous parameters: " + yesNo(fingerprint.recoversContinuous);
    if (fingerprint.discreteJumped)
    {
        recoveryDetail += ", switches and choices: " + yesNo(fingerprint.recoversDiscrete);
    }
    if (timeVarying)
    {
        recoveryDetail += "; " + expectedNote;
    }
    add("recovers", "recovers from parameter jumps", yesNo(fingerprint.recoversFromJumps), recoveryDetail, fingerprint.recoversFromJumps || timeVarying);
    juce::String idle;
    if (! fingerprint.silenceStaysSilent)
    {
        idle = "peak " + juce::String(fingerprint.idleLevelDb, 1) + " dBFS";
    }
    if (timeVarying && ! fingerprint.silenceStaysSilent)
    {
        idle += "; " + expectedNote;
    }
    add("silence", "digital silence in gives digital silence out", yesNo(fingerprint.silenceStaysSilent), idle, fingerprint.silenceStaysSilent || timeVarying);

    const RealTimeFingerprint& realTime = fingerprint.realTime;
    if (! realTime.measured)
    {
        return items;
    }
    add("offline", "the same when the host renders offline (offline flag)", yesNo(realTime.sameOffline), formatDifference(realTime.offlineDifference),
        realTime.sameOffline);
    if (! realTime.pacedMeasured)
    {
        add("realTimePace", "the same at real-time pace (message loop running)", "not measured", "realTimeTests is off in the settings", true);
        return items;
    }
    add("realTimePace", "the same at real-time pace (message loop running)", yesNo(realTime.sameWhenPaced), formatDifference(realTime.pacedDifference),
        realTime.sameWhenPaced);
    if (realTime.changeJudged)
    {
        juce::String timing = "alike";
        if (! realTime.changeTimingAlike)
        {
            timing = "different";
        }
        add("changeTiming", "a parameter change reaches the audio, fast and at real-time pace", timing, describeChangeTiming(realTime), realTime.changeTimingAlike);
    }
    else
    {
        add("changeTiming", "a parameter change reaches the audio, fast and at real-time pace", "not judged",
            "two renders of B differ (" + formatDifference(realTime.baseline) + ")", true);
    }
    if (realTime.longMeasured)
    {
        add("longRun", "long run at real-time pace the same as fast", yesNo(realTime.longDifferentSeconds.empty()),
            juce::String(static_cast<int>(realTime.longDifferentSeconds.size())) + " segment(s) differ", realTime.longDifferentSeconds.empty());
    }
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

// a change in the parameter table: the difference, or "no" if it is below the threshold
juce::String describeChange(const Difference& change, const FingerprintSettings& settings)
{
    if (! isAbove(change, settings.reactsAboveDb))
    {
        return "no";
    }
    return formatDifference(change);
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
    text << "\nBold (orange on the Developer page): worth a look (see the findings and the details below).\n\n";

    text << "How to read the differences: every difference is given as relative / absolute: relative = RMS(output - reference) / RMS(reference) in dB "
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

    text << "\n## Channels and buses\n";
    text << "The buses of the plugin as it is created, the main-bus layouts it accepts (other buses switched off where possible), MIDI, and the coupling of the "
            "channels at the setting B: one input channel gets noise, the other silence; the output of the silent channel relative to the output of the driven one "
            "(below " << juce::String(settings.couplingBelowDb, 0) << " dB = independent channels). The measurement runs with " << fingerprint.measuredChannels
         << " channel(s); other channels of the plugin get silence.\n\n";
    text << "| bus | name | default layout |\n|---|---|---|\n";
    for (const BusFingerprint& bus : fingerprint.buses)
    {
        juce::String kind = "output ";
        if (bus.isInput)
        {
            kind = "input ";
        }
        text << "| " << kind << bus.index << " | " << bus.name << " | " << bus.defaultLayout << " |\n";
    }
    text << "\n| layout | accepted |\n|---|---|\n";
    for (const LayoutFingerprint& layout : fingerprint.layouts)
    {
        text << "| " << layout.name << " | " << yesNo(layout.accepted) << " |\n";
    }
    text << "\n- side chain: " << yesNo(fingerprint.hasSideChain) << "; MIDI in: " << yesNo(fingerprint.acceptsMidi) << ", out: " << yesNo(fingerprint.producesMidi)
         << "; instrument: " << yesNo(fingerprint.isInstrument) << "\n";
    if (fingerprint.couplingMeasured)
    {
        text << "- coupling L to R: " << describeCoupling(fingerprint.couplingLeftToRight) << ", R to L: " << describeCoupling(fingerprint.couplingRightToLeft)
             << ": channels independent " << yesNo(fingerprint.channelsIndependent) << "\n";
    }

    text << "\n## Parameters\n";
    text << "Noise (peak " << juce::String(settings.noiseLevel, 2) << ") through the plugin with each parameter at " << juce::String(settings.lowSetting, 2) << " and "
         << juce::String(settings.highSetting, 2) << " of its range, against the plugin at the base setting named in the last column; the larger change is shown. "
         << "\"no\": below " << juce::String(settings.reactsAboveDb, 0) << " dB in both passes. " << fingerprint.numberOfParameters << " parameters";
    if (fingerprint.numberOfParameters > static_cast<int>(fingerprint.parameters.size()))
    {
        text << ", the first " << static_cast<int>(fingerprint.parameters.size()) << " examined";
    }
    text << ". Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a "
            "width or mid/side control reacts only to this one). The scan started from " << fingerprint.scanBase << ".\n\n| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |\n"
            "|---|---|---|---|---|---|---|---|---|---|\n";
    for (const ParameterFingerprint& parameter : fingerprint.parameters)
    {
        text << "| " << parameter.index << " | " << parameter.name << " | " << parameter.textAtMinimum << " | " << parameter.textAtDefault << " | "
             << parameter.textAtMaximum << " | " << describeSteps(parameter.numSteps) << " | " << yesNo(parameter.automatable) << " | ";
        if (parameter.changesTheAudio)
        {
            text << describeChange(parameter.changeSame, settings) << " | ";
            if (fingerprint.measuredChannels > 1)
            {
                text << describeChange(parameter.changeDifferent, settings);
            }
            else
            {
                text << "-";
            }
            text << " | " << parameter.measuredWith;
        }
        else
        {
            text << "no | no | ";
        }
        text << " |\n";
    }

    text << "\n## The settings A and B\n";
    text << "A = the defaults. B = the parameters that change the audio at " << juce::String(settings.highSetting, 2) << " of their range (switches left out, starting from "
         << fingerprint.scanBase << "); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters "
         << "where B differs from A are listed.\n\n";
    if (fingerprint.settingB.empty())
    {
        text << "B equals A (no parameter changes the audio).\n";
    }
    else
    {
        text << "| no. | name | A (normalised) | A | B (normalised) | B |\n|---|---|---|---|---|---|\n";
        for (const SettingEntry& entry : fingerprint.settingB)
        {
            text << "| " << entry.index << " | " << entry.name << " | " << juce::String(entry.valueA, 3) << " | " << entry.textA << " | "
                 << juce::String(entry.valueB, 3) << " | " << entry.textB << " |\n";
        }
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
    text << "- time-invariant (one instance: settled for " << juce::String(settings.longSettleSeconds, 2) << " s, noise, " << juce::String(settings.timeInvarianceGapSeconds, 2) << " s silence, the same noise again; the two outputs the same): "
         << yesNo(fingerprint.timeInvariant) << " (" << formatDifference(fingerprint.timeInvarianceDifference) << ")\n";
    text << "- settles within " << juce::String(settings.settleSeconds, 2) << " s after a parameter change (the output then against the output after "
         << juce::String(settings.longSettleSeconds, 2) << " s): " << yesNo(fingerprint.settlesInTime) << " (" << formatDifference(fingerprint.settleDifference) << ")\n";
    text << "- deterministic (two instances, the same noise, bit exact): " << yesNo(fingerprint.deterministic) << "\n";
    text << "- output stays finite (no NaN or infinity in the jump test): " << yesNo(fingerprint.outputStaysFinite) << "\n";
    text << "- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters "
            "and switches/choices in separate runs): continuous " << yesNo(fingerprint.recoversContinuous);
    if (fingerprint.discreteJumped)
    {
        text << ", switches and choices " << yesNo(fingerprint.recoversDiscrete);
    }
    text << "\n";
    text << "- digital silence in gives digital silence out: " << yesNo(fingerprint.silenceStaysSilent);
    if (! fingerprint.silenceStaysSilent)
    {
        text << " (peak " << juce::String(fingerprint.idleLevelDb, 1) << " dBFS)";
    }
    text << "\n";

    const RealTimeFingerprint& realTime = fingerprint.realTime;
    if (realTime.measured)
    {
        text << "\n## Real-time behaviour\n";
        text << "All other measurements render as fast as possible, in real-time mode (offline flag off) and without letting the message thread run. "
                "Here the setting B with " << juce::String(settings.realTimeSeconds, 1) << " s of noise is rendered again (fresh instances, as above): "
                "with the offline flag, and at real-time pace (after every block the message loop runs until the wall clock has caught up with the "
                "audio, so that timers and asynchronous updates of the plugin run as in a DAW). Different = more than " << juce::String(kBaselineMarginDb, 0)
             << " dB above the difference of two fast renders (" << formatDifference(realTime.baseline) << ") and above " << juce::String(settings.sameBelowDb, 0)
             << " dB.\n\n";
        if (realTime.ownSettingB)
        {
            text << "No parameter changed the audio in the (fast) scan: here B is every continuous parameter at the high position.\n\n";
        }
        text << "- offline flag on against off: " << formatDifference(realTime.offlineDifference) << " - the same: " << yesNo(realTime.sameOffline) << "\n";
        if (realTime.pacedMeasured)
        {
            text << "- real-time pace against fast: " << formatDifference(realTime.pacedDifference) << " - the same: " << yesNo(realTime.sameWhenPaced) << "\n";
            text << "- a parameter change in the middle of the noise (all parameters A -> B), until every block equals the output of B rendered at real-time pace (below "
                 << juce::String(settings.differentAboveDb, 0) << " dB): ";
            if (realTime.changeJudged)
            {
                text << describeChangeTiming(realTime) << "\n";
            }
            else
            {
                text << "not judged (two renders of B differ)\n";
            }
        }
        else
        {
            text << "- real-time pace: not measured (realTimeTests is off in the settings)\n";
        }
        if (realTime.longMeasured)
        {
            text << "- long run (" << juce::String(settings.longRealTimeSeconds, 0) << " s, setting A, real-time pace against fast): "
                 << static_cast<int>(realTime.longDifferentSeconds.size()) << " of the 1 s segments differ\n";
        }
    }

    text << "\n## Settings used\n```\n" << settings.toJson() << "\n```\n";
    return text;
}
}
