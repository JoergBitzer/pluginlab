#include <cmath>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "MeasureTestDevices.h"
#include "pluginlab/measure/Crosstalk.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kSampleRate = 48000.0;

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}
}

// W7.8: inter-channel crosstalk (AES17-2015 6.5.2) against channel matrices with known crosstalk (docs/measurements/crosstalk.md)
class MeasureCrosstalkTests : public juce::UnitTest
{
public:
    MeasureCrosstalkTests()
        : juce::UnitTest("Measure: crosstalk", "pluginlab")
    {
    }

    void runTest() override
    {
        testMatrices();
        testFilteredAndNoisy();
        logMessage("results for docs/measurements/crosstalk.md:\n" + m_lines.joinIntoString("\n"));
    }

private:
    void testMatrices()
    {
        beginTest("channel matrices: constant crosstalk, stereo width and independent channels equal their coefficients at every frequency");
        struct Case
        {
            juce::String name;
            ref::ChannelMatrix matrix;
        };
        const std::vector<Case> cases = {
            {"crosstalk -40 dB", ref::ChannelMatrix::makeCrosstalk(-40.0)},
            {"crosstalk -90 dB", ref::ChannelMatrix::makeCrosstalk(-90.0)},
            {"width 1.5 (wider)", ref::ChannelMatrix::makeWidth(1.5)},
            {"width 0.5 (narrower)", ref::ChannelMatrix::makeWidth(0.5)},
            {"width 0 (mono)", ref::ChannelMatrix::makeWidth(0.0)},
            {"asymmetric: L' = L, R' = R + 0.01 L", ref::ChannelMatrix(1.0, 0.0, 0.01, 1.0)},
            {"independent (identity)", ref::ChannelMatrix(1.0, 0.0, 0.0, 1.0)},
        };
        m_lines.add("| device | expected L -> R | measured L -> R (20 Hz ... 20 kHz) | expected R -> L | measured R -> L |\n|---|---|---|---|---|");
        for (const Case& item : cases)
        {
            const ref::ChannelMatrix matrix = item.matrix;
            const measure::CrosstalkResult result = measure::measureCrosstalk(
                measure::makeProcessorDevice([matrix](double) { return std::make_unique<ref::ChannelMatrix>(matrix); }), measure::CrosstalkSettings());
            // driven L: L' = ll, R' = rl; driven R: L' = lr, R' = rr
            const double leftToRight = toDb(std::abs(matrix.getRightFromLeft()) / std::abs(matrix.getLeftFromLeft()));
            const double rightToLeft = toDb(std::abs(matrix.getLeftFromRight()) / std::abs(matrix.getRightFromRight()));
            const auto range = [](const std::vector<double>& values)
            {
                double low = values.front();
                double high = values.front();
                for (const double value : values)
                {
                    low = std::min(low, value);
                    high = std::max(high, value);
                }
                return std::make_pair(low, high);
            };
            const auto [lrLow, lrHigh] = range(result.selectiveDb[0][1]);
            const auto [rlLow, rlHigh] = range(result.selectiveDb[1][0]);
            for (size_t index = 0; index < result.frequencyHz.size(); ++index)
            {
                expectCrosstalk(result.selectiveDb[0][1][index], leftToRight, item.name + " L -> R at " + juce::String(result.frequencyHz[index]));
                expectCrosstalk(result.selectiveDb[1][0][index], rightToLeft, item.name + " R -> L at " + juce::String(result.frequencyHz[index]));
            }
            m_lines.add("| " + item.name + " | " + formatDb(leftToRight) + " | " + formatRange(lrLow, lrHigh) + " | " + formatDb(rightToLeft) + " | " + formatRange(rlLow, rlHigh) + " |");
        }
    }

    void testFilteredAndNoisy()
    {
        beginTest("crosstalk rising with frequency (first-order high-pass into the other channel) follows k |H(f)|; noise shows in the broadband level only");
        const double factor = std::pow(10.0, -40.0 / 20.0);
        const std::vector<ref::BiquadCoefficients> highPass = ref::designButterworth(ref::Pass::High, 1, kSampleRate, 10000.0);
        const measure::CrosstalkResult filtered = measure::measureCrosstalk(
            measure::makeProcessorDevice([factor, highPass](double rate) { return std::make_unique<pluginlab::test::FilteredCrosstalk>(factor, highPass, rate); }),
            measure::CrosstalkSettings());
        juce::String line = "\nfiltered crosstalk (-40 dB through a first-order high-pass at 10 kHz), L -> R, expected / measured (dB):";
        for (size_t index = 0; index < filtered.frequencyHz.size(); ++index)
        {
            const double f = filtered.frequencyHz[index];
            // the driven channel keeps its signal (1) plus nothing from the silent channel
            const double expected = toDb(factor * std::abs(ref::getCascadeResponse(highPass, f, kSampleRate)));
            expectCrosstalk(filtered.selectiveDb[0][1][index], expected, "filtered at " + juce::String(f));
            line << " " << juce::String(f, 0) << " Hz " << formatDb(expected) << " / " << formatDb(filtered.selectiveDb[0][1][index]) << ";";
        }
        m_lines.add(line);

        // independent channels with noise at -100 dB rms: the tone does not leak, the broadband level of the undriven channel is the noise
        const measure::CrosstalkResult noisy = measure::measureCrosstalk(
            measure::makeProcessorDevice([](double) { return std::make_unique<ref::NoiseAdder>(pluginlab::signals::NoiseColour::WhiteGaussian, -100.0, 3); }),
            measure::CrosstalkSettings());
        const double toneRms = std::pow(10.0, -20.0 / 20.0) / std::sqrt(2.0);
        const double expectedBroadband = toDb(std::pow(10.0, -100.0 / 20.0) / toneRms);
        double worstSelective = -1000.0;
        for (size_t index = 0; index < noisy.frequencyHz.size(); ++index)
        {
            expectWithinAbsoluteError(noisy.broadbandDb[0][1][index], expectedBroadband, 0.3, "noise, broadband at " + juce::String(noisy.frequencyHz[index]));
            worstSelective = std::max(worstSelective, noisy.selectiveDb[0][1][index]);
        }
        expect(worstSelective < expectedBroadband - 15.0, "the selective level rejects most of the noise: " + juce::String(worstSelective));
        m_lines.add("independent channels with white noise at -100 dB rms (sample rms): broadband " + formatDb(noisy.broadbandDb[0][1][5]) + " dB at "
                    + juce::String(noisy.frequencyHz[5], 0) + " Hz (expected " + formatDb(expectedBroadband) + "), selective at most " + formatDb(worstSelective) + " dB");
    }

    void expectCrosstalk(double measured, double expected, const juce::String& label)
    {
        if (expected < -200.0)
        {
            expect(measured < -200.0, label + ": " + juce::String(measured));
            return;
        }
        expectWithinAbsoluteError(measured, expected, 0.001, label);
    }

    static juce::String formatDb(double value)
    {
        if (value < -200.0)
        {
            return "none";
        }
        return juce::String(value, 3);
    }

    static juce::String formatRange(double low, double high)
    {
        if (high < -200.0)
        {
            return "none (below -200)";
        }
        return formatDb(low) + " ... " + formatDb(high);
    }

    juce::StringArray m_lines;
};

static MeasureCrosstalkTests measureCrosstalkTests;
