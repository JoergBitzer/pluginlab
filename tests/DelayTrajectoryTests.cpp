#include <cmath>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/measure/DelayTrajectory.h"
#include "pluginlab/reference/FeedbackDelay.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kSampleRate = 48000.0;
constexpr double kRelativeTolerance = 0.01;   // the plan's "done when" of W11.1d: rate and depth within 1 %
constexpr int kPeriod = 2048;                 // the impulse train
constexpr int kImpulses = 94;                 // 4 s
constexpr double kToneHz = 3150.0;            // AES6
constexpr int kToneSamples = 4 * 48000;
constexpr int kToneEdge = 4096;

// the wet signal of the reference alone (mix 1, no feedback) for an input
std::vector<double> render(ref::FeedbackDelay& delay, const std::vector<float>& input)
{
    juce::AudioBuffer<float> buffer(1, static_cast<int>(input.size()));
    for (size_t sample = 0; sample < input.size(); ++sample)
    {
        buffer.setSample(0, static_cast<int>(sample), input[sample]);
    }
    delay.reset();
    delay.process(buffer);
    std::vector<double> output;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        output.push_back(buffer.getSample(0, sample));
    }
    return output;
}

ref::FeedbackDelaySettings makeSettings(double depth, ref::LfoShape shape)
{
    ref::FeedbackDelaySettings settings;
    settings.delaySamples = 480.0;
    settings.feedback = 0.0;
    settings.mix = 1.0;
    settings.lfoRateHz = 1.3;
    settings.lfoDepthSamples = depth;
    settings.lfoShape = shape;
    return settings;
}
}

// W11.1d: the delay trajectory (docs/design/W11-delay-plan.md 4.3) of the reference's LFO, by an impulse train and by the AES6 tone
class DelayTrajectoryTests : public juce::UnitTest
{
public:
    DelayTrajectoryTests()
        : juce::UnitTest("Delay trajectory and time change (W11.1d, h)", "pluginlab")
    {
    }

