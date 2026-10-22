#include <cmath>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/Dynamics.h"
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
    }
};

static DynamicsTests dynamicsTests;
