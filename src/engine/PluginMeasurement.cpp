#include "pluginlab/engine/PluginMeasurement.h"

#include <cmath>
#include <limits>

#include "pluginlab/measure/Crosstalk.h"
#include "pluginlab/measure/Delay.h"
#include "pluginlab/measure/Distortion.h"
#include "pluginlab/measure/Dynamics.h"
#include "pluginlab/measure/EchoResponse.h"
#include "pluginlab/measure/Reverb.h"
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
constexpr int kLatencyBlock = 512; // the block size of the instance that reports its latency

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

juce::String getStereoKindName(measure::StereoEchoKind kind)
{
    if (kind == measure::StereoEchoKind::PingPong)
    {
        return "ping-pong";
    }
    if (kind == measure::StereoEchoKind::CrossFeedback)
    {
        return "cross feedback";
    }
    return "independent channels";
}

juce::String getShapeName(measure::LfoShapeFound shape)
{
    if (shape == measure::LfoShapeFound::Sine)
    {
        return "sine";
    }
    if (shape == measure::LfoShapeFound::Triangle)
    {
        return "triangle";
    }
    if (shape == measure::LfoShapeFound::Other)
    {
        return "neither sine nor triangle";
    }
    return "none";
}

// the echo response (W11.1f): rows only for a device that answers with echoes
void addEchoRows(Rows& rows, const measure::Device& device, int channels, const MeasurementOptions& options)
{
    const juce::String kClause = "W11.1";
    const double fs = options.sampleRate;
    const auto ms = [fs](double samples) { return juce::String(samples * 1000.0 / fs, 2) + " ms (" + juce::String(samples, 1) + " samples)"; };
    measure::EchoMeasurementSettings settings;
    settings.sampleRate = fs;
    settings.channels = channels;
    settings.latencySamples = options.latencySamples;
    const measure::EchoMeasurement echoes = measure::measureEchoResponse(device, settings);
    if (! echoes.hasEchoes)
    {
        rows.add("echoes", "echoes after an impulse (channel 1)", 0.0, "none (no delay)", kClause);
        return;
    }
    const measure::EchoAnalysis& analysis = echoes.echoes;
    rows.add("echoes", "echoes after an impulse (channel 1)", static_cast<double>(analysis.echoes.size()),
             juce::String(static_cast<int>(analysis.echoes.size())) + " within 60 dB of the largest arrival", kClause);
    rows.add("echoes", "delay time (first echo after the dry path)", analysis.delaySamples * 1000.0 / fs, ms(analysis.delaySamples), kClause);
    if (analysis.echoes.size() >= 2)
    {
        rows.add("echoes", "echo spacing (spread)", analysis.spacingSamples * 1000.0 / fs,
                 ms(analysis.spacingSamples) + ", spread " + juce::String(analysis.spacingSpreadSamples, 2) + " samples", kClause);
        rows.add("echoes", "feedback per repeat", analysis.feedbackDb,
                 juce::String(analysis.feedbackDb, 2) + " dB (" + juce::String(analysis.decayDbPerSecond, 1) + " dB/s)", kClause);
    }
    if (analysis.hasDirect)
    {
        rows.addDb("echoes", "first echo against the dry path (mix)", analysis.mixDb, "dB", kClause);
    }
    if (echoes.stereo)
    {
        juce::String text = getStereoKindName(echoes.stereoEchoes.kind);
        if (echoes.stereoEchoes.kind == measure::StereoEchoKind::PingPong)
        {
            text << ", one bounce " << ms(echoes.stereoEchoes.pingPongSpacingSamples);
        }
        rows.add("echoes", "stereo (one input at a time)", static_cast<double>(static_cast<int>(echoes.stereoEchoes.kind)), text, kClause);
    }
    const measure::LevelDependence& levels = echoes.levels;
    juce::String levelText = "first echo " + juce::String(levels.compressionDb, 2) + " dB, feedback " + juce::String(levels.feedbackChangeDb, 2)
                             + " dB per repeat from -40 to -6 dBFS: ";
    if (levels.saturates)
    {
        levelText << "saturation in the loop";
    }
    else
    {
        levelText << "linear";
    }
    if (levels.selfOscillating)
    {
        levelText << "; SELF-OSCILLATING (the echoes do not decay)";
    }
    rows.add("echoes", "level dependence of the echoes", levels.compressionDb, levelText, kClause);
    const measure::DelayTrajectory& trajectory = echoes.trajectory;
    juce::String modulation = "none";
    double modulationValue = 0.0;
    if (trajectory.shape != measure::LfoShapeFound::None)
    {
        modulationValue = trajectory.depthSamples * 1000.0 / fs;
        modulation = getShapeName(trajectory.shape) + ", " + juce::String(trajectory.rateHz, 2) + " Hz, depth " + ms(trajectory.depthSamples) + ", "
                     + juce::String(trajectory.pitchDeviationCents, 1) + " cents";
    }
    rows.add("echoes", "delay modulation (impulse train)", modulationValue, modulation, kClause);
}

