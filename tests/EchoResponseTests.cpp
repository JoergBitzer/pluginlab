#include <cmath>
#include <complex>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/EchoResponse.h"
#include "pluginlab/reference/FeedbackDelay.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr double kTimeTolerance = 0.1;   // samples (the plan's "done when" of W11.1b)
constexpr double kGainTolerance = 0.05;  // dB
constexpr double kLoopTolerance = 0.1;   // dB

double toDb(double magnitude)
{
    return 20.0 * std::log10(magnitude);
}

// the response of the delay to a unit impulse, `latency` samples of silence in front (a plugin's latency)
std::vector<double> renderResponse(ref::FeedbackDelay& delay, int samples, int latency = 0)
{
    juce::AudioBuffer<float> buffer(1, samples);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    delay.reset();
    delay.process(buffer);
    std::vector<double> response(static_cast<size_t>(latency), 0.0);
    for (int sample = 0; sample < samples; ++sample)
    {
        response.push_back(buffer.getSample(0, sample));
    }
    return response;
}

// the samples until the echoes are 120 dB down
int getLength(const ref::FeedbackDelaySettings& settings)
{
    const int echoes = static_cast<int>(std::ceil(-6.0 / std::log10(std::abs(settings.feedback)))) + 2;
    return static_cast<int>(settings.delaySamples * (echoes + 1)) + echoes;
}
}

// W11.1b: the reference delay (docs/design/W11-delay-plan.md 7) and the echo response measured on it (4.1): every value has an exact answer
class EchoResponseTests : public juce::UnitTest
{
public:
    EchoResponseTests()
        : juce::UnitTest("Echo response (W11.1b)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the reference delay: its output is its own transfer function (fractional, both interpolations, the loop filters, both orders)");
        for (const ref::DelayInterpolation interpolation : {ref::DelayInterpolation::Lagrange, ref::DelayInterpolation::AllPass})
        {
            for (const ref::LoopOrder order : {ref::LoopOrder::Exact, ref::LoopOrder::AuthorsOrder})
            {
                ref::FeedbackDelaySettings settings;
                settings.delaySamples = 37.6;
                settings.feedback = -0.6;
                settings.mix = 0.4;
                settings.lowCutHz = 150.0;
                settings.highCutHz = 6000.0;
                settings.interpolation = interpolation;
                settings.order = order;
                ref::FeedbackDelay delay(kSampleRate, settings);
                const std::vector<double> response = renderResponse(delay, 16384);
                double worst = 0.0;
                for (const double frequency : {50.0, 200.0, 997.0, 3100.0, 8000.0, 15000.0})
                {
                    std::complex<double> measured;
                    for (size_t sample = 0; sample < response.size(); ++sample)
                    {
                        measured += response[sample] * std::polar(1.0, -2.0 * kPi * frequency * static_cast<double>(sample) / kSampleRate);
                    }
                    worst = std::max(worst, std::abs(measured - delay.getResponse(frequency)));
                }
                expectLessThan(worst, 1.0e-4);
            }
        }

        beginTest("integer delay, feedback, mix: the direct path, the echoes, the delay time, the feedback per repeat, the mix");
        {
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 4800.0;
            settings.feedback = 0.5;
            settings.mix = 0.5;
            ref::FeedbackDelay delay(kSampleRate, settings);
            const measure::EchoAnalysis analysis = measure::analyseEchoes(renderResponse(delay, getLength(settings)), {});
            expect(analysis.hasDirect, "the dry path");
            expectWithinAbsoluteError(analysis.direct.gainDb, toDb(1.0 - settings.mix), kGainTolerance);
            expectEchoes(analysis, delay, 8);
            expectWithinAbsoluteError(analysis.delaySamples, 4800.0, kTimeTolerance);
            expectWithinAbsoluteError(analysis.spacingSamples, 4800.0, kTimeTolerance);
            expectWithinAbsoluteError(analysis.feedbackDb, toDb(0.5), kGainTolerance);
            expectWithinAbsoluteError(analysis.decayDbPerSecond, toDb(0.5) * 10.0, 10.0 * kGainTolerance);
            expectWithinAbsoluteError(analysis.mixDb, 0.0, kGainTolerance);
        }

        for (const ref::DelayInterpolation interpolation : {ref::DelayInterpolation::Lagrange, ref::DelayInterpolation::AllPass})
        {
            juce::String kind = "Lagrange";
            if (interpolation == ref::DelayInterpolation::AllPass)
            {
                kind = "all-pass";
            }
            beginTest("a fractional delay (" + kind + "): every echo within 0.1 sample and 0.05 dB");
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 1234.37;
            settings.feedback = 0.7;
            settings.mix = 0.3;
            settings.interpolation = interpolation;
            ref::FeedbackDelay delay(kSampleRate, settings);
            const measure::EchoAnalysis analysis = measure::analyseEchoes(renderResponse(delay, getLength(settings)), {});
            expect(analysis.hasDirect);
            expectEchoes(analysis, delay, 15);
            expectWithinAbsoluteError(analysis.spacingSamples, settings.delaySamples, kTimeTolerance);
            expectLessThan(analysis.spacingSpreadSamples, kTimeTolerance);
            expectWithinAbsoluteError(analysis.feedbackDb, toDb(settings.feedback), kGainTolerance);
        }

