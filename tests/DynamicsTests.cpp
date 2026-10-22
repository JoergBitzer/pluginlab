#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/Dynamics.h"
#include "pluginlab/reference/BandDynamics.h"
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
    }

private:
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
                                 Case{"the gain smoothed in dB with a hold of 30 ms", ref::Smoothing::GainLog, measure::TimeModel::GainLog, 0.03}})
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
};

static DynamicsTests dynamicsTests;
