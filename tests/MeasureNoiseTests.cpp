#include <cmath>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "MeasureTestDevices.h"
#include "pluginlab/measure/Noise.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;
using measure::Weighting;

constexpr double kSampleRate = 48000.0;
const std::vector<Weighting> kWeightings = {Weighting::Unweighted, Weighting::Band, Weighting::CcirRms, Weighting::A};

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}

// The power gain of a weighting (times an optional notch) for white noise: the mean of |W(f)|^2 over 0 ... fs/2, by the trapezoidal rule on a
// 0.25 Hz grid (independent of the unit's bin sum)
double getWhiteNoiseShare(Weighting weighting, const ref::BiquadCoefficients* notch = nullptr)
{
    const double step = 0.25;
    const int points = static_cast<int>(0.5 * kSampleRate / step);
    double sum = 0.0;
    for (int index = 0; index <= points; ++index)
    {
        const double f = index * step;
        double gain = measure::getWeightingGain(weighting, f);
        if (notch != nullptr)
        {
            gain *= std::abs(ref::getBiquadResponse(*notch, f, kSampleRate));
        }
        double weight = 1.0;
        if (index == 0 || index == points)
        {
            weight = 0.5;
        }
        sum += weight * gain * gain;
    }
    return sum / points;
}

juce::String getWeightingName(Weighting weighting)
{
    switch (weighting)
    {
        case Weighting::Unweighted:
            return "unweighted";
        case Weighting::Band:
            return "20 Hz ... 20 kHz";
        case Weighting::CcirRms:
            return "CCIR-RMS";
        case Weighting::A:
            return "A";
    }
    return {};
}

measure::Device makeDevice(std::function<std::unique_ptr<ref::Processor>(double)> factory)
{
    return measure::makeProcessorDevice(std::move(factory));
}
}

// W7.7: noise (AES17-2015 6.4.1, 6.4.2, 6.5.1; weightings 5.2.7 and IEC 61672-1) against devices with known noise and hum (docs/measurements/noise.md)
class MeasureNoiseTests : public juce::UnitTest
{
public:
    MeasureNoiseTests()
        : juce::UnitTest("Measure: noise", "pluginlab")
    {
    }

    void runTest() override
    {
        testWeightings();
        testIdleNoise();
        testDynamicRange();
        testMains();
        logMessage("results for docs/measurements/noise.md:\n" + m_lines.joinIntoString("\n"));
    }

private:
    void testWeightings()
    {
        beginTest("CCIR-RMS within the tolerances of AES17 table 1, A-weighting at the nominal values of IEC 61672-1");
        struct Point
        {
            double frequencyHz;
            double gainDb;
            double toleranceDb;
        };
        // AES17-2015 table 1. At 6.3 kHz the table gives 6.6 +- 0.01 dB: BS.468's +12.2 dB minus 5.63 dB is 6.57, rounded to 6.6; the curve gives 6.587.
        const std::vector<Point> ccir = {{31.5, -35.5, 2.0}, {63, -29.5, 1.4}, {100, -25.4, 1.0}, {200, -19.4, 0.85}, {400, -13.4, 0.7}, {800, -7.5, 0.55},
                                         {1000, -5.6, 0.5}, {2000, 0.0, 0.5}, {3150, 3.4, 0.5}, {4000, 4.9, 0.5}, {5000, 6.1, 0.5}, {6300, 6.57, 0.02},
                                         {7100, 6.4, 0.2}, {8000, 5.8, 0.4}, {9000, 4.5, 0.6}, {10000, 2.5, 0.8}, {12500, -5.6, 1.2}, {14000, -10.9, 1.4},
                                         {16000, -17.3, 1.6}, {20000, -27.8, 2.0}, {31500, -48.3, 2.8}};
        double largest = 0.0;
        juce::String line = "CCIR-RMS (table / curve):";
        for (const Point& point : ccir)
        {
            const double value = toDb(measure::getWeightingGain(Weighting::CcirRms, point.frequencyHz));
            expectWithinAbsoluteError(value, point.gainDb, point.toleranceDb, "CCIR-RMS at " + juce::String(point.frequencyHz));
            largest = std::max(largest, std::abs(value - point.gainDb));
            line << " " << juce::String(point.frequencyHz) << " Hz " << juce::String(point.gainDb, 2) << "/" << juce::String(value, 3) << ";";
        }
        m_lines.add(line + " largest difference " + juce::String(largest, 3) + " dB");
        // IEC 61672-1 table 3: nominal A-weighting at the exact base-10 frequencies 1000 x 10^(n/10) (rounded to 0.1 dB)
        const std::vector<std::pair<int, double>> a = {{-15, -39.4}, {-12, -26.2}, {-9, -16.1}, {-6, -8.6}, {-3, -3.2}, {0, 0.0}, {3, 1.2}, {6, 1.0}, {9, -1.1}, {12, -6.6}};
        line = "A (table / curve):";
        largest = 0.0;
        for (const auto& [exponent, gain] : a)
        {
            const double f = 1000.0 * std::pow(10.0, exponent / 10.0);
            const double value = toDb(measure::getWeightingGain(Weighting::A, f));
            expectWithinAbsoluteError(value, gain, 0.05, "A at " + juce::String(f));
            largest = std::max(largest, std::abs(value - gain));
            line << " " << juce::String(f, 1) << " Hz " << juce::String(gain, 1) << "/" << juce::String(value, 3) << ";";
        }
        m_lines.add(line + " largest difference " + juce::String(largest, 3) + " dB");
        juce::String shares = "white-noise power share (dB):";
        for (const Weighting weighting : kWeightings)
        {
            shares << " " << getWeightingName(weighting) << " " << juce::String(10.0 * std::log10(getWhiteNoiseShare(weighting)), 3) << ";";
        }
        m_lines.add(shares);
    }