    void runTest() override
    {
        for (const ref::LfoShape shape : {ref::LfoShape::Sine, ref::LfoShape::Triangle})
        {
            juce::String kind = "sine";
            measure::LfoShapeFound expectedShape = measure::LfoShapeFound::Sine;
            if (shape == ref::LfoShape::Triangle)
            {
                kind = "triangle";
                expectedShape = measure::LfoShapeFound::Triangle;
            }
            const ref::FeedbackDelaySettings settings = makeSettings(24.0, shape);
            ref::FeedbackDelay delay(kSampleRate, settings);

            beginTest("an LFO (" + kind + ", 1.3 Hz, 24 samples) by the impulse train: rate and depth within 1 %, the shape");
            const measure::DelayTrajectory byImpulses =
                measure::measureTrajectoryFromImpulses(render(delay, measure::makeImpulseTrain(kImpulses, kPeriod, 0.5f)), kImpulses, kPeriod, {});
            expectTrajectory(byImpulses, settings, expectedShape);
            double worst = 0.0;
            for (const measure::TrajectoryPoint& point : byImpulses.points)
            {
                worst = std::max(worst, std::abs(point.delaySamples - delay.getDelayAt(juce::roundToInt(point.timeSamples))));
            }
            expectLessThan(worst, 0.5, "every point near the reference's tau (the delay moves within the arrival)");

            beginTest("an LFO (" + kind + ") by the 3150 Hz tone: rate and depth within 1 %, the shape, tau at every sample");
            const measure::DelayTrajectory byTone = measure::measureTrajectoryFromTone(render(delay, measure::makeTone(kToneHz, kToneSamples, kSampleRate, 0.5f)),
                                                                                       kToneHz, byImpulses.meanSamples, kToneEdge, {});
            expectTrajectory(byTone, settings, expectedShape);
            worst = 0.0;
            for (const measure::TrajectoryPoint& point : byTone.points)
            {
                worst = std::max(worst, std::abs(point.delaySamples - delay.getDelayAt(static_cast<juce::int64>(point.timeSamples))));
            }
            expectLessThan(worst, 0.1, "tau within 0.1 sample at every point");
        }

        beginTest("no LFO: no modulation found, the mean is the delay");
        {
            const ref::FeedbackDelaySettings settings = makeSettings(0.0, ref::LfoShape::Sine);
            ref::FeedbackDelay delay(kSampleRate, settings);
            const measure::DelayTrajectory trajectory =
                measure::measureTrajectoryFromImpulses(render(delay, measure::makeImpulseTrain(kImpulses, kPeriod, 0.5f)), kImpulses, kPeriod, {});
            expect(trajectory.shape == measure::LfoShapeFound::None);
            expectWithinAbsoluteError(trajectory.meanSamples, 480.0, 0.1);
        }

        testPlugin();
        testTimeChange();
    }

private:
    // W11.1h: the three ways of the reference to change its time, told apart by the tone; the glide's pitch within 1 cent
    void testTimeChange()
    {
        constexpr int kChangeAt = 2 * 48000;
        constexpr double kSwitch = 4800.0;
        // 963.8 = 480 + 31.75 periods of 3150 Hz: old and new a quarter period apart (a whole number of periods would hide a crossfade)
        struct Case
        {
            juce::String name;
            ref::TimeChange change;
            double to;
            measure::TimeChangeKind kind;
        };
        for (const Case& item : {Case{"jump", ref::TimeChange::Jump, 963.8, measure::TimeChangeKind::Jump},
                                 Case{"crossfade", ref::TimeChange::Crossfade, 963.8, measure::TimeChangeKind::Crossfade},
                                 Case{"glide", ref::TimeChange::Glide, 960.0, measure::TimeChangeKind::Glide}})
        {
            beginTest("changing the delay time: the " + item.name + " is recognised (480 to " + juce::String(item.to, 1) + " samples)");
            ref::FeedbackDelaySettings settings = makeSettings(0.0, ref::LfoShape::Sine);
            settings.timeChange = item.change;
            settings.switchSamples = kSwitch;
            ref::FeedbackDelay delay(kSampleRate, settings, 2000.0);
            const std::vector<float> tone = measure::makeTone(kToneHz, kToneSamples, kSampleRate, 0.5f);
            juce::AudioBuffer<float> buffer(1, kToneSamples);
            for (int sample = 0; sample < kToneSamples; ++sample)
            {
                buffer.setSample(0, sample, tone[static_cast<size_t>(sample)]);
            }
            delay.reset();
            juce::AudioBuffer<float> before(buffer.getArrayOfWritePointers(), 1, 0, kChangeAt);
            delay.process(before);
            settings.delaySamples = item.to;
            delay.setSettings(settings);
            juce::AudioBuffer<float> after(buffer.getArrayOfWritePointers(), 1, kChangeAt, kToneSamples - kChangeAt);
            delay.process(after);
            std::vector<double> output;
            for (int sample = 0; sample < kToneSamples; ++sample)
            {
                output.push_back(buffer.getSample(0, sample));
            }
            const measure::TimeChangeAnalysis analysis = measure::analyseTimeChange(output, kToneHz, 480.0, item.to, kToneEdge, kSampleRate);
            logMessage("start " + juce::String(analysis.startSamples, 0) + ", length " + juce::String(analysis.lengthSamples, 0) + ", delays "
                       + juce::String(analysis.delayBeforeSamples, 3) + " -> " + juce::String(analysis.delayAfterSamples, 3) + ", dip "
                       + juce::String(analysis.envelopeDipDb, 2) + " dB, glide " + juce::String(analysis.glidePitchCents, 2) + " cents, residual "
                       + juce::String(analysis.glideResidualRadians, 4));
            expect(analysis.kind == item.kind, "the kind");
            expectWithinAbsoluteError(analysis.delayBeforeSamples, 480.0, 0.1);
            expectWithinAbsoluteError(analysis.delayAfterSamples, item.to, 0.1);
            if (item.change == ref::TimeChange::Jump)
            {
                expectWithinAbsoluteError(analysis.startSamples, static_cast<double>(kChangeAt), 16.0, "where the jump is (the analytic signal spreads it)");
            }
            else
            {
                // a linear crossfade moves the phase very little at its ends: its start and length are blurred (by up to 10 %)
                expectWithinAbsoluteError(analysis.startSamples, static_cast<double>(kChangeAt), 0.1 * kSwitch, "where the change starts");
                expectWithinAbsoluteError(analysis.lengthSamples, kSwitch, 0.2 * kSwitch, "its length");
            }
            if (item.change == ref::TimeChange::Glide)
            {
                // dtau/dt = 480 / 4800 = 0.1: f_out = 0.9 f, 1200 log2(0.9) cents
                expectWithinAbsoluteError(analysis.glidePitchCents, 1200.0 * std::log2(0.9), 1.0, "the glide's pitch within 1 cent");
            }
        }
    }