// the latency the plugin reports at a setting (where the dry path of its echo response arrives; a look-ahead may depend on the setting): prepared,
// the setting delivered, one block of silence processed; 0 if it cannot be created
double getReportedLatency(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& setting,
                          double sampleRate)
{
    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> instance = formatManager.createPluginInstance(description, sampleRate, kLatencyBlock, error);
    if (instance == nullptr)
    {
        return 0.0;
    }
    instance->prepareToPlay(sampleRate, kLatencyBlock);
    const juce::Array<juce::AudioProcessorParameter*>& parameters = instance->getParameters();
    for (int index = 0; index < std::min(parameters.size(), static_cast<int>(setting.size())); ++index)
    {
        parameters[index]->setValueNotifyingHost(setting[static_cast<size_t>(index)]);
    }
    juce::AudioBuffer<float> silence(std::max(1, std::max(instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels())), kLatencyBlock);
    silence.clear();
    juce::MidiBuffer midi;
    instance->processBlock(silence, midi);
    const double latency = instance->getLatencySamples();
    instance->releaseResources();
    return latency;
}

// the reverb (W11.2e): rows only for a device with a diffuse decaying tail
void addReverbRows(Rows& rows, const measure::Device& device, int channels, const MeasurementOptions& options)
{
    const juce::String kClause = "W11.2, ISO 3382-1";
    const double fs = options.sampleRate;
    measure::ReverbMeasurementSettings settings;
    settings.sampleRate = fs;
    settings.channels = channels;
    settings.latencySamples = std::max(0.0, options.latencySamples);
    const measure::ReverbMeasurement reverb = measure::measureReverbResponse(device, settings);
    if (! reverb.hasReverb)
    {
        rows.add("reverb", "reverb (a diffuse decaying tail, channel 1)", 0.0, "none (no reverb)", kClause);
        return;
    }
    const measure::ReverbAnalysis& decay = reverb.decay;
    rows.add("reverb", "reverb (a diffuse decaying tail, channel 1)", 1.0, "yes", kClause);
    juce::String fromWhere = "the whole response";
    if (decay.hasDirect)
    {
        fromWhere = "the reverberant part: the direct path cut out, from the onset";
    }
    rows.add("reverb", "T30 broadband", decay.broadband.t30,
             juce::String(decay.broadband.t30, 3) + " s (EDT " + juce::String(decay.broadband.edt, 3) + " s, T20 " + juce::String(decay.broadband.t20, 3) + " s; from "
                 + fromWhere + ")",
             kClause);
    juce::StringArray octaves;
    for (const measure::BandDecay& band : decay.octaves)
    {
        octaves.add(juce::String(juce::roundToInt(band.band.centreHz)) + " Hz: " + juce::String(band.times.t30, 2));
    }
    rows.add("reverb", "T30 per octave (s)", kNotApplicable, octaves.joinIntoString(", "), kClause);
    rows.addDb("reverb", "C80", decay.c80Db, "dB", kClause);
    rows.add("reverb", "pre-delay (to the reverb's onset)", decay.preDelaySeconds * 1000.0, juce::String(decay.preDelaySeconds * 1000.0, 2) + " ms", kClause);
    if (decay.hasDirect)
    {
        rows.addDb("reverb", "direct-to-reverberant ratio (DRR)", decay.directToReverberantDb, "dB", kClause);
    }
    else
    {
        rows.add("reverb", "direct-to-reverberant ratio (DRR)", kNotApplicable, "no direct path (100 % wet)", kClause);
    }
    const measure::Texture& texture = reverb.texture;
    rows.add("reverb", "mixing time (echo density 0.95)", texture.mixingTimeSeconds * 1000.0, juce::String(texture.mixingTimeSeconds * 1000.0, 1) + " ms after the onset", kClause);
    rows.add("reverb", "late kurtosis (0: Gaussian like a good tail)", texture.lateKurtosis, juce::String(texture.lateKurtosis, 2), kClause);
    rows.add("reverb", "repetition in the tail (normalized autocorrelation)", texture.repetitionStrength,
             juce::String(texture.repetitionStrength, 3) + " at " + juce::String(texture.repetitionLagSeconds * 1000.0, 2) + " ms", kClause);
    rows.add("reverb", "coloration (late spectrum around its third-octave smoothing)", texture.colorationDb,
             juce::String(texture.colorationDb, 2) + " dB (noise: 5.6 dB)", kClause);
    if (reverb.stereo)
    {
        rows.add("reverb", "IACC early (0 ... 80 ms, with the direct sound) and late", reverb.iaccLate.value,
                 juce::String(reverb.iaccEarly.value, 3) + ", late " + juce::String(reverb.iaccLate.value, 3) + " (lag " + juce::String(reverb.iaccLate.lagSeconds * 1000.0, 2)
                     + " ms)",
                 kClause);
        rows.add("reverb", "coherence of the tail (200 Hz ... 8 kHz)", reverb.meanCoherence, juce::String(reverb.meanCoherence, 3), kClause);
        rows.add("reverb", "LEDT (-10 dB point x 6; 350 ms)", reverb.lateral.tenDbSeconds,
                 juce::String(reverb.lateral.tenDbSeconds, 3) + " s; " + juce::String(reverb.lateral.at350msSeconds, 3) + " s", kClause);
    }
}