    void testIdleNoise()
    {
        beginTest("idle channel noise (6.4.2): dithered quantizers and added white noise equal their known level in every weighting");
        struct Case
        {
            juce::String name;
            std::function<std::unique_ptr<ref::Processor>(double)> factory;
            double rms;                          // the known rms of the white noise (0: none)
        };
        const double step16 = std::pow(2.0, -15.0);
        const double step24 = std::pow(2.0, -23.0);
        const std::vector<Case> cases = {
            {"16-bit quantizer, TPDF dither", [](double) { return std::make_unique<ref::Quantizer>(16, true); }, step16 / 2.0},
            {"24-bit quantizer, TPDF dither", [](double) { return std::make_unique<ref::Quantizer>(24, true); }, step24 / 2.0},
            {"white Gaussian noise, -80 dB rms", [](double) { return std::make_unique<ref::NoiseAdder>(pluginlab::signals::NoiseColour::WhiteGaussian, -80.0, 9); },
             std::pow(10.0, -80.0 / 20.0)},
        };
        m_lines.add("\nidle channel noise, 48 kHz, dBFS (AES17: re a full-scale sine), expected / measured");
        m_lines.add("| device | unweighted | 20 Hz ... 20 kHz | CCIR-RMS | A |\n|---|---|---|---|---|");
        for (const Case& item : cases)
        {
            measure::NoiseSettings settings;
            settings.channels = 1;
            const measure::IdleNoiseResult result = measure::measureIdleNoise(makeDevice(item.factory), settings);
            juce::String line = "| " + item.name + " |";
            for (const Weighting weighting : kWeightings)
            {
                const double expected = measure::rmsToDbfs(item.rms * std::sqrt(getWhiteNoiseShare(weighting)));
                const double measured = result.channels[0].get(weighting);
                expectWithinAbsoluteError(measured, expected, 0.15, item.name + ", " + getWeightingName(weighting));
                line << " " << juce::String(expected, 2) << " / " << juce::String(measured, 2) << " |";
            }
            m_lines.add(line);
        }

        // without dither a quantizer is silent at idle; a DC offset counts only unweighted (the standard low-pass passes DC)
        measure::NoiseSettings settings;
        settings.channels = 1;
        const measure::IdleNoiseResult silent = measure::measureIdleNoise(makeDevice([](double) { return std::make_unique<ref::Quantizer>(16, false); }), settings);
        expect(silent.channels[0].unweightedDbfs < -300.0, "undithered quantizer at idle");
        const measure::IdleNoiseResult offset = measure::measureIdleNoise(makeDevice([](double) { return std::make_unique<ref::DcOffset>(0.001); }), settings);
        expectWithinAbsoluteError(offset.channels[0].unweightedDbfs, measure::rmsToDbfs(0.001), 0.001, "DC offset, unweighted");
        expect(offset.channels[0].ccirRmsDbfs < -200.0 && offset.channels[0].aWeightedDbfs < -200.0 && offset.channels[0].bandDbfs < -200.0, "DC offset, weighted");
        m_lines.add("| 16-bit quantizer, no dither | silent | | | below -300 |");
        m_lines.add("| DC offset 0.001 | " + juce::String(measure::rmsToDbfs(0.001), 2) + " / " + juce::String(offset.channels[0].unweightedDbfs, 2) + " | - | - | - |");
    }

