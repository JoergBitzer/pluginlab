#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/PluginMeasurement.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/match/DynamicsDomain.h"
#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/Dynamics.h"
#include "pluginlab/reference/BandDynamics.h"
#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/ui/DynamicsViews.h"
#include "pluginlab/reference/Compressor.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kSampleRate = 48000.0;
constexpr double kCurveTolerance = 0.1;   // dB: the plan's "done when" of W11.3a
constexpr double kParameterDb = 0.2;      // W11.3b: threshold, knee, makeup within 0.2 dB ...
constexpr double kRatioShare = 0.01;      // ... the ratio within 1 %

ref::CompressorSettings makeSettings()
{
    ref::CompressorSettings settings;
    settings.sampleRate = kSampleRate;
    settings.thresholdDb = -20.0;
    settings.ratio = 4.0;
    settings.kneeDb = 6.0;
    settings.makeupDb = 3.0;
    settings.attackSeconds = 0.005;
    settings.releaseSeconds = 0.05;
    return settings;
}

measure::Device makeDevice(const ref::CompressorSettings& settings)
{
    return measure::makeProcessorDevice([settings](double rate)
    {
        ref::CompressorSettings copy = settings;
        copy.sampleRate = rate;
        return std::make_unique<ref::Compressor>(copy);
    });
}
}

// W11.3a, W11.3b: the gain envelope, the static curve from the staircase, its fit (docs/design/W11-dynamics-plan.md 4.0, 4.1)
class DynamicsTests : public juce::UnitTest
{
public:
    DynamicsTests()
        : juce::UnitTest("Dynamics (W11.3)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the gain envelope of a square wave is the compressor's own gain, sample by sample");
        {
            ref::CompressorSettings settings = makeSettings();
            settings.lookaheadSeconds = 0.002;
            ref::Compressor compressor(settings);
            compressor.setRecording(true);
            measure::StaircaseSettings staircase;
            staircase.levelsDbfs = {-40.0, -6.0, -40.0};
            staircase.stepSeconds = 0.3;
            const std::vector<float> signal = measure::makeStaircase(staircase);
            juce::AudioBuffer<float> buffer(1, static_cast<int>(signal.size()));
            buffer.copyFrom(0, 0, signal.data(), static_cast<int>(signal.size()));
            compressor.process(buffer);
            std::vector<double> output;
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                output.push_back(buffer.getSample(0, sample));
            }
            const std::vector<double> envelope = measure::getGainEnvelopeDb(std::vector<double>(signal.begin(), signal.end()), output, compressor.getLatencySamples());
            double worst = 0.0;
            for (size_t sample = static_cast<size_t>(compressor.getLatencySamples()); sample < envelope.size(); ++sample)
            {
                worst = std::max(worst, std::abs(envelope[sample] - compressor.getGainTraceDb()[sample]));
            }
            expectLessThan(worst, 1.0e-4, "the envelope equals the gain applied (float output)");
        }

        beginTest("the static curve from the rising staircase within 0.1 dB of the gain computer; no hysteresis for a compressor");
        const ref::CompressorSettings settings = makeSettings();
        const ref::Compressor reference(settings);
        measure::StaticCurveSettings curveSettings;
        curveSettings.stepSeconds = 0.5;
        const measure::StaticCurve curve = measure::measureStaticCurve(makeDevice(settings), 1, curveSettings);
        double worst = 0.0;
        for (const measure::StaticPoint& point : curve.rising)
        {
            worst = std::max(worst, std::abs(point.gainDb - reference.getStaticGainDb(point.inputDbfs)));
        }
        logMessage("static curve: largest error " + juce::String(worst, 4) + " dB, hysteresis " + juce::String(curve.hysteresisDb, 4) + " dB, unsettled steps "
                   + juce::String(curve.unsettledSteps));
        expectLessThan(worst, kCurveTolerance, "every step");
        expectEquals(curve.unsettledSteps, 0, "every step settled");
        expectLessThan(curve.hysteresisDb, 0.05, "rising and falling alike");

        beginTest("the fit recovers threshold, ratio, knee and makeup (hard and soft knee)");
        for (const double knee : {0.0, 6.0, 12.0})
        {
            ref::CompressorSettings kneeSettings = makeSettings();
            kneeSettings.kneeDb = knee;
            const measure::StaticCurve kneeCurve = measure::measureStaticCurve(makeDevice(kneeSettings), 1, curveSettings);
            const measure::StaticFit fit = measure::fitStaticCurve(kneeCurve.rising);
            logMessage("knee " + juce::String(knee, 0) + " dB: threshold " + juce::String(fit.thresholdDb, 3) + ", ratio " + juce::String(fit.ratio, 4) + ", knee "
                       + juce::String(fit.kneeDb, 3) + ", makeup " + juce::String(fit.makeupDb, 3) + ", rms error " + juce::String(fit.rmsErrorDb, 4) + " dB");
            expectWithinAbsoluteError(fit.thresholdDb, kneeSettings.thresholdDb, kParameterDb, "threshold");
            expectWithinAbsoluteError(fit.ratio, kneeSettings.ratio, kRatioShare * kneeSettings.ratio, "ratio");
            expectWithinAbsoluteError(fit.kneeDb, knee, kParameterDb, "knee");
            expectWithinAbsoluteError(fit.makeupDb, kneeSettings.makeupDb, kParameterDb, "makeup");
            expect(fit.compresses);
        }
        testTimeBehaviour();
        testDetectorAndBands();
        testDistortionAndLimiter();
        testReportUnit();
        testComparisonAndMatch();
    }

