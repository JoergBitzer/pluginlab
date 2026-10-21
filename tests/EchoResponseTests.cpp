#include <cmath>
#include <complex>
#include <vector>

#include <juce_core/juce_core.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/ImpulseRender.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/PluginMeasurement.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"
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

// the response of the delay to an impulse of the given height (divided by it), `latency` samples of silence in front (a plugin's latency)
std::vector<double> renderResponse(ref::FeedbackDelay& delay, int samples, int latency = 0, double height = 1.0)
{
    juce::AudioBuffer<float> buffer(1, samples);
    buffer.clear();
    buffer.setSample(0, 0, static_cast<float>(height));
    delay.reset();
    delay.process(buffer);
    std::vector<double> response(static_cast<size_t>(latency), 0.0);
    for (int sample = 0; sample < samples; ++sample)
    {
        response.push_back(buffer.getSample(0, sample) / height);
    }
    return response;
}

// the responses [input][output] of a stereo delay: a unit impulse into one input, the other silent
void renderStereo(ref::FeedbackDelay& delay, int samples, std::vector<double> (&responses)[2][2])
{
    for (int input = 0; input < 2; ++input)
    {
        juce::AudioBuffer<float> buffer(2, samples);
        buffer.clear();
        buffer.setSample(input, 0, 1.0f);
        delay.reset();
        delay.process(buffer);
        for (int output = 0; output < 2; ++output)
        {
            std::vector<double>& response = responses[input][output];
            response.clear();
            for (int sample = 0; sample < samples; ++sample)
            {
                response.push_back(buffer.getSample(output, sample));
            }
        }
    }
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
        : juce::UnitTest("Echo response (W11.1)", "pluginlab")
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

        testStereo();
        testLevels();
        testComparison();
        testReferenceDelayPlugin();
    }