// the dynamics (W11.3f): rows only for a level-dependent device; the time behaviour and the rest only for a compressor
void addDynamicsRows(Rows& rows, const measure::Device& device, int channels, const MeasurementOptions& options, MeasurementSummary& summary)
{
    const juce::String kClause = "W11.3";
    measure::DynamicsMeasurementSettings settings;
    settings.sampleRate = options.sampleRate;
    settings.channels = channels;
    settings.latencySamples = std::max(0.0, options.latencySamples);
    const std::shared_ptr<const measure::DynamicsMeasurement> result = std::make_shared<const measure::DynamicsMeasurement>(measure::measureDynamicsResponse(device, settings));
    summary.dynamics = result;
    const measure::DynamicsMeasurement& dynamics = *result;
    if (! dynamics.levelDependent)
    {
        rows.add("dynamics", "dynamics (a level-dependent gain, channel 1)", 0.0,
                 "none (the steady gain varies by " + juce::String(dynamics.gainRangeDb, 2) + " dB from -60 to 0 dBFS)", kClause);
        return;
    }
    rows.add("dynamics", "dynamics (a level-dependent gain, channel 1)", 1.0, "yes (" + juce::String(dynamics.gainRangeDb, 1) + " dB of gain change)", kClause);
    const measure::StaticFit& fit = dynamics.fit;
    rows.add("dynamics", "static curve: threshold, ratio, knee, makeup", fit.thresholdDb,
             juce::String(fit.thresholdDb, 1) + " dBFS, " + juce::String(fit.ratio, 2) + " : 1, knee " + juce::String(fit.kneeDb, 1) + " dB, makeup "
                 + juce::String(std::round(fit.makeupDb * 10.0) / 10.0 + 0.0, 1) + " dB (the model's error " + juce::String(fit.rmsErrorDb, 2) + " dB)",
             "W11.3b, Giannoulis et al. 2012");
    rows.add("dynamics", "hysteresis (rising against falling staircase)", dynamics.curve.hysteresisDb,
             juce::String(dynamics.curve.hysteresisDb, 2) + " dB; " + juce::String(dynamics.curve.unsettledSteps) + " steps of 0.5 s not settled", kClause);
    if (! dynamics.compressor)
    {
        rows.add("dynamics", "dynamics: kind", kNotApplicable, "level dependent, but not a compressor (an expander or gate: not measured yet)", kClause);
        return;
    }
    const measure::TimeFit& time = dynamics.step.best;
    juce::String timing = juce::String(measure::getTimeModelName(time.model)) + ": attack " + juce::String(time.parameters.attackSeconds * 1000.0, 2) + " ms, release "
                          + juce::String(time.parameters.releaseSeconds * 1000.0, 1) + " ms";
    if (time.model == measure::TimeModel::TwoStage)
    {
        timing << ", detector " << juce::String(time.parameters.detectorAttackSeconds * 1000.0, 2) << " / "
               << juce::String(time.parameters.detectorReleaseSeconds * 1000.0, 1) << " ms";
    }
    timing << ", hold " << juce::String(time.parameters.holdSeconds * 1000.0, 1) << " ms (the model's error " << juce::String(time.rmsErrorDb, 2) << " dB)";
    if (measure::isAtypicalTimeModel(time.model))
    {
        timing << "; atypical: the linear level is smoothed, so attack and release in dB depend on how far the level steps";
    }
    rows.add("dynamics", "time model (the best of four variants)", time.parameters.attackSeconds * 1000.0, timing, "W11.3c, Simmer, Schmidt, Bitzer 2006");
    rows.add("dynamics", "attack and release (within 3 / 4 dB of the final value)", dynamics.step.attackSettleSeconds * 1000.0,
             juce::String(dynamics.step.attackSettleSeconds * 1000.0, 1) + " ms, " + juce::String(dynamics.step.releaseSettleSeconds * 1000.0, 1) + " ms",
             "ANSI S3.22");
    rows.add("dynamics", "look-ahead (the gain moves before the audio)", dynamics.step.leadSamples,
             juce::String(dynamics.step.leadSamples) + " samples (" + juce::String(dynamics.step.leadSamples * 1000.0 / options.sampleRate, 2) + " ms); the audio delayed by "
                 + juce::String(dynamics.audioDelaySamples) + " samples (the cross-correlation)",
             kClause);
    rows.add("dynamics", "detector", static_cast<double>(dynamics.detector.kind),
             juce::String(measure::getDetectorKindName(dynamics.detector.kind)) + " (zeros test " + juce::String(dynamics.detector.gainChangeDb, 2) + " dB; RMS "
                 + juce::String(dynamics.detector.rmsChangeDb, 2) + ", peak model " + juce::String(dynamics.detector.peakModelChangeDb, 2) + ")",
             "W11.3d");
    if (! dynamics.transfer.frequencies.empty())
    {
        juce::String bands = measure::getBandKindName(dynamics.transfer.kind);
        if (dynamics.transfer.kind == measure::BandKind::Multiband || dynamics.transfer.kind == measure::BandKind::DynamicEq)
        {
            bands << " (within 3 dB of the largest reduction over " << juce::String(dynamics.transfer.plateauOctaves, 1) << " octaves)";
        }
        rows.add("dynamics", "bands (the two-tone test, " + juce::String(juce::roundToInt(dynamics.toneHz)) + " Hz loud)", static_cast<double>(dynamics.transfer.kind), bands,
                 "W11.3d");
    }
    if (dynamics.stereo)
    {
        rows.add("dynamics", "stereo link (the quiet channel's reduction re the loud one's)", dynamics.stereoLink, juce::String(juce::roundToInt(dynamics.stereoLink * 100.0)) + " %",
                 "W11.3d");
    }
    juce::StringArray thd;
    for (size_t index = 0; index < dynamics.distortion.frequencies.size(); ++index)
    {
        thd.add(juce::String(juce::roundToInt(dynamics.distortion.frequencies[index])) + " Hz: " + juce::String(dynamics.distortion.thdDb[index], 1) + " dB");
    }
    double lowThd = kNotApplicable;
    if (! dynamics.distortion.thdDb.empty())
    {
        lowThd = dynamics.distortion.thdDb.front();
    }
    rows.add("dynamics", "THD of the dynamics (" + juce::String(juce::roundToInt(dynamics.distortionLevelDbfs)) + " dBFS)", lowThd, thd.joinIntoString(", "), "W11.3e, IEC 60268-3");
    rows.add("dynamics", "limiter test (+6 dBFS at fs/4): sample peak, true peak, overshoot", dynamics.limiter.truePeakDbfs,
             juce::String(dynamics.limiter.samplePeakDbfs, 2) + " dBFS, true peak " + juce::String(dynamics.limiter.truePeakDbfs, 2) + " dBTP, overshoot "
                 + juce::String(dynamics.limiter.overshootDb, 2) + " dB",
             "W11.3e, ITU-R BS.1770-5");
}

