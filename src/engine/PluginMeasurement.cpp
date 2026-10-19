#include "pluginlab/engine/PluginMeasurement.h"

#include <cmath>
#include <limits>

#include "pluginlab/measure/Crosstalk.h"
#include "pluginlab/measure/Delay.h"
#include "pluginlab/measure/Distortion.h"
#include "pluginlab/measure/FrequencyResponse.h"
#include "pluginlab/measure/Gain.h"
#include "pluginlab/measure/Intermodulation.h"
#include "pluginlab/measure/Linearity.h"
#include "pluginlab/measure/Noise.h"
#include "pluginlab/measure/Phase.h"
#include "pluginlab/measure/SystemClass.h"

namespace pluginlab::engine
{
namespace
{
constexpr double kNotApplicable = std::numeric_limits<double>::quiet_NaN();

class Rows
{
public:
    explicit Rows(std::vector<MeasurementRow>& rows)
        : m_rows(rows)
    {
    }

    void add(const juce::String& unit, const juce::String& quantity, double value, const juce::String& text, const juce::String& clause)
    {
        m_rows.push_back({unit, quantity, value, text, clause});
    }

    void addDb(const juce::String& unit, const juce::String& quantity, double value, const juce::String& suffix, const juce::String& clause, int decimals = 2)
    {
        add(unit, quantity, value, juce::String(value, decimals) + " " + suffix, clause);
    }

private:
    std::vector<MeasurementRow>& m_rows;
};

// "L: x, R: y" or "x" for one channel
template <typename Function>
juce::String perChannel(int channels, Function function)
{
    if (channels == 1)
    {
        return function(0);
    }
    return "L: " + function(0) + ", R: " + function(1);
}

// A level or ratio with its unit; "silent" (no unit) for the floor of exact zeros
juce::String formatDb(double value, const juce::String& unit, int decimals = 2)
{
    if (value < -290.0)
    {
        return "silent";
    }
    return juce::String(value, decimals) + " " + unit;
}
}

const juce::StringArray& getMeasurementUnitKeys()
{
    static const juce::StringArray keys{"gain", "response", "delay", "phase", "distortion", "imd", "noise", "crosstalk", "maximum", "linearity"};
    return keys;
}

juce::String getMeasurementUnitTitle(const juce::String& key)
{
    static const juce::StringArray titles{"level and gain",       "frequency response", "delay and polarity", "phase",
                                          "THD and THD+N",        "intermodulation",    "noise",              "crosstalk",
                                          "maximum input level",  "gain linearity (slow)"};
    const int index = getMeasurementUnitKeys().indexOf(key);
    return index >= 0 ? titles[index] : key;
}

MeasurementSummary measureDevice(const measure::Device& device, int channels, const juce::String& deviceName, const MeasurementOptions& options)
{
    const auto wants = [&options](const juce::String& key) { return options.units.isEmpty() || options.units.contains(key); };
    int step = 0;
    const int steps = 1 + (options.units.isEmpty() ? getMeasurementUnitKeys().size() : options.units.size());
    const auto announce = [&options, &step, steps](const juce::String& unit) {
        if (options.progress)
        {
            options.progress(static_cast<double>(step) / steps, unit);
        }
        ++step;
    };
    const double startMs = juce::Time::getMillisecondCounterHiRes();
    MeasurementSummary summary;
    summary.deviceName = deviceName;
    summary.sampleRate = options.sampleRate;
    summary.channels = std::min(std::max(channels, 0), 2);
    if (summary.channels == 0)
    {
        return summary;
    }
    const int used = summary.channels;
    Rows rows(summary.rows);
    const double fs = options.sampleRate;

    // the system class first (W8.1): which of the following measurements mean what
    announce("system class");
    measure::SystemClassSettings classification;
    classification.sampleRate = fs;
    const measure::SystemClassResult systemClass = measure::classifySystem(device, classification);
    rows.add("system class", "system class (channel 1)", static_cast<double>(static_cast<int>(systemClass.systemClass)),
             measure::getSystemClassName(systemClass.systemClass) + ": " + measure::getSystemClassMeaning(systemClass.systemClass) + " (" + systemClass.reason
                 + ")",
             "W8.1");

    if (wants("gain"))
    {
        announce("level and gain");
        // level and gain (W7.1)
        measure::GainSettings gain;
        gain.sampleRate = fs;
        gain.channels = used;
        const measure::GainResult gainResult = measure::measureGain(device, gain);
        rows.add("gain", "gain at 997 Hz, -20 dBFS (broadband)", gainResult.channels[0].gainDb,
                 perChannel(used, [&](int c) { return juce::String(gainResult.channels[static_cast<size_t>(c)].gainDb, 3); }) + " dB", "AES17 6.2.2");
        if (used == 2)
        {
            rows.addDb("gain", "gain matching between the channels", gainResult.matchingDb, "dB", "AES17 6.2.4", 3);
        }
    }

    if (wants("response"))
    {
        announce("frequency response");
        // frequency response (W7.2), by the synchronized sweep, at the standard third-octave frequencies
        measure::SweepResponseSettings sweep;
        sweep.sampleRate = fs;
        sweep.channels = used;
        sweep.frequencies = measure::getStandardThirdOctaveFrequencies();
        const measure::FrequencyResponse response = measure::measureSweptResponse(device, sweep);
        double lowest = 1000.0;
        double highest = -1000.0;
        for (const double value : response.channels[0].relativeDb)
        {
            lowest = std::min(lowest, value);
            highest = std::max(highest, value);
        }
        rows.add("frequency response", "response 20 Hz ... 20 kHz re 997 Hz (third octaves, channel 1)", highest - lowest,
                 juce::String(lowest, 2) + " ... +" + juce::String(highest, 2) + " dB", "AES17 6.2.3, A.4");
    }

    if (wants("delay"))
    {
        announce("delay and polarity");
        // delay and polarity (W7.3)
        measure::DelaySettings delay;
        delay.sampleRate = fs;
        delay.channels = used;
        const measure::DelayResult delayResult = measure::measureDelay(device, delay);
        rows.add("delay", "delay: impulse peak", delayResult.channels[0].impulsePeakSamples,
                 perChannel(used, [&](int c) { return juce::String(delayResult.channels[static_cast<size_t>(c)].impulsePeakSamples); }) + " samples", "AES17 6.8.2 a");
        rows.add("delay", "phase delay at 100 Hz", delayResult.channels[0].phaseDelaySamples,
                 perChannel(used, [&](int c) { return juce::String(delayResult.channels[static_cast<size_t>(c)].phaseDelaySamples, 3); }) + " samples", "W7.3");
        double inverted = 0.0;
        if (delayResult.channels[0].invertingByImpulse)
        {
            inverted = 1.0;
        }
        rows.add("polarity", "polarity (impulse response)", inverted,
                 perChannel(used, [&](int c)
                            {
                                if (delayResult.channels[static_cast<size_t>(c)].invertingByImpulse)
                                {
                                    return juce::String("inverting");
                                }
                                return juce::String("non-inverting");
                            }),
                 "AES17 6.2.8 c");
    }

    if (wants("phase"))
    {
        announce("phase");
        // phase (W7.4)
        measure::PhaseSettings phase;
        phase.sampleRate = fs;
        phase.channels = used;
        const measure::PhaseResponse phaseResult = measure::measurePhaseResponse(device, phase);
        const measure::ChannelPhase& firstPhase = phaseResult.channels[0];
        rows.add("phase", "deviation from linear phase (passband, channel 1)", firstPhase.deviationMaxDegrees - firstPhase.deviationMinDegrees,
                 "+" + juce::String(firstPhase.deviationMaxDegrees, 1) + "/" + juce::String(firstPhase.deviationMinDegrees, 1) + " degrees", "AES17 6.8.3");
    }

    if (wants("distortion"))
    {
        announce("THD and THD+N");
        // THD and THD+N (W7.5)
        for (const double level : {-1.0, -20.0})
        {
            measure::DistortionSettings distortion;
            distortion.sampleRate = fs;
            distortion.levelDbfs = level;
            distortion.channels = used;
            const measure::DistortionResult result = measure::measureDistortion(device, distortion);
            const juce::String at = " at " + juce::String(level, 0) + " dBFS, 997 Hz";
            rows.add("THD+N", "THD+N" + at, result.channels[0].thdnDb,
                     perChannel(used, [&](int c) { return formatDb(result.channels[static_cast<size_t>(c)].thdnDb, "dB"); }), "AES17 6.3.1, A.3.6");
            rows.add("THD", "THD" + at, result.channels[0].thdDb,
                     perChannel(used, [&](int c) { return formatDb(result.channels[static_cast<size_t>(c)].thdDb, "dB"); }), "IEC 60268-3");
        }
    }

    if (wants("imd"))
    {
        announce("intermodulation");
        // intermodulation (W7.6)
        measure::DifferenceFrequencySettings dfd;
        dfd.sampleRate = fs;
        dfd.channels = used;
        const measure::DifferenceFrequencyResult dfdResult = measure::measureDifferenceFrequency(device, dfd);
        rows.add("IMD", "difference-frequency distortion 18 + 20 kHz, 0 dBFS", dfdResult.channels[0].ratioDb,
                 perChannel(used, [&](int c) { return formatDb(dfdResult.channels[static_cast<size_t>(c)].ratioDb, "dB"); }), "AES17 6.3.5");
        measure::ModulationSettings md;
        md.sampleRate = fs;
        md.channels = used;
        const measure::ModulationResult mdResult = measure::measureModulation(device, md);
        rows.add("IMD", "modulation distortion 41 + 7993 Hz 4:1, 0 dBFS", mdResult.channels[0].ratioDb,
                 perChannel(used, [&](int c) { return formatDb(mdResult.channels[static_cast<size_t>(c)].ratioDb, "dB"); }) + " (all sidebands: "
                     + perChannel(used, [&](int c) { return formatDb(mdResult.channels[static_cast<size_t>(c)].allSidebandsDb, "dB"); }) + ")",
                 "AES17 6.3.6");
    }

    if (wants("noise"))
    {
        announce("noise");
        // noise (W7.7)
        measure::NoiseSettings noise;
        noise.sampleRate = fs;
        noise.channels = used;
        const measure::IdleNoiseResult idle = measure::measureIdleNoise(device, noise);
        rows.add("noise", "idle channel noise", idle.channels[0].ccirRmsDbfs,
                 perChannel(used, [&](int c) { return formatDb(idle.channels[static_cast<size_t>(c)].ccirRmsDbfs, "dBFS CCIR-RMS"); }) + " ("
                     + perChannel(used, [&](int c) { return formatDb(idle.channels[static_cast<size_t>(c)].aWeightedDbfs, "dBFS(A)"); }) + ")",
                 "AES17 6.4.2");
        const measure::DynamicRangeResult range = measure::measureDynamicRange(device, noise);
        rows.add("noise", "dynamic range (997 Hz at -60 dBFS)", range.channels[0].dynamicRange.ccirRmsDbfs,
                 perChannel(used, [&](int c) { return formatDb(range.channels[static_cast<size_t>(c)].dynamicRange.ccirRmsDbfs, "dB CCIR-RMS"); }) + " ("
                     + perChannel(used, [&](int c) { return formatDb(range.channels[static_cast<size_t>(c)].dynamicRange.aWeightedDbfs, "dB(A)"); }) + ")",
                 "AES17 6.4.1");
        measure::MainsSettings mains;
        mains.sampleRate = fs;
        mains.channels = used;
        const measure::MainsResult hum = measure::measureMainsProducts(device, mains);
        rows.add("noise", "mains products 50 Hz (M = 1 ... 5)", hum.channels[0].totalDbfs,
                 perChannel(used, [&](int c) { return formatDb(hum.channels[static_cast<size_t>(c)].totalDbfs, "dBFS"); }), "AES17 6.5.1");
    }

    if (wants("crosstalk"))
    {
        announce("crosstalk");
        // crosstalk (W7.8)
        if (used == 2)
        {
            measure::CrosstalkSettings crosstalk;
            crosstalk.sampleRate = fs;
            const measure::CrosstalkResult crosstalkResult = measure::measureCrosstalk(device, crosstalk);
            rows.add("crosstalk", "worst crosstalk 20 Hz ... 20 kHz (selective)", crosstalkResult.worstSelectiveDb, formatDb(crosstalkResult.worstSelectiveDb, "dB"),
                     "AES17 6.5.2");
        }
    }

    measure::MaximumLevelResult level;
    if (wants("maximum") || wants("linearity"))
    {
        announce("maximum input level");
        // maximum level and linearity (W7.9)
        measure::MaximumLevelSettings maximum;
        maximum.sampleRate = fs;
        level = measure::measureMaximumLevel(device, maximum);
        juce::String levelText = "none up to +24 dBFS (THD+N stays below -40 dB)";
        double levelValue = kNotApplicable;
        if (level.found)
        {
            levelValue = level.maximumInputDbfs;
            levelText = juce::String(level.maximumInputDbfs, 2) + " dBFS (output " + juce::String(level.maximumOutputDbfs, 2) + " dBFS); +3 dB above: THD+N "
                        + juce::String(level.overloadThdnDb, 1) + " dB";
            if (level.rollover)
            {
                levelText << ", ROLLOVER";
            }
        }
        else if (level.exceededEverywhere)
        {
            levelText = "THD+N above -40 dB at every level (noise): the method does not apply";
        }
        rows.add("maximum level", "maximum input level (THD+N -40 dB, channel 1)", levelValue, levelText, "AES17 6.2.1 a, 6.2.6, 6.6.8");
    }
    if (options.linearity && wants("linearity"))
    {
        announce("gain linearity");
        measure::LinearitySettings linearity;
        linearity.sampleRate = fs;
        if (level.found)
        {
            linearity.maximumInputDbfs = std::min(level.maximumInputDbfs, 0.0);
        }
        const measure::LinearityResult curve = measure::measureGainLinearity(device, linearity);
        double worst = 0.0;
        double worstAt = 0.0;
        for (size_t index = 0; index < curve.deviationDb.size(); ++index)
        {
            if (std::abs(curve.deviationDb[index]) > std::abs(worst) && curve.outputDbfs[index] > -290.0)
            {
                worst = curve.deviationDb[index];
                worstAt = curve.inputDbfs[index];
            }
        }
        rows.add("linearity", "gain non-linearity: largest deviation (channel 1)", worst,
                 juce::String(worst, 3) + " dB at " + juce::String(worstAt, 0) + " dBFS (down to " + juce::String(curve.inputDbfs.back(), 0) + " dBFS)", "AES17 6.3.7");
    }
    summary.seconds = (juce::Time::getMillisecondCounterHiRes() - startMs) / 1000.0;
    return summary;
}

MeasurementSummary measurePlugin(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& setting,
                                 const MeasurementOptions& options)
{
    const int channels = getPluginChannels(formatManager, description, options.sampleRate);
    return measureDevice(makePluginDevice(formatManager, description, setting), channels, description.name, options);
}

juce::String createMeasurementReport(const MeasurementSummary& summary)
{
    juce::String text;
    text << "## Measurements (AES17)\n\n";
    if (summary.channels == 0)
    {
        text << "The plugin could not be measured (no instance, or no usable channel layout).\n";
        return text;
    }
    text << "The measurement units of W7 with their AES17 defaults (docs/measurements/), at " << juce::String(summary.sampleRate, 0) << " Hz, "
         << summary.channels << " channel(s), the plugin's settings as delivered (fresh instance per render). Levels in dBFS re a full-scale sine "
         << "(AES17 3.12); 0 dBFS is taken as the maximum input level. Time: " << juce::String(summary.seconds, 1) << " s.\n\n";
    text << "| Unit | Quantity | Result | Clause |\n|---|---|---|---|\n";
    for (const MeasurementRow& row : summary.rows)
    {
        text << "| " << row.unit << " | " << row.quantity << " | " << row.text << " | " << row.clause << " |\n";
    }
    return text;
}

juce::var createMeasurementJson(const MeasurementSummary& summary)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("device", summary.deviceName);
    root->setProperty("sampleRate", summary.sampleRate);
    root->setProperty("channels", summary.channels);
    root->setProperty("seconds", summary.seconds);
    juce::Array<juce::var> rows;
    for (const MeasurementRow& row : summary.rows)
    {
        auto entry = std::make_unique<juce::DynamicObject>();
        entry->setProperty("unit", row.unit);
        entry->setProperty("quantity", row.quantity);
        entry->setProperty("value", std::isfinite(row.value) ? juce::var(row.value) : juce::var()); // NaN is no JSON
        entry->setProperty("text", row.text);
        entry->setProperty("clause", row.clause);
        rows.add(juce::var(entry.release()));
    }
    root->setProperty("rows", rows);
    return juce::var(root.release());
}

MeasurementSummary parseMeasurementJson(const juce::var& json)
{
    MeasurementSummary summary;
    summary.deviceName = json.getProperty("device", "").toString();
    summary.sampleRate = json.getProperty("sampleRate", 48000.0);
    summary.channels = json.getProperty("channels", 0);
    summary.seconds = json.getProperty("seconds", 0.0);
    if (const juce::Array<juce::var>* rows = json.getProperty("rows", {}).getArray())
    {
        for (const juce::var& entry : *rows)
        {
            MeasurementRow row;
            row.unit = entry.getProperty("unit", "").toString();
            row.quantity = entry.getProperty("quantity", "").toString();
            const juce::var value = entry.getProperty("value", {});
            row.value = value.isVoid() ? kNotApplicable : static_cast<double>(value);
            row.text = entry.getProperty("text", "").toString();
            row.clause = entry.getProperty("clause", "").toString();
            summary.rows.push_back(row);
        }
    }
    return summary;
}

const MeasurementRow* findRow(const MeasurementSummary& summary, const juce::String& quantity)
{
    for (const MeasurementRow& row : summary.rows)
    {
        if (row.quantity == quantity)
        {
            return &row;
        }
    }
    return nullptr;
}
}