    void testDynamicRange()
    {
        beginTest("dynamic range (6.4.1): -60 dBFS at 997 Hz, notched and weighted; dithered quantizers exact, undithered near 6.02 N + 1.76 dB");
        m_lines.add("\ndynamic range, 48 kHz, 997 Hz (996.83 Hz coherent) at -60 dBFS, dB re 0 dBFS, expected / measured");
        m_lines.add("| device | unweighted | 20 Hz ... 20 kHz | dB CCIR-RMS | dB(A) |\n|---|---|---|---|---|");
        measure::NoiseSettings settings;
        settings.channels = 1;
        for (const int bits : {16, 24})
        {
            const measure::DynamicRangeResult result = measure::measureDynamicRange(makeDevice([bits](double) { return std::make_unique<ref::Quantizer>(bits, true); }),
                                                                                    settings);
            const ref::BiquadCoefficients notch = ref::designRbj(ref::FilterType::Notch, kSampleRate, result.frequencyHz, 0.0, settings.notchQ);
            const double rms = std::pow(2.0, 1 - bits) / 2.0;
            juce::String line = "| " + juce::String(bits) + "-bit quantizer, TPDF dither |";
            for (const Weighting weighting : kWeightings)
            {
                const double expected = -measure::rmsToDbfs(rms * std::sqrt(getWhiteNoiseShare(weighting, &notch)));
                const double measured = result.channels[0].dynamicRange.get(weighting);
                expectWithinAbsoluteError(measured, expected, 0.15, juce::String(bits) + " bits, " + getWeightingName(weighting));
                line << " " << juce::String(expected, 2) << " / " << juce::String(measured, 2) << " |";
            }
            m_lines.add(line);
        }
        // without dither the error is not white and depends on the signal; the textbook 6.02 N + 1.76 dB (full-scale sine against q^2/12) is a guide
        const measure::DynamicRangeResult plain = measure::measureDynamicRange(makeDevice([](double) { return std::make_unique<ref::Quantizer>(16, false); }), settings);
        const double textbook = 6.0206 * 16 + 1.7609;
        expectWithinAbsoluteError(plain.channels[0].dynamicRange.unweightedDbfs, textbook, 2.0, "undithered 16 bits");
        m_lines.add("| 16-bit quantizer, no dither | " + juce::String(textbook, 2) + " (6.02 N + 1.76) / " + juce::String(plain.channels[0].dynamicRange.unweightedDbfs, 2) + " | "
                    + juce::String(plain.channels[0].dynamicRange.bandDbfs, 2) + " | " + juce::String(plain.channels[0].dynamicRange.ccirRmsDbfs, 2) + " | "
                    + juce::String(plain.channels[0].dynamicRange.aWeightedDbfs, 2) + " |");
    }

    void testMains()
    {
        beginTest("mains products (6.5.1): hum lines at M x 50 and 60 Hz equal the hum adder's levels, their rms sum is the power line level");
        m_lines.add("\nmains products, 48 kHz, window 1 s (1 Hz bins), bands half the mains frequency wide; dBFS, expected / measured");
        for (const double mains : {50.0, 60.0})
        {
            const std::vector<double> relative = {-6.0, -12.0, -20.0, -30.0};
            measure::MainsSettings settings;
            settings.mainsHz = mains;
            settings.channels = 1;
            const measure::MainsResult result = measure::measureMainsProducts(
                makeDevice([mains, relative](double rate) { return std::make_unique<ref::HumAdder>(rate, mains, -90.0, relative); }), settings);
            juce::String line = juce::String(mains, 0) + " Hz hum at -90 dBFS with harmonics -6, -12, -20, -30 dB:";
            double total = 0.0;
            for (int multiple = 1; multiple <= 5; ++multiple)
            {
                double expected = -90.0;
                if (multiple > 1)
                {
                    expected += relative[static_cast<size_t>(multiple - 2)];
                }
                total += std::pow(10.0, expected / 10.0);
                expectWithinAbsoluteError(result.channels[0].lineDbfs[static_cast<size_t>(multiple - 1)], expected, 0.001, juce::String(mains) + " Hz, M = " + juce::String(multiple));
                line << " M=" << multiple << " " << juce::String(expected, 1) << "/" << juce::String(result.channels[0].lineDbfs[static_cast<size_t>(multiple - 1)], 3) << ";";
            }
            expectWithinAbsoluteError(result.channels[0].totalDbfs, 10.0 * std::log10(total), 0.001, juce::String(mains) + " Hz total");
            m_lines.add(line + " total " + juce::String(10.0 * std::log10(total), 3) + "/" + juce::String(result.channels[0].totalDbfs, 3));
        }

        // hum at 50 Hz, measured as 60 Hz mains: the bands at 60, 120, ... miss it (the 60 Hz band reaches 45 ... 75 Hz: it holds 50 Hz)
        measure::MainsSettings settings;
        settings.mainsHz = 60.0;
        settings.channels = 1;
        const measure::MainsResult wrong = measure::measureMainsProducts(makeDevice([](double rate) { return std::make_unique<ref::HumAdder>(rate, 50.0, -90.0); }), settings);
        expectWithinAbsoluteError(wrong.channels[0].lineDbfs[0], -90.0, 0.001, "50 Hz hum inside the 60 Hz band");
        m_lines.add("50 Hz hum (-90 dBFS, no harmonics) measured as 60 Hz mains: M=1 " + juce::String(wrong.channels[0].lineDbfs[0], 3) + " dBFS (the band 45 ... 75 Hz holds it), M=2 "
                    + juce::String(wrong.channels[0].lineDbfs[1], 1) + " dBFS");
    }

    juce::StringArray m_lines;
};

static MeasureNoiseTests measureNoiseTests;