// the number of units a run measures: the chosen ones, or all if none is chosen
int getUnitCount(const MeasurementOptions& options)
{
    if (options.units.isEmpty())
    {
        return getMeasurementUnitKeys().size();
    }
    return options.units.size();
}
}

const juce::StringArray& getMeasurementUnitKeys()
{
    static const juce::StringArray keys{"gain", "response", "delay", "phase", "distortion", "imd", "noise", "crosstalk", "maximum", "linearity", "echoes", "reverb", "dynamics"};
    return keys;
}

juce::String getMeasurementUnitTitle(const juce::String& key)
{
    static const juce::StringArray titles{"level and gain",       "frequency response", "delay and polarity", "phase",
                                          "THD and THD+N",        "intermodulation",    "noise",              "crosstalk",
                                          "maximum input level",  "gain linearity (slow)", "echo response (delays)",
                                          "reverb (decay, texture, stereo)", "dynamics (static curve, time, detector)"};
    const int index = getMeasurementUnitKeys().indexOf(key);
    if (index >= 0)
    {
        return titles[index];
    }
    return key;
}

MeasurementSummary measureDevice(const measure::Device& device, int channels, const juce::String& deviceName, const MeasurementOptions& options)
{
    const auto wants = [&options](const juce::String& key) { return options.units.isEmpty() || options.units.contains(key); };
    int step = 0;
    const int steps = 1 + getUnitCount(options);
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
    if (wants("echoes"))
    {
        announce("echo response");
        addEchoRows(rows, device, used, options);
    }
    if (wants("reverb"))
    {
        announce("reverb");
        addReverbRows(rows, device, used, options);
    }
    if (wants("dynamics"))
    {
        announce("dynamics");
        addDynamicsRows(rows, device, used, options, summary);
    }
    summary.seconds = (juce::Time::getMillisecondCounterHiRes() - startMs) / 1000.0;
    return summary;
}

