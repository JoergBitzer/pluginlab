#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Loudness.h"

namespace
{
namespace measure = pluginlab::measure;

constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kLoudnessTolerance = 0.1;  // LU: EBU Tech 3341's tolerance for the integrated loudness
constexpr double kRangeTolerance = 1.0;     // LU: EBU Tech 3342's tolerance for the loudness range

// a stereo sine of 1 kHz, the same in both channels, in segments of (level in dBFS, seconds)
std::vector<std::vector<double>> makeSegments(const std::vector<std::pair<double, double>>& segments, double sampleRate)
{
    std::vector<double> signal;
    for (const auto& [levelDb, seconds] : segments)
    {
        const double amplitude = std::pow(10.0, levelDb / 20.0);
        const int count = static_cast<int>(std::lround(seconds * sampleRate));
        for (int sample = 0; sample < count; ++sample)
        {
            signal.push_back(amplitude * std::sin(2.0 * kPi * 1000.0 * static_cast<double>(signal.size()) / sampleRate));
        }
    }
    return {signal, signal};
}
}

// W11.3f: the loudness of ITU-R BS.1770-5 and EBU Tech 3342 (docs/design/W11-dynamics-plan.md 4.8), checked against the cases of EBU Tech
// 3341 and 3342 (their full lengths: shorter segments put more blocks across the steps, which pass the gates) and the filter coefficients
// the standard prints for 48 kHz
class LoudnessTests : public juce::UnitTest
{
public:
    LoudnessTests()
        : juce::UnitTest("Loudness (W11.3f)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the K-weighting at 48 kHz: the impulse response of BS.1770's two biquads");
        {
            // BS.1770-5, table 1 and 2: the pre-filter b0, b1, b2, a1, a2 and the RLB filter's a1, a2
            const double b[3] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
            const double a[2] = {-1.69065929318241, 0.73248077421585};
            const double rlb[2] = {-1.99004745483398, 0.99007225036621};
            std::vector<double> impulse(64, 0.0);
            impulse[0] = 1.0;
            std::vector<double> expected(64, 0.0);
            {
                double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
                for (size_t n = 0; n < expected.size(); ++n)
                {
                    const double y = b[0] * impulse[n] + b[1] * x1 + b[2] * x2 - a[0] * y1 - a[1] * y2;
                    x2 = x1;
                    x1 = impulse[n];
                    y2 = y1;
                    y1 = y;
                    expected[n] = y;
                }
                x1 = x2 = y1 = y2 = 0.0;
                for (double& value : expected)
                {
                    const double y = value - 2.0 * x1 + x2 - rlb[0] * y1 - rlb[1] * y2;
                    x2 = x1;
                    x1 = value;
                    y2 = y1;
                    y1 = y;
                    value = y;
                }
            }
            const std::vector<double> weighted = measure::applyKWeighting(impulse, kSampleRate);
            double worst = 0.0;
            for (size_t n = 0; n < expected.size(); ++n)
            {
                worst = std::max(worst, std::abs(weighted[n] - expected[n]));
            }
            logMessage("the largest difference: " + juce::String(worst, 12));
            expectLessThan(worst, 1.0e-6, "the impulse response");
        }

        for (const double rate : {44100.0, 48000.0, 96000.0})
        {
            beginTest("a stereo sine of 1 kHz at -23 dBFS reads -23.0 LUFS (Tech 3341 case 1) at " + juce::String(rate / 1000.0, 1) + " kHz");
            const measure::LoudnessResult result = measure::measureLoudness(makeSegments({{-23.0, 5.0}}, rate), rate);
            logMessage(juce::String(result.integratedLufs, 3) + " LUFS");
            expectWithinAbsoluteError(result.integratedLufs, -23.0, kLoudnessTolerance);
        }

        beginTest("the gates: -72, -36, -23, -36, -72 dBFS (Tech 3341 case 4) read -23.0 LUFS");
        {
            const measure::LoudnessResult result
                = measure::measureLoudness(makeSegments({{-72.0, 10.0}, {-36.0, 10.0}, {-23.0, 60.0}, {-36.0, 10.0}, {-72.0, 10.0}}, kSampleRate), kSampleRate);
            logMessage(juce::String(result.integratedLufs, 3) + " LUFS");
            expectWithinAbsoluteError(result.integratedLufs, -23.0, kLoudnessTolerance);
        }

        struct RangeCase
        {
            double low;
            double high;
            double expected;
        };
        for (const RangeCase& item : {RangeCase{-20.0, -30.0, 10.0}, RangeCase{-20.0, -15.0, 5.0}})
        {
            beginTest("the loudness range: " + juce::String(item.low, 0) + " then " + juce::String(item.high, 0) + " dBFS read " + juce::String(item.expected, 0)
                      + " LU (Tech 3342)");
            const measure::LoudnessResult result = measure::measureLoudness(makeSegments({{item.low, 20.0}, {item.high, 20.0}}, kSampleRate), kSampleRate);
            logMessage(juce::String(result.loudnessRangeLu, 3) + " LU");
            expectWithinAbsoluteError(result.loudnessRangeLu, item.expected, kRangeTolerance);
        }

        beginTest("the crest factor of a sine is 3.01 dB, its sample peak its amplitude");
        {
            const measure::LoudnessResult result = measure::measureLoudness(makeSegments({{-6.0, 1.0}}, kSampleRate), kSampleRate);
            expectWithinAbsoluteError(result.crestFactorDb, 10.0 * std::log10(2.0), 0.01, "the crest factor");
            expectWithinAbsoluteError(result.samplePeakDbfs, -6.0, 0.01, "the sample peak");
            expectWithinAbsoluteError(result.truePeakDbfs, -6.0, 0.05, "the true peak");
        }

        beginTest("the gain curve: a delayed copy at half the amplitude is -6.02 dB in every window above the floor");
        {
            juce::Random random(1);
            constexpr int kLag = 37;
            std::vector<double> input(static_cast<size_t>(kSampleRate), 0.0);
            for (size_t sample = 0; sample < input.size() / 2; ++sample)
            {
                input[sample] = random.nextDouble() * 2.0 - 1.0;
            }
            std::vector<double> output(input.size() + kLag, 0.0);
            for (size_t sample = 0; sample < input.size(); ++sample)
            {
                output[sample + kLag] = 0.5 * input[sample];
            }
            const measure::GainCurve curve = measure::getGainCurve(input, output, kLag, kSampleRate);
            int valid = 0;
            int silent = 0;
            for (const double gain : curve.gainDb)
            {
                if (std::isnan(gain))
                {
                    ++silent;
                    continue;
                }
                ++valid;
                expectWithinAbsoluteError(gain, 20.0 * std::log10(0.5), 1.0e-9);
            }
            expectEquals(valid, 50, "the windows with signal");
            expectEquals(silent, 50, "the silent windows");
        }
    }
};

static LoudnessTests loudnessTests;