private:
    // W11.3e: the distortion grows at low frequencies with a faster release; the true peak; a limiter's ceiling and overshoot
    void testDistortionAndLimiter()
    {
        beginTest("the distortion of the dynamics: at 50 Hz the THD grows when the release gets faster");
        std::vector<double> thd50;
        for (const double release : {0.5, 0.05, 0.005})
        {
            ref::CompressorSettings settings = makeSettings();
            settings.kneeDb = 0.0;
            settings.attackSeconds = 0.001;
            settings.releaseSeconds = release;
            const measure::DynamicsDistortion distortion = measure::measureDynamicsDistortion(makeDevice(settings), 1, kSampleRate, -1.0, {50.0, 100.0, 1000.0});
            logMessage("release " + juce::String(release * 1000.0, 0) + " ms: THD at 50 Hz " + juce::String(distortion.thdDb[0], 1) + " dB, 100 Hz "
                       + juce::String(distortion.thdDb[1], 1) + " dB, 1 kHz " + juce::String(distortion.thdDb[2], 1) + " dB");
            thd50.push_back(distortion.thdDb[0]);
        }
        expectGreaterThan(thd50[1], thd50[0] + 6.0, "50 ms against 500 ms");
        expectGreaterThan(thd50[2], thd50[1] + 6.0, "5 ms against 50 ms");

        beginTest("the true peak: a sine at fs/4 with 45 degrees has its samples 3.01 dB below its peak");
        {
            std::vector<double> sine;
            for (int sample = 0; sample < 4800; ++sample)
            {
                sine.push_back(std::sin(3.14159265358979323846 / 2.0 * sample + 3.14159265358979323846 / 4.0));
            }
            expectWithinAbsoluteError(measure::getSamplePeakDbfs(sine), -3.0103, 0.001);
            expectWithinAbsoluteError(measure::getTruePeakDbfs(sine), 0.0, 0.05, "the true peak");
        }

        beginTest("a limiter (ratio 100, threshold -1 dBFS): the sample peak at the ceiling, the true peak 3 dB above; look-ahead takes the overshoot");
        for (const double lookahead : {0.0, 0.005})
        {
            ref::CompressorSettings settings = makeSettings();
            settings.thresholdDb = -1.0;
            settings.ratio = 100.0;
            settings.kneeDb = 0.0;
            settings.makeupDb = 0.0;
            settings.attackSeconds = 0.0005;
            settings.releaseSeconds = 0.1;
            settings.lookaheadSeconds = lookahead;
            const measure::LimiterTest limiter = measure::measureLimiter(makeDevice(settings), 1, kSampleRate);
            logMessage("look-ahead " + juce::String(lookahead * 1000.0, 0) + " ms: sample peak " + juce::String(limiter.samplePeakDbfs, 2) + " dBFS, true peak "
                       + juce::String(limiter.truePeakDbfs, 2) + " dBFS, overshoot " + juce::String(limiter.overshootDb, 2) + " dB");
            expectWithinAbsoluteError(limiter.samplePeakDbfs, -1.0 + 7.0 / 100.0, 0.1, "the ceiling (+6 dBFS in: 7 dB over at ratio 100)");
            expectWithinAbsoluteError(limiter.truePeakDbfs - limiter.samplePeakDbfs, 3.01, 0.1, "the inter-sample peak a sample-peak limiter lets through");
            if (lookahead > 0.0)
            {
                expectLessThan(limiter.overshootDb, 0.5, "with look-ahead (10 attack times): hardly any overshoot");
            }
            else
            {
                expectGreaterThan(limiter.overshootDb, 3.0, "without: the step overshoots");
            }
        }
    }

    // W11.3d: peak and RMS told apart, the side chain's high-pass, single band / multiband / dynamic EQ, the stereo link
    void testDetectorAndBands()
    {
        beginTest("the detector: peak and RMS told apart (a 4 kHz square with every second period zero)");
        for (const ref::Detector detector : {ref::Detector::Peak, ref::Detector::Rms})
        {
            ref::CompressorSettings settings = makeSettings();
            settings.kneeDb = 0.0;
            settings.makeupDb = 0.0;
            settings.detector = detector;
            const measure::Device device = makeDevice(settings);
            const measure::StaticFit curve = measure::fitStaticCurve(measure::measureStaticCurve(device, 1, {}).rising);
            const measure::StepResponse step = measure::measureStepResponse(device, 1, curve, {});
            const measure::DetectorTest test = measure::measureDetector(device, 1, curve, step.best, kSampleRate);
            logMessage(juce::String(measure::getDetectorKindName(test.kind)) + ": the pulses change the gain by " + juce::String(test.crestChangeDb, 3) + " dB (peak: "
                       + juce::String(test.peakChangeDb, 3) + " dB); the zeros by " + juce::String(test.gainChangeDb, 3) + " dB (RMS: " + juce::String(test.rmsChangeDb, 3) + " dB, the time model as a peak detector: "
                       + juce::String(test.peakModelChangeDb, 3) + " dB)");
            measure::DetectorKind expected = measure::DetectorKind::Peak;
            if (detector == ref::Detector::Rms)
            {
                expected = measure::DetectorKind::Rms;
            }
            expect(test.kind == expected, "the detector");
        }

        beginTest("the side chain's high-pass (1 kHz): the threshold rises towards low frequencies");
        {
            ref::CompressorSettings settings = makeSettings();
            settings.kneeDb = 0.0;
            settings.sidechainHighPassHz = 1000.0;
            const measure::Device device = makeDevice(settings);
            const double at100 = measure::measureThresholdAt(device, 1, 100.0, kSampleRate);
            const double at1k = measure::measureThresholdAt(device, 1, 1000.0, kSampleRate);
            const double at10k = measure::measureThresholdAt(device, 1, 10000.0, kSampleRate);
            logMessage("1 dB of reduction at 100 Hz: " + juce::String(at100, 2) + ", 1 kHz: " + juce::String(at1k, 2) + ", 10 kHz: " + juce::String(at10k, 2) + " dBFS");
            expect(std::isnan(at100) || at100 > at10k + 20.0, "100 Hz: 40 dB down in the side chain");
            expectWithinAbsoluteError(at1k - at10k, 3.0, 1.0, "1 kHz: the high-pass's -3 dB");
        }

        std::vector<double> probes;
        for (double frequency = 50.0; frequency <= 15000.0; frequency *= std::pow(2.0, 1.0 / 6.0))
        {
            probes.push_back(frequency); // a sixth of an octave: a bell is resolved
        }
        struct Case
        {
            juce::String name;
            std::function<std::unique_ptr<ref::Processor>(double)> factory;
            double toneHz;
            measure::BandKind kind;
        };
        ref::CompressorSettings bandSettings = makeSettings();
        bandSettings.kneeDb = 0.0;
        bandSettings.makeupDb = 0.0;
        for (const Case& item : {Case{"a compressor: single band", [bandSettings](double rate) { ref::CompressorSettings copy = bandSettings; copy.sampleRate = rate; return std::unique_ptr<ref::Processor>(std::make_unique<ref::Compressor>(copy)); }, 300.0, measure::BandKind::SingleBand},
                                 Case{"two bands, crossover 2 kHz: multiband", [bandSettings](double rate) { ref::CompressorSettings copy = bandSettings; copy.sampleRate = rate; return std::unique_ptr<ref::Processor>(std::make_unique<ref::TwoBandCompressor>(copy, 2000.0)); }, 300.0, measure::BandKind::Multiband},
                                 Case{"a dynamic EQ band at 1 kHz: dynamic EQ", [bandSettings](double rate) { ref::CompressorSettings copy = bandSettings; copy.sampleRate = rate; return std::unique_ptr<ref::Processor>(std::make_unique<ref::DynamicEq>(copy, 1000.0, 2.0)); }, 1000.0, measure::BandKind::DynamicEq}})
        {
            beginTest("the two-tone test: " + item.name);
            const measure::DynamicTransfer transfer = measure::measureDynamicTransfer(measure::makeProcessorDevice(item.factory), 1, kSampleRate, item.toneHz, 0.0, -40.0, probes);
            juce::String curve;
            for (size_t index = 0; index < transfer.frequencies.size(); ++index)
            {
                curve << juce::String(juce::roundToInt(transfer.frequencies[index])) << ": " << juce::String(transfer.reductionDb[index], 1) << "  ";
            }
            logMessage(juce::String(measure::getBandKindName(transfer.kind)) + " (within 3 dB of the largest over " + juce::String(transfer.plateauOctaves, 2) + " octaves): " + curve);
            expect(transfer.kind == item.kind, "the kind");
        }

        beginTest("the stereo link: 0, 50 and 100 %");
        for (const double link : {0.0, 0.5, 1.0})
        {
            ref::CompressorSettings settings = makeSettings();
            settings.kneeDb = 0.0;
            settings.stereoLink = link;
            const ref::Compressor reference(settings);
            // the quiet channel (-30 dBFS) hears (1 - link) (-30) + link (0); the loud one 0 dBFS
            const double expected = (reference.getSettings().makeupDb - reference.getStaticGainDb((1.0 - link) * -30.0))
                                    / (reference.getSettings().makeupDb - reference.getStaticGainDb(0.0));
            const double measured = measure::measureStereoLink(makeDevice(settings), kSampleRate);
            expectWithinAbsoluteError(measured, expected, 0.02, "link " + juce::String(link, 1));
        }
    }

    // W11.3c: every model variant of the reference recognised with its time constants within 2 %; hold, look-ahead, the standards' convention
    void testTimeBehaviour()
    {
        constexpr double kTimeShare = 0.02;
        struct Case
        {
            juce::String name;
            ref::Smoothing smoothing;
            measure::TimeModel model;
            double holdSeconds;
        };
        for (const Case& item : {Case{"the gain smoothed in dB", ref::Smoothing::GainLog, measure::TimeModel::GainLog, 0.0},
                                 Case{"the level smoothed in dB", ref::Smoothing::LevelLog, measure::TimeModel::LevelLog, 0.0},
                                 Case{"two stages", ref::Smoothing::TwoStage, measure::TimeModel::TwoStage, 0.0},
                                 Case{"the gain smoothed in dB with a hold of 30 ms", ref::Smoothing::GainLog, measure::TimeModel::GainLog, 0.03},
                                 Case{"the level smoothed as an amplitude (atypical)", ref::Smoothing::LevelLinear, measure::TimeModel::LevelLinear, 0.0}})
        {
            beginTest("the step response: " + item.name + " recognised, the time constants within 2 %");
            ref::CompressorSettings settings = makeSettings();
            settings.kneeDb = 0.0;
            settings.smoothing = item.smoothing;
            settings.holdSeconds = item.holdSeconds;
            const measure::Device device = makeDevice(settings);
            measure::StaticCurveSettings curveSettings;
            const measure::StaticFit curve = measure::fitStaticCurve(measure::measureStaticCurve(device, 1, curveSettings).rising);
            const measure::StepResponse step = measure::measureStepResponse(device, 1, curve, {});
            juce::String fits;
            for (const measure::TimeFit& fit : step.fits)
            {
                fits << measure::getTimeModelName(fit.model) << ": " << juce::String(fit.rmsErrorDb, 4) << " dB; ";
            }
            logMessage(fits + "best: attack " + juce::String(step.best.parameters.attackSeconds * 1000.0, 3) + " ms, release "
                       + juce::String(step.best.parameters.releaseSeconds * 1000.0, 3) + " ms, detector " + juce::String(step.best.parameters.detectorAttackSeconds * 1000.0, 3)
                       + " / " + juce::String(step.best.parameters.detectorReleaseSeconds * 1000.0, 3) + " ms, hold " + juce::String(step.best.parameters.holdSeconds * 1000.0, 2)
                       + " ms; settling (3 / 4 dB) " + juce::String(step.attackSettleSeconds * 1000.0, 2) + " / " + juce::String(step.releaseSettleSeconds * 1000.0, 2) + " ms");
            expect(step.best.model == item.model, "the variant");
            expect(measure::isAtypicalTimeModel(step.best.model) == (item.model == measure::TimeModel::LevelLinear), "atypical only for the linear level");
            expectWithinAbsoluteError(step.best.parameters.attackSeconds, settings.attackSeconds, kTimeShare * settings.attackSeconds, "attack");
            expectWithinAbsoluteError(step.best.parameters.releaseSeconds, settings.releaseSeconds, kTimeShare * settings.releaseSeconds, "release");
            if (item.model == measure::TimeModel::TwoStage)
            {
                expectWithinAbsoluteError(step.best.parameters.detectorAttackSeconds, settings.detectorAttackSeconds, kTimeShare * settings.detectorAttackSeconds,
                                          "the detector's attack");
                expectWithinAbsoluteError(step.best.parameters.detectorReleaseSeconds, settings.detectorReleaseSeconds, kTimeShare * settings.detectorReleaseSeconds,
                                          "the detector's release");
            }
            expectWithinAbsoluteError(step.best.parameters.holdSeconds, item.holdSeconds, 0.0005, "the hold (the model's)");
        }

        beginTest("the look-ahead from the cross-correlation with noise: 2 ms = 96 samples");
        {
            ref::CompressorSettings settings = makeSettings();
            settings.lookaheadSeconds = 0.002;
            expectEquals(measure::measureLookahead(makeDevice(settings), 1, kSampleRate), 96);
        }
    }

    // W11.3f: the unit "dynamics" of the reports on the reference as a device, none for a filter, and the Reference Compressor plugin through the host
    void testReportUnit()
    {
        beginTest("the unit 'dynamics' of the reports: the reference compressor's curve, times, detector, bands and link; none for a filter");
        pluginlab::engine::MeasurementOptions options;
        options.units = {"dynamics"};
        ref::CompressorSettings settings = makeSettings();
        const pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measureDevice(makeDevice(settings), 2, "compressor", options);
        for (const pluginlab::engine::MeasurementRow& row : summary.rows)
        {
            logMessage(row.quantity + ": " + row.text);
        }
        const pluginlab::engine::MeasurementRow* curve = pluginlab::engine::findRow(summary, "static curve: threshold, ratio, knee, makeup");
        expect(curve != nullptr && std::abs(curve->value - settings.thresholdDb) < kParameterDb, "the threshold");
        const pluginlab::engine::MeasurementRow* time = pluginlab::engine::findRow(summary, "time model (the best of four variants)");
        expect(time != nullptr && std::abs(time->value - settings.attackSeconds * 1000.0) < 0.02 * settings.attackSeconds * 1000.0, "the attack");
        const juce::String line = pluginlab::engine::summarizeDynamics(summary);
        logMessage(line);
        expect(line.startsWith("-20.0 dBFS, 4.00 : 1, knee 6.0 dB; gain reduction smoothed in dB"), line);
        expect(line.endsWith("; peak; single band"), line);
        const pluginlab::engine::MeasurementRow* link = pluginlab::engine::findRow(summary, "stereo link (the quiet channel's reduction re the loud one's)");
        expect(link != nullptr && std::abs(link->value - 1.0) < 0.02, "fully linked");

        beginTest("the views of a compressor: five pictures and their Markdown; the gain curves on music in one picture");
        {
            expect(summary.dynamics != nullptr, "the measurement kept for the views");
            const juce::TemporaryFile folder;
            folder.getFile().createDirectory();
            if (summary.dynamics != nullptr)
            {
                const juce::String markdown = pluginlab::ui::writeDynamicsViews(*summary.dynamics, kSampleRate, folder.getFile(), "comp", "compressor");
                for (const juce::String view : {"static_curve", "gain_reduction", "attack", "release", "two_tone"})
                {
                    expect(folder.getFile().getChildFile("comp_" + view + ".png").existsAsFile(), view);
                    expect(markdown.contains("comp_" + view + ".png"), view + " in the Markdown");
                }
            }
            folder.getFile().deleteRecursively();
            measure::GainCurve curve;
            curve.times = {0.005, 0.015, 0.025};
            curve.gainDb = {0.0, -3.0, std::nan("")};
            curve.inputLevelDbfs = {-20.0, -10.0, -90.0};
            const juce::Image image = pluginlab::ui::renderMusicGainCurves({{"A", curve}, {"B", curve}}, "two");
            expectEquals(image.getWidth(), 1000);
        }

        const measure::Device filter = measure::makeProcessorDevice([](double rate)
        {
            return std::make_unique<ref::BiquadCascade>(ref::designButterworthBandPass(3, rate, 500.0, 2000.0), rate);
        });
        expect(pluginlab::engine::summarizeDynamics(pluginlab::engine::measureDevice(filter, 2, "band-pass", options)) == "none", "a filter has no dynamics");

        beginTest("the Reference Compressor plugin through the host with a look-ahead of 2 ms: the latency reported, the curve and times recovered");
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getReferencePlugin("Compressor"));
        expect(! scan.descriptions.isEmpty(), "the Reference Compressor plugin");
        if (scan.descriptions.isEmpty())
        {
            return;
        }
        constexpr size_t kLookahead = 8;
        std::vector<float> setting = pluginlab::engine::getDefaultSetting(formatManager, scan.descriptions[0], kSampleRate);
        setting[kLookahead] = 0.2f; // 2 ms of 0 ... 10 ms
        const pluginlab::engine::MeasurementSummary plugin = pluginlab::engine::measurePlugin(formatManager, scan.descriptions[0], setting, options);
        for (const pluginlab::engine::MeasurementRow& row : plugin.rows)
        {
            logMessage(row.quantity + ": " + row.text);
        }
        const juce::String pluginLine = pluginlab::engine::summarizeDynamics(plugin);
        expect(pluginLine.startsWith("-20.0 dBFS, 4.00 : 1, knee 6.0 dB; gain reduction smoothed in dB"), pluginLine);
        const pluginlab::engine::MeasurementRow* pluginTime = pluginlab::engine::findRow(plugin, "time model (the best of four variants)");
        expect(pluginTime != nullptr && std::abs(pluginTime->value - 5.0) < 0.1, "the attack 5 ms");
        const pluginlab::engine::MeasurementRow* lead = pluginlab::engine::findRow(plugin, "look-ahead (the gain moves before the audio)");
        expect(lead != nullptr && juce::roundToInt(lead->value) == 96, "the look-ahead: 96 samples");
    }

    // W11.3g: two compressors compared
    void testComparisonAndMatch()
    {
        const auto measureSettings = [](const ref::CompressorSettings& settings)
        {
            measure::DynamicsMeasurementSettings dynamics;
            dynamics.channels = 1;
            dynamics.bands = false;
            dynamics.quietDbfs = -50.0;
            return measure::measureDynamicsResponse(makeDevice(settings), dynamics);
        };
        const ref::CompressorSettings base = makeSettings();
        const measure::DynamicsMeasurement master = measureSettings(base);
        beginTest("the comparison: a compressor against itself is the same; 6 : 1 against 4 : 1 differs in the curve; 6 ms against 5 ms attack is close");
        {
            const measure::DynamicsComparison same = measure::compareDynamics(master, master);
            expect(same.curve == measure::DynamicsVerdict::Same && same.attack == measure::DynamicsVerdict::Same && same.release == measure::DynamicsVerdict::Same,
                   "itself");
            expectWithinAbsoluteError(same.stepRmsDifferenceDb, 0.0, 1.0e-9, "the step's gain curves");
            ref::CompressorSettings steeper = base;
            steeper.ratio = 6.0;
            const measure::DynamicsComparison ratio = measure::compareDynamics(master, measureSettings(steeper));
            // at 0 dBFS: 20 dB over the threshold, 15 dB against 16.7 dB of reduction
            logMessage("6 : 1 against 4 : 1: largest " + juce::String(ratio.largestCurveDifferenceDb, 2) + " dB at " + juce::String(ratio.largestAtDbfs, 0) + " dBFS");
            expect(ratio.curve == measure::DynamicsVerdict::Different, "the curve differs");
            expectWithinAbsoluteError(ratio.largestCurveDifferenceDb, -(20.0 * (1.0 - 1.0 / 6.0) - 20.0 * (1.0 - 1.0 / 4.0)), 0.01, "1.67 dB at 0 dBFS");
            ref::CompressorSettings slower = base;
            slower.attackSeconds = 0.006;
            const measure::DynamicsComparison attack = measure::compareDynamics(master, measureSettings(slower));
            expectWithinAbsoluteError(attack.attackRatio, 1.2, 0.024, "x 1.2");
            expect(attack.attack == measure::DynamicsVerdict::Close, "close");
            expect(attack.release == measure::DynamicsVerdict::Same, "the release the same");
            const juce::String report = pluginlab::engine::createDynamicsComparisonReport(master, measureSettings(slower), "A", "B");
            for (const juce::String expected : {"# Dynamics comparison", "| attack | 5.00 ms | 6.00 ms | x 1.20: close |", "## The static curves"})
            {
                expect(report.contains(expected), "the report contains " + expected);
            }
        }

    }
};