MeasurementSummary measurePlugin(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& setting,
                                 const MeasurementOptions& options)
{
    const int channels = getPluginChannels(formatManager, description, options.sampleRate);
    MeasurementOptions withLatency = options;
    if (withLatency.latencySamples < 0.0)
    {
        withLatency.latencySamples = getReportedLatency(formatManager, description, setting, options.sampleRate); // the dry path of the echo response
    }
    return measureDevice(makePluginDevice(formatManager, description, setting), channels, description.name, withLatency);
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
        juce::var value; // void unless finite: NaN is no JSON
        if (std::isfinite(row.value))
        {
            value = juce::var(row.value);
        }
        entry->setProperty("value", value);
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
            if (value.isVoid())
            {
                row.value = kNotApplicable;
            }
            else
            {
                row.value = static_cast<double>(value);
            }
            row.text = entry.getProperty("text", "").toString();
            row.clause = entry.getProperty("clause", "").toString();
            summary.rows.push_back(row);
        }
    }
    return summary;
}

namespace
{
measure::EchoMeasurement measurePluginEchoes(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                                             const std::vector<float>& setting, double sampleRate)
{
    measure::EchoMeasurementSettings settings;
    settings.sampleRate = sampleRate;
    settings.channels = std::max(1, std::min(2, getPluginChannels(formatManager, description, sampleRate)));
    settings.latencySamples = getReportedLatency(formatManager, description, setting, sampleRate);
    return measure::measureEchoResponse(makePluginDevice(formatManager, description, setting), settings);
}

// the loop filter of an analysis at a frequency (the nearest bin), or nothing where it was not measured
juce::String getLoopText(const measure::EchoAnalysis& analysis, double frequency, double& value)
{
    if (analysis.loopFrequencies.size() < 2)
    {
        return "-";
    }
    const double width = analysis.loopFrequencies[1] - analysis.loopFrequencies[0];
    const size_t bin = static_cast<size_t>(std::lround(frequency / width));
    if (bin >= analysis.loopValid.size() || ! analysis.loopValid[bin])
    {
        return "-";
    }
    value = analysis.loopMagnitudeDb[bin];
    return juce::String(value, 2) + " dB";
}

juce::String getModulationText(const measure::DelayTrajectory& trajectory, double sampleRate)
{
    if (trajectory.shape == measure::LfoShapeFound::None)
    {
        return "none";
    }
    return getShapeName(trajectory.shape) + " " + juce::String(trajectory.rateHz, 2) + " Hz, " + juce::String(trajectory.depthSamples * 1000.0 / sampleRate, 3) + " ms";
}
}