private:
    // W11.1c: the 2 x 2 response (docs/design/W11-delay-plan.md 4.2): the processing against the exact matrix, and what the analysis calls it
    void testStereo()
    {
        beginTest("stereo: the processing is its 2 x 2 transfer function (cross feedback, ping-pong, both orders)");
        for (const bool pingPong : {false, true})
        {
            for (const ref::LoopOrder order : {ref::LoopOrder::Exact, ref::LoopOrder::AuthorsOrder})
            {
                ref::FeedbackDelaySettings settings;
                settings.delaySamples = 41.3;
                settings.feedback = 0.5;
                settings.crossFeedback = -0.3;
                settings.mix = 0.6;
                settings.highCutHz = 8000.0;
                settings.pingPong = pingPong;
                settings.order = order;
                ref::FeedbackDelay delay(kSampleRate, settings);
                std::vector<double> responses[2][2];
                renderStereo(delay, 16384, responses);
                double worst = 0.0;
                for (int input = 0; input < 2; ++input)
                {
                    for (int output = 0; output < 2; ++output)
                    {
                        for (const double frequency : {60.0, 997.0, 5100.0, 14000.0})
                        {
                            std::complex<double> measured;
                            const std::vector<double>& response = responses[input][output];
                            for (size_t sample = 0; sample < response.size(); ++sample)
                            {
                                measured += response[sample] * std::polar(1.0, -2.0 * kPi * frequency * static_cast<double>(sample) / kSampleRate);
                            }
                            worst = std::max(worst, std::abs(measured - delay.getResponse(output, input, frequency)));
                        }
                    }
                }
                expectLessThan(worst, 1.0e-4);
            }
        }

        struct Case
        {
            juce::String name;
            double crossFeedback;
            bool pingPong;
            measure::StereoEchoKind kind;
        };
        for (const Case& item : {Case{"independent channels", 0.0, false, measure::StereoEchoKind::Independent},
                                 Case{"cross feedback", 0.3, false, measure::StereoEchoKind::CrossFeedback},
                                 Case{"ping-pong", 0.0, true, measure::StereoEchoKind::PingPong}})
        {
            beginTest("stereo: " + item.name + " is recognised");
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 3000.25;
            settings.feedback = 0.5;
            settings.crossFeedback = item.crossFeedback;
            settings.pingPong = item.pingPong;
            ref::FeedbackDelay delay(kSampleRate, settings);
            std::vector<double> responses[2][2];
            renderStereo(delay, getLength(settings), responses);
            const measure::StereoEchoAnalysis analysis = measure::analyseStereoEchoes(responses, {});
            expect(analysis.kind == item.kind, "the kind");
            if (! item.pingPong)
            {
                continue;
            }
            // ping-pong: echo k at k D in the left output for odd k, in the right one for even k, gain mix / 2 g^(k-1) (the input is (L + R) / 2)
            expectWithinAbsoluteError(analysis.pingPongSpacingSamples, settings.delaySamples, kTimeTolerance);
            for (int input = 0; input < 2; ++input)
            {
                for (int output = 0; output < 2; ++output)
                {
                    const std::vector<measure::Arrival>& echoes = analysis.paths[input][output].echoes;
                    expectGreaterOrEqual(static_cast<int>(echoes.size()), 4);
                    for (size_t index = 0; index < 4 && index < echoes.size(); ++index)
                    {
                        const int echo = 2 * static_cast<int>(index) + 1 + output;
                        expectWithinAbsoluteError(echoes[index].timeSamples, echo * settings.delaySamples, kTimeTolerance);
                        expectWithinAbsoluteError(echoes[index].gainDb, toDb(0.5 * delay.getEchoGain(echo)), kGainTolerance);
                    }
                }
            }
        }
    }

    // W11.1e: the echo response at three levels (docs/design/W11-delay-plan.md 4.4): the saturation in the loop, self-oscillation
    void testLevels()
    {
        const std::vector<double> levels = {-40.0, -20.0, -6.0};
        for (const double drive : {0.0, 2.0})
        {
            beginTest("level dependence, drive " + juce::String(drive, 0) + ": every echo at -40, -20, -6 dBFS as the exact iteration a(k+1) = s(g a(k))");
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 2400.0;
            settings.feedback = 0.6;
            settings.saturationDrive = drive;
            ref::FeedbackDelay delay(kSampleRate, settings);
            std::vector<measure::EchoAnalysis> analyses;
            for (const double level : levels)
            {
                const double height = std::pow(10.0, level / 20.0);
                analyses.push_back(measure::analyseEchoes(renderResponse(delay, getLength(settings), 0, height), {}));
                const std::vector<double> expected = delay.getSaturatedEchoes(height, 5);
                expectGreaterOrEqual(static_cast<int>(analyses.back().echoes.size()), 5);
                for (size_t echo = 0; echo < 5 && echo < analyses.back().echoes.size(); ++echo)
                {
                    expectWithinAbsoluteError(analyses.back().echoes[echo].gainDb, toDb(expected[echo] / height), kGainTolerance,
                                              "echo " + juce::String(static_cast<int>(echo) + 1) + " at " + juce::String(level, 0) + " dBFS");
                }
            }
            const measure::LevelDependence dependence = measure::analyseLevelDependence(levels, analyses);
            logMessage("compression " + juce::String(dependence.compressionDb, 3) + " dB, feedback change " + juce::String(dependence.feedbackChangeDb, 3) + " dB");
            expect(dependence.saturates == (drive > 0.0), "saturation found exactly when there is one");
            expect(! dependence.selfOscillating);
        }

        beginTest("self-oscillation: feedback 120 % held by the saturation does not decay and is reported");
        {
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = 2400.0;
            settings.feedback = 1.2;
            settings.saturationDrive = 2.0;
            ref::FeedbackDelay delay(kSampleRate, settings);
            const double height = std::pow(10.0, -20.0 / 20.0);
            const std::vector<measure::EchoAnalysis> analyses = {measure::analyseEchoes(renderResponse(delay, 2400 * 40, 0, height), {})};
            const measure::LevelDependence dependence = measure::analyseLevelDependence({-20.0}, analyses);
            expect(dependence.selfOscillating, "self-oscillating");
        }
    }

    // W11.1g: two delays compared echo by echo: B is 1.5 samples longer, has more feedback and a low-pass in the loop
    void testComparison()
    {
        beginTest("two delays compared echo by echo: the time and gain differences (the same loop filter), the loop filters' difference");
        // B is 1.5 samples longer and has more feedback; the same low-pass in both loops, so the times differ by 1.5 samples per trip only
        // (a low-pass of its own would add its group delay to every trip)
        ref::FeedbackDelaySettings settingsA;
        settingsA.delaySamples = 4800.0;
        settingsA.feedback = 0.5;
        settingsA.highCutHz = 3000.0;
        ref::FeedbackDelaySettings settingsB = settingsA;
        settingsB.delaySamples = 4801.5;
        settingsB.feedback = 0.6;
        ref::FeedbackDelay delayA(kSampleRate, settingsA);
        ref::FeedbackDelay delayB(kSampleRate, settingsB);
        const measure::EchoAnalysis a = measure::analyseEchoes(renderResponse(delayA, getLength(settingsB)), {});
        const measure::EchoAnalysis b = measure::analyseEchoes(renderResponse(delayB, getLength(settingsB)), {});
        const measure::EchoComparison comparison = measure::compareEchoes(a, b);
        expectGreaterOrEqual(static_cast<int>(comparison.pairs.size()), 5);
        for (size_t index = 0; index < 3 && index < comparison.pairs.size(); ++index)
        {
            const int echo = static_cast<int>(index) + 1;
            expectWithinAbsoluteError(comparison.pairs[index].timeB - comparison.pairs[index].timeA, 1.5 * echo, kTimeTolerance, "the time of echo " + juce::String(echo));
            expectWithinAbsoluteError(comparison.pairs[index].gainDbB - comparison.pairs[index].gainDbA, (echo - 1) * toDb(0.6 / 0.5), kGainTolerance,
                                      "the gain of echo " + juce::String(echo));
        }
        expectWithinAbsoluteError(comparison.delayDifferenceSamples, 1.5, kTimeTolerance);

        // C: A with a lower cut-off; the loop filters' difference is 20 log10(|H_C| / |H_A|)
        ref::FeedbackDelaySettings settingsC = settingsA;
        settingsC.highCutHz = 1500.0;
        ref::FeedbackDelay delayC(kSampleRate, settingsC);
        const measure::EchoComparison filters = measure::compareEchoes(a, measure::analyseEchoes(renderResponse(delayC, getLength(settingsA)), {}));
        double worst = 0.0;
        for (size_t bin = 0; bin < filters.loopFrequencies.size(); ++bin)
        {
            const double frequency = filters.loopFrequencies[bin];
            if (! filters.loopValid[bin] || frequency < 100.0 || frequency > 10000.0)
            {
                continue;
            }
            const double expected = toDb(delayC.getLoopFilterMagnitude(frequency)) - toDb(delayA.getLoopFilterMagnitude(frequency));
            worst = std::max(worst, std::abs(filters.loopDifferenceDb[bin] - expected));
        }
        expectLessThan(worst, 0.2, "the loop filters' difference");
    }

    // W11.1b: the Reference Delay plugin through the measurement rig (renderImpulse with the play head's tempo): its echoes as the processor's
    void testReferenceDelayPlugin()
    {
        constexpr size_t kSync = 1;
        constexpr float kSyncQuarter = 0.2f; // "1/4" of six choices
        constexpr double kOddTempo = 97.0;
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getReferencePlugin("Delay"));
        expect(! scan.descriptions.isEmpty(), "the Reference Delay plugin");
        if (scan.descriptions.isEmpty())
        {
            return;
        }
        const juce::PluginDescription description = scan.descriptions[0];
        std::vector<float> setting = pluginlab::engine::getDefaultSetting(formatManager, description, kSampleRate);

        struct Case
        {
            juce::String name;
            double tempo;
            float sync;
            double delaySamples;
        };
        for (const Case& item : {Case{"the defaults (250 ms, feedback 40 %, mix 50 %)", 120.0, 0.0f, 12000.0},
                                 Case{"synced to a quarter note at 97 BPM", kOddTempo, kSyncQuarter, 60.0 / kOddTempo * kSampleRate}})
        {
            beginTest("the Reference Delay plugin, " + item.name + ": the echoes of its reference processor");
            setting[kSync] = item.sync;
            pluginlab::engine::ImpulseRenderOptions options;
            options.tempoBpm = item.tempo;
            const pluginlab::engine::ImpulseRender render = pluginlab::engine::renderImpulse(formatManager, description, setting, options);
            expect(render.error.isEmpty() && render.decayed, render.error);
            if (render.output.empty())
            {
                continue;
            }
            const double height = std::pow(10.0, options.levelDbfs / 20.0);
            std::vector<double> response;
            for (const float value : render.output.front())
            {
                response.push_back(value / height);
            }
            ref::FeedbackDelaySettings settings;
            settings.delaySamples = item.delaySamples;
            settings.feedback = 0.4;
            settings.mix = 0.5;
            const ref::FeedbackDelay expected(kSampleRate, settings);
            const measure::EchoAnalysis analysis = measure::analyseEchoes(response, {});
            expect(analysis.hasDirect, "the dry path");
            expectWithinAbsoluteError(analysis.direct.gainDb, toDb(0.5), kGainTolerance);
            expectEchoes(analysis, expected, 6); // the later echoes of a fractional delay are spread by the interpolator, their peaks below -60 dB
            expectWithinAbsoluteError(analysis.feedbackDb, toDb(0.4), kGainTolerance);
        }

        beginTest("the Reference Delay plugin with its saturation (W11.1e): at -40, -20, -6 dBFS through the rig the echo gains fall with the level");
        {
            constexpr size_t kSaturation = 14;
            for (const float drive : {0.0f, 0.4f}) // 0 and 2 of 0 ... 5
            {
                setting[kSync] = 0.0f;
                setting[kSaturation] = drive;
                const std::vector<double> levels = {-40.0, -20.0, -6.0};
                std::vector<measure::EchoAnalysis> analyses;
                for (const double level : levels)
                {
                    pluginlab::engine::ImpulseRenderOptions options;
                    options.levelDbfs = level;
                    const pluginlab::engine::ImpulseRender render = pluginlab::engine::renderImpulse(formatManager, description, setting, options);
                    std::vector<double> response;
                    for (const float value : render.output.front())
                    {
                        response.push_back(value / std::pow(10.0, level / 20.0));
                    }
                    analyses.push_back(measure::analyseEchoes(response, {}));
                }
                const measure::LevelDependence dependence = measure::analyseLevelDependence(levels, analyses);
                expect(dependence.saturates == (drive > 0.0f), "saturation found exactly when there is one");
            }
            setting[kSaturation] = 0.0f;
        }

        beginTest("the unit 'echoes' of the reports (W11.1f): the Reference Delay with ping-pong, LFO and saturation; no echoes for EQs and latencies");
        {
            constexpr size_t kPingPong = 10;
            constexpr size_t kLfoDepth = 12;
            constexpr size_t kSaturation = 14;
            constexpr size_t kFeedback = 2;
            std::vector<float> delaySetting = setting;
            delaySetting[kSync] = 0.0f;
            delaySetting[kPingPong] = 1.0f;
            delaySetting[kLfoDepth] = 0.05f;    // 0.5 ms
            delaySetting[kSaturation] = 0.4f;   // drive 2
            delaySetting[kFeedback] = 0.75f;    // 47.5 %
            pluginlab::engine::MeasurementOptions options;
            options.units = {"echoes"};
            const pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measurePlugin(formatManager, description, delaySetting, options);
            for (const pluginlab::engine::MeasurementRow& row : summary.rows)
            {
                logMessage(row.quantity + ": " + row.text);
            }
            const pluginlab::engine::MeasurementRow* stereo = pluginlab::engine::findRow(summary, "stereo (one input at a time)");
            expect(stereo != nullptr && stereo->text.startsWith("ping-pong"), "ping-pong in the report");
            const pluginlab::engine::MeasurementRow* levels = pluginlab::engine::findRow(summary, "level dependence of the echoes");
            expect(levels != nullptr && levels->text.contains("saturation"), "the saturation in the report");
            const pluginlab::engine::MeasurementRow* modulation = pluginlab::engine::findRow(summary, "delay modulation (impulse train)");
            expect(modulation != nullptr && modulation->text.startsWith("sine, 0.50 Hz"), "the LFO in the report");

            for (const juce::File& file : {testpaths::getReferencePlugin("Eq"), testpaths::getLinearPhasePlugin(), testpaths::getLatencyLiarPlugin()})
            {
                const pluginlab::hosting::PluginScanResult other = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, file);
                expect(! other.descriptions.isEmpty(), file.getFileName());
                if (other.descriptions.isEmpty())
                {
                    continue;
                }
                const pluginlab::engine::MeasurementSummary none = pluginlab::engine::measurePlugin(formatManager, other.descriptions[0], {}, options);
                const pluginlab::engine::MeasurementRow* row = pluginlab::engine::findRow(none, "echoes after an impulse (channel 1)");
                expect(row != nullptr && row->text.startsWith("none"), file.getFileName() + ": no echoes");
            }
        }

        beginTest("the echo comparison report (W11.1g): the Reference Delay at two settings");
        {
            constexpr size_t kTime = 0;
            constexpr size_t kFeedback = 2;
            constexpr size_t kLoopFilter = 4;
            std::vector<float> settingA = setting;
            settingA[kSync] = 0.0f;
            std::vector<float> settingB = settingA;
            settingB[kTime] = settingA[kTime] + 0.0025f; // 5 ms longer (1 ... 2000 ms)
            settingB[kFeedback] = 0.75f;                 // 47.5 %
            settingB[kLoopFilter] = 1.0f;
            const juce::String report = pluginlab::engine::compareEchoResponses(formatManager, description, settingA, description, settingB);
            logMessage(report);
            for (const juce::String expected : {"# Echo comparison", "| delay time |", "## The echoes", "| 1 |", "## The loop filter", "| 1000 Hz |"})
            {
                expect(report.contains(expected), "the report contains " + expected);
            }
        }

        beginTest("the Reference Delay plugin with ping-pong (W11.1c): the 2 x 2 response through the rig is recognised as ping-pong");
        {
            constexpr size_t kPingPong = 10;
            setting[kSync] = 0.0f;
            setting[kPingPong] = 1.0f;
            const double height = std::pow(10.0, -6.0 / 20.0);
            std::vector<double> responses[2][2];
            for (int input = 0; input < 2; ++input)
            {
                pluginlab::engine::ImpulseRenderOptions options;
                options.impulseChannel = input;
                const pluginlab::engine::ImpulseRender render = pluginlab::engine::renderImpulse(formatManager, description, setting, options);
                expect(render.channels == 2 && render.decayed, render.error);
                for (int output = 0; output < 2 && output < render.channels; ++output)
                {
                    for (const float value : render.output[static_cast<size_t>(output)])
                    {
                        responses[input][output].push_back(value / height);
                    }
                }
            }
            const measure::StereoEchoAnalysis analysis = measure::analyseStereoEchoes(responses, {});
            expect(analysis.kind == measure::StereoEchoKind::PingPong, "ping-pong");
            expectWithinAbsoluteError(analysis.pingPongSpacingSamples, 12000.0, kTimeTolerance);
        }
    }

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