    // the Reference Delay plugin with its LFO (0.5 Hz, 0.5 ms), feedback 0, as a measurement device: the impulse train finds the LFO
    void testPlugin()
    {
        beginTest("the Reference Delay plugin's LFO (0.5 Hz, 0.5 ms = 24 samples, around 250 ms) by the impulse train through the host");
        constexpr size_t kFeedback = 2;
        constexpr size_t kLfoDepth = 12;
        constexpr float kNoFeedback = 0.5f;     // -95 ... 95 %
        constexpr float kHalfMillisecond = 0.05f; // 0 ... 10 ms
        constexpr int kLongPeriod = 16384;      // longer than 250 ms plus the depth
        constexpr int kLongImpulses = 24;       // 8 s: four periods of the LFO
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getReferencePlugin("Delay"));
        expect(! scan.descriptions.isEmpty(), "the Reference Delay plugin");
        if (scan.descriptions.isEmpty())
        {
            return;
        }
        std::vector<float> setting = pluginlab::engine::getDefaultSetting(formatManager, scan.descriptions[0], kSampleRate);
        setting[kFeedback] = kNoFeedback;
        setting[kLfoDepth] = kHalfMillisecond;
        const measure::Device device = pluginlab::engine::makePluginDevice(formatManager, scan.descriptions[0], setting);
        const std::vector<float> train = measure::makeImpulseTrain(kLongImpulses, kLongPeriod, 0.5f);
        juce::AudioBuffer<float> input(2, static_cast<int>(train.size()));
        for (int channel = 0; channel < 2; ++channel)
        {
            for (size_t sample = 0; sample < train.size(); ++sample)
            {
                input.setSample(channel, static_cast<int>(sample), train[sample]);
            }
        }
        const juce::AudioBuffer<float> output = device(input, kSampleRate);
        std::vector<double> left;
        for (int sample = 0; sample < output.getNumSamples(); ++sample)
        {
            left.push_back(output.getSample(0, sample));
        }
        const measure::DelayTrajectory trajectory = measure::measureTrajectoryFromImpulses(left, kLongImpulses, kLongPeriod, {});
        ref::FeedbackDelaySettings expected;
        expected.delaySamples = 12000.0;
        expected.lfoRateHz = 0.5;
        expected.lfoDepthSamples = 24.0;
        expectTrajectory(trajectory, expected, measure::LfoShapeFound::Sine);
    }

    void expectTrajectory(const measure::DelayTrajectory& trajectory, const ref::FeedbackDelaySettings& settings, measure::LfoShapeFound shape)
    {
        logMessage("rate " + juce::String(trajectory.rateHz, 4) + " Hz, depth " + juce::String(trajectory.depthSamples, 3) + ", mean "
                   + juce::String(trajectory.meanSamples, 3) + ", residuals sine " + juce::String(trajectory.sineResidualSamples, 4) + " triangle "
                   + juce::String(trajectory.triangleResidualSamples, 4) + ", " + juce::String(trajectory.pitchDeviationCents, 2) + " cents, "
                   + juce::String(static_cast<int>(trajectory.points.size())) + " points");
        expectWithinAbsoluteError(trajectory.rateHz, settings.lfoRateHz, kRelativeTolerance * settings.lfoRateHz, "the rate");
        expectWithinAbsoluteError(trajectory.depthSamples, settings.lfoDepthSamples, kRelativeTolerance * settings.lfoDepthSamples, "the depth");
        expectWithinAbsoluteError(trajectory.meanSamples, settings.delaySamples, 0.1, "the mean");
        expect(trajectory.shape == shape, "the shape");
    }
};

static DelayTrajectoryTests delayTrajectoryTests;