juce::String compareEchoResponses(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& descriptionA, const std::vector<float>& settingA,
                                  const juce::PluginDescription& descriptionB, const std::vector<float>& settingB, double sampleRate)
{
    const measure::EchoMeasurement a = measurePluginEchoes(formatManager, descriptionA, settingA, sampleRate);
    const measure::EchoMeasurement b = measurePluginEchoes(formatManager, descriptionB, settingB, sampleRate);
    const measure::EchoComparison comparison = measure::compareEchoes(a.echoes, b.echoes);
    const auto ms = [sampleRate](double samples) { return juce::String(samples * 1000.0 / sampleRate, 3) + " ms"; };
    juce::String text;
    text << "# Echo comparison: " << descriptionA.name << " (A) and " << descriptionB.name << " (B)\n\n";
    text << "The echo response of each at its setting (an impulse of -6 dBFS into both inputs, output channel 1; docs/design/W11-delay-plan.md 4.1 and 5), "
            "no matching. Differences are B - A.\n\n";
    if (! a.hasEchoes || ! b.hasEchoes)
    {
        text << "No comparison: " << descriptionA.name << " has " << juce::String(static_cast<int>(a.echoes.echoes.size())) << " echoes, " << descriptionB.name
             << " has " << juce::String(static_cast<int>(b.echoes.echoes.size())) << ".\n";
        return text;
    }
    text << "| quantity | A | B | B - A |\n|---|---|---|---|\n";
    text << "| echoes within 60 dB | " << comparison.echoesA << " | " << comparison.echoesB << " | " << (comparison.echoesB - comparison.echoesA) << " |\n";
    text << "| delay time | " << ms(a.echoes.delaySamples) << " | " << ms(b.echoes.delaySamples) << " | " << ms(comparison.delayDifferenceSamples) << " |\n";
    text << "| echo spacing | " << ms(a.echoes.spacingSamples) << " | " << ms(b.echoes.spacingSamples) << " | " << ms(comparison.spacingDifferenceSamples) << " |\n";
    text << "| feedback per repeat | " << juce::String(a.echoes.feedbackDb, 2) << " dB | " << juce::String(b.echoes.feedbackDb, 2) << " dB | "
         << juce::String(comparison.feedbackDifferenceDb, 2) << " dB |\n";
    text << "| first echo against the dry path | " << juce::String(a.echoes.mixDb, 2) << " dB | " << juce::String(b.echoes.mixDb, 2) << " dB | "
         << juce::String(comparison.mixDifferenceDb, 2) << " dB |\n";
    text << "| stereo | " << getStereoKindName(a.stereoEchoes.kind) << " | " << getStereoKindName(b.stereoEchoes.kind) << " | |\n";
    text << "| level dependence (first echo, -40 to -6 dBFS) | " << juce::String(a.levels.compressionDb, 2) << " dB | " << juce::String(b.levels.compressionDb, 2)
         << " dB | |\n";
    text << "| delay modulation | " << getModulationText(a.trajectory, sampleRate) << " | " << getModulationText(b.trajectory, sampleRate) << " | |\n\n";

    text << "## The echoes\n\n| echo | time A | time B | B - A | gain A | gain B | B - A | polarity |\n|---|---|---|---|---|---|---|---|\n";
    for (const measure::EchoPair& pair : comparison.pairs)
    {
        juce::String polarity = "same";
        if (pair.polarityDiffers)
        {
            polarity = "DIFFERS";
        }
        text << "| " << pair.echo << " | " << ms(pair.timeA) << " | " << ms(pair.timeB) << " | " << juce::String(pair.timeB - pair.timeA, 2) << " samples | "
             << juce::String(pair.gainDbA, 2) << " dB | " << juce::String(pair.gainDbB, 2) << " dB | " << juce::String(pair.gainDbB - pair.gainDbA, 2) << " dB | "
             << polarity << " |\n";
    }

    text << "\n## The loop filter (one trip with its feedback: the ratio of the spectra of successive echoes)\n\n";
    text << "Largest difference 50 Hz ... 15 kHz: " << juce::String(comparison.largestLoopDifferenceDb, 2) << " dB.\n\n";
    text << "| frequency | A | B | B - A |\n|---|---|---|---|\n";
    for (const double frequency : {63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0})
    {
        double valueA = 0.0;
        double valueB = 0.0;
        const juce::String textA = getLoopText(a.echoes, frequency, valueA);
        const juce::String textB = getLoopText(b.echoes, frequency, valueB);
        juce::String difference = "-";
        if (textA != "-" && textB != "-")
        {
            difference = juce::String(valueB - valueA, 2) + " dB";
        }
        text << "| " << juce::String(juce::roundToInt(frequency)) << " Hz | " << textA << " | " << textB << " | " << difference << " |\n";
    }
    return text;
}

namespace
{
void addBandTable(juce::String& text, const std::vector<measure::BandComparison>& bands)
{
    text << "| band | T30 A | T30 B | B - A | verdict |\n|---|---|---|---|---|\n";
    for (const measure::BandComparison& band : bands)
    {
        text << "| " << juce::String(band.band.centreHz, 1) << " Hz | " << juce::String(band.t30A, 3) << " s | " << juce::String(band.t30B, 3) << " s | "
             << juce::String(band.differencePercent, 1) << " % | " << measure::getJndVerdictName(band.verdict) << " |\n";
    }
}
}