static DynamicsTests dynamicsTests;

// W11.3g: the Reference Compressor plugin matched to itself from its defaults (the dynamics domain of the matcher); a group of its own (about
// 160 s in Debug), so that "Dynamics (W11.3)" stays within its time limit when the tests run side by side
class DynamicsMatchTests : public juce::UnitTest
{
public:
    DynamicsMatchTests()
        : juce::UnitTest("Dynamics matcher (W11.3g)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the dynamics domain of the matcher: the Reference Compressor plugin matched to itself (-30 dBFS, 6 : 1, 10 / 100 ms from the defaults)");
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getReferencePlugin("Compressor"));
        expect(! scan.descriptions.isEmpty(), "the Reference Compressor plugin");
        if (scan.descriptions.isEmpty())
        {
            return;
        }
        const juce::PluginDescription description = scan.descriptions[0];
        // the normalised values of the linear ranges: threshold -60 ... 0 dBFS, ratio 1 ... 20, attack 0.1 ... 200 ms, release 5 ... 2000 ms
        constexpr size_t kThreshold = 0;
        constexpr size_t kRatio = 1;
        constexpr size_t kAttack = 4;
        constexpr size_t kRelease = 5;
        const std::vector<float> defaults = pluginlab::engine::getDefaultSetting(formatManager, description, kSampleRate);
        std::vector<float> masterSetting = defaults;
        masterSetting[kThreshold] = static_cast<float>((-30.0 + 60.0) / 60.0);
        masterSetting[kRatio] = static_cast<float>((6.0 - 1.0) / 19.0);
        masterSetting[kAttack] = static_cast<float>((10.0 - 0.1) / 199.9);
        masterSetting[kRelease] = static_cast<float>((100.0 - 5.0) / 1995.0);
        const auto probe = std::make_shared<pluginlab::match::DynamicsProbe>();
        const std::vector<double> masterFeatures = probe->measure(pluginlab::engine::makePluginDevice(formatManager, description, masterSetting));
        pluginlab::match::ParameterSpace space;
        space.base = defaults;
        for (const size_t index : {kThreshold, kRatio, kAttack, kRelease})
        {
            space.released.push_back({static_cast<int>(index), "parameter " + juce::String(static_cast<int>(index)), 0.0, 1.0, defaults[index]});
        }
        pluginlab::match::MatchProblem problem(
            masterFeatures, [&formatManager, description](const std::vector<float>& setting) { return pluginlab::engine::makePluginDevice(formatManager, description, setting); },
            probe, pluginlab::match::makeDynamicsObjective(masterFeatures.size()), space);
        pluginlab::match::Budget budget;
        budget.evaluations = 150;
        budget.seconds = 600.0;
        const int scanned = problem.scanStart(9);
        const pluginlab::match::OptimizerResult result = problem.solve(budget);
        logMessage("the start scan: " + juce::String(scanned) + " renders");
        const double threshold = -60.0 + 60.0 * result.values[0];
        const double ratio = 1.0 + 19.0 * result.values[1];
        const double attack = 0.1 + 199.9 * result.values[2];
        const double release = 5.0 + 1995.0 * result.values[3];
        logMessage("matched: " + juce::String(threshold, 2) + " dBFS, " + juce::String(ratio, 3) + " : 1, attack " + juce::String(attack, 3) + " ms, release "
                   + juce::String(release, 2) + " ms; largest residual " + juce::String(result.largest, 3) + " units, " + juce::String(result.evaluations) + " renders, "
                   + pluginlab::match::getStopReasonName(result.reason));
        expect(result.withinTolerance, "within 0.5 dB in every feature");
        expectWithinAbsoluteError(threshold, -30.0, 0.2, "the threshold");
        // the match stops within 0.5 dB in every feature: a ratio of 5.85 instead of 6 moves the gain at 0 dBFS (30 dB over the threshold) by 0.13 dB
        expectWithinAbsoluteError(ratio, 6.0, 0.15, "the ratio");
        expectWithinAbsoluteError(attack, 10.0, 0.5, "the attack");
        expectWithinAbsoluteError(release, 100.0, 5.0, "the release");
    }
};

static DynamicsMatchTests dynamicsMatchTests;