        beginTest("negative feedback: the echoes alternate in polarity");
        {
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 2000.0;
            settings.feedback = -0.6;
            settings.mix = 1.0;
            ref::FeedbackDelay delay(kSampleRate, settings);
            const measure::EchoAnalysis analysis = measure::analyseEchoes(renderResponse(delay, getLength(settings)), {});
            expect(! analysis.hasDirect, "100 % wet: no direct path");
            expectWithinAbsoluteError(analysis.delaySamples, 2000.0, kTimeTolerance);
            expectEchoes(analysis, delay, 8);
        }

        beginTest("the author's loop order: the first echo at D, the spacing D + 1 (the echo response shows it directly)");
        {
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 4800.0;
            settings.order = ref::LoopOrder::AuthorsOrder;
            ref::FeedbackDelay delay(kSampleRate, settings);
            const measure::EchoAnalysis analysis = measure::analyseEchoes(renderResponse(delay, getLength(settings)), {});
            expectEchoes(analysis, delay, 8);
            expectWithinAbsoluteError(analysis.delaySamples, 4800.0, kTimeTolerance);
            expectWithinAbsoluteError(analysis.spacingSamples - analysis.delaySamples, 1.0, kTimeTolerance);
        }

        beginTest("a latency in front: the direct path is the arrival at the latency, the delay is counted from it");
        {
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 3000.5;
            ref::FeedbackDelay delay(kSampleRate, settings);
            constexpr int kLatency = 64;
            measure::EchoSettings echoSettings;
            echoSettings.latencySamples = kLatency;
            const measure::EchoAnalysis analysis = measure::analyseEchoes(renderResponse(delay, getLength(settings), kLatency), echoSettings);
            expect(analysis.hasDirect);
            expectWithinAbsoluteError(analysis.direct.timeSamples, static_cast<double>(kLatency), kTimeTolerance);
            expectWithinAbsoluteError(analysis.delaySamples, 3000.5, kTimeTolerance);
        }

        beginTest("the loop filter: low and high cut in the loop, the interpolator's mark, within 0.1 dB of the feedback times one trip");
        for (const ref::DelayInterpolation interpolation : {ref::DelayInterpolation::Lagrange, ref::DelayInterpolation::AllPass})
        {
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 4800.5;
            settings.feedback = 0.8;
            settings.mix = 0.5;
            settings.lowCutHz = 200.0;
            settings.highCutHz = 3000.0;
            settings.interpolation = interpolation;
            ref::FeedbackDelay delay(kSampleRate, settings);
            const measure::EchoAnalysis analysis = measure::analyseEchoes(renderResponse(delay, getLength(settings)), {});
            double worst = 0.0;
            int compared = 0;
            for (size_t bin = 0; bin < analysis.loopFrequencies.size(); ++bin)
            {
                const double frequency = analysis.loopFrequencies[bin];
                if (! analysis.loopValid[bin] || frequency < 50.0 || frequency > 15000.0)
                {
                    continue;
                }
                const double expected = toDb(std::abs(settings.feedback) * delay.getLoopFilterMagnitude(frequency));
                worst = std::max(worst, std::abs(analysis.loopMagnitudeDb[bin] - expected));
                ++compared;
            }
            expectGreaterThan(compared, 1000, "the loop filter over most of the band");
            expectLessThan(worst, kLoopTolerance);
        }
    }

private:
    // the first `count` echoes at k D (the author's order: k - 1 more) with gain mix g^(k-1) and its polarity
    void expectEchoes(const measure::EchoAnalysis& analysis, const ref::FeedbackDelay& delay, int count)
    {
        expectGreaterOrEqual(static_cast<int>(analysis.echoes.size()), count, "the echoes found");
        for (int echo = 1; echo <= count && echo <= static_cast<int>(analysis.echoes.size()); ++echo)
        {
            const measure::Arrival& arrival = analysis.echoes[static_cast<size_t>(echo - 1)];
            const double gain = delay.getEchoGain(echo);
            expectWithinAbsoluteError(arrival.timeSamples, delay.getEchoTime(echo), kTimeTolerance, "the time of echo " + juce::String(echo));
            expectWithinAbsoluteError(arrival.gainDb, toDb(std::abs(gain)), kGainTolerance, "the gain of echo " + juce::String(echo));
            expect(arrival.inverted == (gain < 0.0), "the polarity of echo " + juce::String(echo));
        }
    }
};

static EchoResponseTests echoResponseTests;