measure::ReverbMeasurement measurePluginReverb(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                                               const std::vector<float>& setting, double sampleRate)
{
    measure::ReverbMeasurementSettings settings;
    settings.sampleRate = sampleRate;
    settings.channels = std::max(1, std::min(2, getPluginChannels(formatManager, description, sampleRate)));
    settings.latencySamples = getReportedLatency(formatManager, description, setting, sampleRate);
    return measure::measureReverbResponse(makePluginDevice(formatManager, description, setting), settings);
}

juce::String compareReverbResponses(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& descriptionA, const std::vector<float>& settingA,
                                    const juce::PluginDescription& descriptionB, const std::vector<float>& settingB, double sampleRate)
{
    const measure::ReverbMeasurement a = measurePluginReverb(formatManager, descriptionA, settingA, sampleRate);
    const measure::ReverbMeasurement b = measurePluginReverb(formatManager, descriptionB, settingB, sampleRate);
    juce::String text;
    text << "# Reverb comparison: " << descriptionA.name << " (A, the master) and " << descriptionB.name << " (B)\n\n";
    text << "Each at its setting: an impulse of -6 dBFS, 8 s, output channel 1 (docs/design/W11-reverb-plan.md 4 and 5). The decay times from the "
            "reverberant part (the direct path cut out). The verdicts on the scale of just noticeable differences of the decay time: the same up to 5 % "
            "(ISO 3382-1 after Seraphim; the author's start), audibly close up to 10 %, different beyond; the literature reports JNDs of 3 ... 26 % "
            "depending on the signal, the listener and the level. Differences are B - A.\n\n";
    if (! a.hasReverb || ! b.hasReverb)
    {
        text << "No comparison: a reverb was not found (A: " << juce::String(static_cast<int>(a.hasReverb)) << ", B: " << juce::String(static_cast<int>(b.hasReverb))
             << ").\n";
        return text;
    }
    const measure::ReverbComparison comparison = measure::compareReverbs(a, b);
    text << "Octaves: " << comparison.octavesSame << " the same, " << comparison.octavesClose << " audibly close, " << comparison.octavesDifferent << " different.\n\n";
    text << "| quantity | A | B | B - A |\n|---|---|---|---|\n";
    text << "| T30 broadband | " << juce::String(a.decay.broadband.t30, 3) << " s | " << juce::String(b.decay.broadband.t30, 3) << " s | "
         << juce::String(comparison.t30DifferencePercent, 1) << " % (" << measure::getJndVerdictName(comparison.t30Verdict) << ") |\n";
    text << "| EDT | " << juce::String(a.decay.broadband.edt, 3) << " s | " << juce::String(b.decay.broadband.edt, 3) << " s | "
         << juce::String(comparison.edtDifferencePercent, 1) << " % |\n";
    text << "| C80 | " << juce::String(a.decay.c80Db, 2) << " dB | " << juce::String(b.decay.c80Db, 2) << " dB | " << juce::String(comparison.c80DifferenceDb, 2)
         << " dB (not judged: the JND is to be checked) |\n";
    text << "| pre-delay | " << juce::String(a.decay.preDelaySeconds * 1000.0, 2) << " ms | " << juce::String(b.decay.preDelaySeconds * 1000.0, 2) << " ms | "
         << juce::String(comparison.preDelayDifferenceMs, 2) << " ms |\n";
    text << "| DRR | " << juce::String(a.decay.directToReverberantDb, 2) << " dB | " << juce::String(b.decay.directToReverberantDb, 2) << " dB | "
         << juce::String(comparison.drrDifferenceDb, 2) << " dB |\n";
    text << "| mixing time | " << juce::String(a.texture.mixingTimeSeconds * 1000.0, 1) << " ms | " << juce::String(b.texture.mixingTimeSeconds * 1000.0, 1)
         << " ms | " << juce::String(comparison.mixingDifferenceMs, 1) << " ms |\n";
    text << "| late kurtosis | " << juce::String(a.texture.lateKurtosis, 2) << " | " << juce::String(b.texture.lateKurtosis, 2) << " | "
         << juce::String(comparison.lateKurtosisDifference, 2) << " |\n";
    text << "| IACC late | " << juce::String(a.iaccLate.value, 3) << " | " << juce::String(b.iaccLate.value, 3) << " | " << juce::String(comparison.iaccLateDifference, 3)
         << " |\n\n";
    text << "## T30 per octave\n\n";
    addBandTable(text, comparison.octaves);
    text << "\n## T30 per third octave\n\n";
    addBandTable(text, comparison.thirdOctaves);
    return text;
}

juce::String summarizeEchoes(const MeasurementSummary& summary)
{
    const MeasurementRow* echoes = findRow(summary, "echoes after an impulse (channel 1)");
    if (echoes == nullptr)
    {
        return {};
    }
    if (echoes->value <= 0.0)
    {
        return "none";
    }
    juce::StringArray parts;
    if (const MeasurementRow* delay = findRow(summary, "delay time (first echo after the dry path)"))
    {
        parts.add(juce::String(delay->value, 1) + " ms");
    }
    if (const MeasurementRow* stereo = findRow(summary, "stereo (one input at a time)"))
    {
        if (! stereo->text.startsWith("independent"))
        {
            parts.add(stereo->text.upToFirstOccurrenceOf(",", false, false));
        }
    }
    if (const MeasurementRow* levels = findRow(summary, "level dependence of the echoes"))
    {
        if (levels->text.contains("SELF-OSCILLATING"))
        {
            parts.add("self-oscillating");
        }
        else if (levels->text.contains("saturation"))
        {
            parts.add("saturates");
        }
    }
    if (const MeasurementRow* modulation = findRow(summary, "delay modulation (impulse train)"))
    {
        if (modulation->value > 0.0)
        {
            parts.add("modulated");
        }
    }
    return parts.joinIntoString(", ");
}

juce::String summarizeReverb(const MeasurementSummary& summary)
{
    const MeasurementRow* reverb = findRow(summary, "reverb (a diffuse decaying tail, channel 1)");
    if (reverb == nullptr)
    {
        return {};
    }
    if (reverb->value <= 0.0)
    {
        return "none";
    }
    juce::StringArray parts;
    if (const MeasurementRow* t30 = findRow(summary, "T30 broadband"))
    {
        parts.add("T30 " + juce::String(t30->value, 2) + " s");
    }
    if (const MeasurementRow* preDelay = findRow(summary, "pre-delay (to the reverb's onset)"))
    {
        parts.add("pre-delay " + juce::String(juce::roundToInt(preDelay->value)) + " ms");
    }
    if (const MeasurementRow* iacc = findRow(summary, "IACC early (0 ... 80 ms, with the direct sound) and late"))
    {
        parts.add("IACC late " + juce::String(iacc->value, 2));
    }
    return parts.joinIntoString(", ");
}

juce::String summarizeDynamics(const MeasurementSummary& summary)
{
    const MeasurementRow* dynamics = findRow(summary, "dynamics (a level-dependent gain, channel 1)");
    if (dynamics == nullptr)
    {
        return {};
    }
    if (dynamics->value <= 0.0)
    {
        return "none";
    }
    juce::StringArray parts;
    const MeasurementRow* curve = findRow(summary, "static curve: threshold, ratio, knee, makeup");
    const MeasurementRow* time = findRow(summary, "time model (the best of four variants)");
    if (curve != nullptr)
    {
        parts.add(curve->text.upToFirstOccurrenceOf(", makeup", false, false));
    }
    if (time != nullptr)
    {
        juce::String timing = time->text.upToFirstOccurrenceOf(", hold", false, false);
        if (time->text.contains("; atypical"))
        {
            timing << " (atypical)";
        }
        parts.add(timing);
    }
    else if (findRow(summary, "dynamics: kind") != nullptr)
    {
        parts.add("not a compressor");
    }
    if (const MeasurementRow* detector = findRow(summary, "detector"))
    {
        parts.add(detector->text.upToFirstOccurrenceOf(" (", false, false));
    }
    if (const MeasurementRow* bands = findRowStartingWith(summary, "bands (the two-tone test"))
    {
        parts.add(bands->text.upToFirstOccurrenceOf(" (", false, false));
    }
    return parts.joinIntoString("; ");
}

const MeasurementRow* findRowStartingWith(const MeasurementSummary& summary, const juce::String& start)
{
    for (const MeasurementRow& row : summary.rows)
    {
        if (row.quantity.startsWith(start))
        {
            return &row;
        }
    }
    return nullptr;
}

double getPluginLatency(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& setting,
                        double sampleRate)
{
    return getReportedLatency(formatManager, description, setting, sampleRate);
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
