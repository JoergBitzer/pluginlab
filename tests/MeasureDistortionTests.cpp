#include <cmath>
#include <complex>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "MeasureTestDevices.h"
#include "pluginlab/measure/Distortion.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
const std::vector<double> kPolynomial = {0.0, 1.0, 0.1, 0.05, 0.0, 0.02};
constexpr double kComparableDbfs = -140.0;   // harmonics below this absolute level are lost in the float samples and are not compared

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}

// THD+N of the hard clipper (amplitude 1, threshold 0.5) with every harmonic up to the 20000th, folded below Nyquist. With a coherent frequency each
// alias lands on a bin; aliases on the same bin add with their signs (a component folded from above fs/2 changes sign).
double getAliasedClipperThdn(double frequency, int length)
{
    const double threshold = 0.5;
    const double a = std::asin(threshold);
    const double scale = 4.0 / kPi;
    std::vector<double> bins(static_cast<size_t>(length / 2 + 1), 0.0);
    const double binWidth = kSampleRate / length;
    for (int order = 1; order <= 20001; order += 2)
    {
        const double n = order;
        double coefficient = scale * (a / 2.0 - std::sin(2.0 * a) / 4.0 + threshold * std::cos(a));
        if (order > 1)
        {
            coefficient = scale * (0.5 * (std::sin((n - 1.0) * a) / (n - 1.0) - std::sin((n + 1.0) * a) / (n + 1.0)) + threshold * std::cos(n * a) / n);
        }
        double folded = std::fmod(n * frequency, kSampleRate);
        double sign = 1.0;
        if (folded > 0.5 * kSampleRate)
        {
            folded = kSampleRate - folded;
            sign = -1.0;
        }
        bins[static_cast<size_t>(std::lround(folded / binWidth))] += sign * coefficient;
    }
    const int fundamentalBin = static_cast<int>(std::lround(frequency / binWidth));
    double total = 0.0;
    double residual = 0.0;
    for (int bin = static_cast<int>(std::ceil(20.0 / binWidth)); bin <= static_cast<int>(std::floor(20000.0 / binWidth)); ++bin)
    {
        const double power = bins[static_cast<size_t>(bin)] * bins[static_cast<size_t>(bin)];
        total += power;
        if (bin != fundamentalBin)
        {
            residual += power;
        }
    }
    return 10.0 * std::log10(residual / total);
}

measure::Device makePolynomialDevice()
{
    return measure::makeProcessorDevice([](double) { return std::make_unique<ref::Waveshaper>(ref::Waveshaper::makePolynomial(kPolynomial)); });
}
}

// W7.5: THD+N and THD (AES17-2015 6.3, annex A.3.6, A.4.7; IEC 60268-3) against devices with known harmonics (docs/measurements/thd-and-thdn.md)
class MeasureDistortionTests : public juce::UnitTest
{
public:
    MeasureDistortionTests()
        : juce::UnitTest("Measure: THD and THD+N", "pluginlab")
    {
    }

    void runTest() override
    {
        testFft();
        testPolynomial();
        testClipperQuantizerNoise();
        testAgainstLevelAndFrequency();
        testSweptHarmonics();
        logMessage("results for docs/measurements/thd-and-thdn.md:\n" + m_lines.joinIntoString("\n"));
    }

private:
    void testFft()
    {
        beginTest("the analyzer's FFT equals the direct DFT; coherent frequencies lie on bins");
        juce::Random random(5);
        std::vector<double> data(1024);
        for (double& value : data)
        {
            value = random.nextDouble() - 0.5;
        }
        const std::vector<std::complex<double>> spectrum = measure::getSpectrum(data, 0, 1024);
        double largest = 0.0;
        for (int bin = 0; bin <= 512; bin += 37)
        {
            std::complex<double> direct = 0.0;
            for (int index = 0; index < 1024; ++index)
            {
                direct += data[static_cast<size_t>(index)] * std::polar(1.0, -2.0 * kPi * bin * index / 1024.0);
            }
            largest = std::max(largest, std::abs(direct - spectrum[static_cast<size_t>(bin)]));
        }
        expect(largest < 1.0e-10, juce::String(largest));
        expectWithinAbsoluteError(measure::getCoherentFrequency(997.0, kSampleRate, 65536), 1361.0 * kSampleRate / 65536.0, 1.0e-9);
    }

    void testPolynomial()
    {
        beginTest("polynomial: harmonics, THD and THD+N (both methods) equal the closed form at -1, -10, -20 dBFS");
        m_lines.add("| device | level | expected THD | THD | expected THD+N | THD+N (bins) | expected THD+N (notch) | THD+N (notch) |\n|---|---|---|---|---|---|---|---|");
        for (const double level : {-1.0, -10.0, -20.0})
        {
            measure::DistortionSettings settings;
            settings.levelDbfs = level;
            settings.channels = 1;
            const measure::DistortionResult result = measure::measureDistortion(makePolynomialDevice(), settings);
            const measure::ChannelDistortion& distortion = result.channels.front();
            const double amplitude = std::pow(10.0, level / 20.0);
            const std::vector<double> closed = ref::getPolynomialHarmonics(kPolynomial, amplitude, 10);
            const ref::BiquadCoefficients notch = ref::designRbj(ref::FilterType::Notch, kSampleRate, result.frequencyHz, 0.0, settings.notchQ);
            double harmonics = 0.0;
            double notched = 0.0;
            for (size_t index = 0; index < distortion.harmonicOrders.size(); ++index)
            {
                const int order = distortion.harmonicOrders[index];
                const double ratio = closed[static_cast<size_t>(order)] / closed[1];
                harmonics += ratio * ratio;
                notched += std::norm(ratio * ref::getBiquadResponse(notch, order * result.frequencyHz, kSampleRate));
                if (toDb(closed[static_cast<size_t>(order)] / std::sqrt(2.0)) + 3.0 > kComparableDbfs && ratio > 1.0e-6)
                {
                    expectWithinAbsoluteError(distortion.harmonicDb[index], toDb(ratio), 0.01, "harmonic " + juce::String(order) + " at " + juce::String(level));
                }
            }
            const double thd = 10.0 * std::log10(harmonics);
            const double thdn = 10.0 * std::log10(harmonics / (1.0 + harmonics));
            const double thdnNotch = 10.0 * std::log10(notched / (1.0 + harmonics));
            expectWithinAbsoluteError(distortion.thdDb, thd, 0.01, "THD");
            expectWithinAbsoluteError(distortion.thdnDb, thdn, 0.01, "THD+N");
            expectWithinAbsoluteError(distortion.thdnNotchDb, thdnNotch, 0.01, "THD+N notch");
            m_lines.add("| x + 0.1x^2 + 0.05x^3 + 0.02x^5 | " + juce::String(level) + " dBFS | " + juce::String(thd, 3) + " | " + juce::String(distortion.thdDb, 3) + " | "
                        + juce::String(thdn, 3) + " | " + juce::String(distortion.thdnDb, 3) + " | " + juce::String(thdnNotch, 3) + " | "
                        + juce::String(distortion.thdnNotchDb, 3) + " |");
        }
    }

    void testClipperQuantizerNoise()
    {
        beginTest("hard clipper (Fourier series; its aliases count in THD+N only), dithered 16-bit quantizer and added noise (THD+N = the known noise)");
        measure::DistortionSettings settings;
        settings.channels = 1;
        settings.levelDbfs = 0.0;
        const measure::DistortionResult clipped = measure::measureDistortion(
            measure::makeProcessorDevice([](double) { return std::make_unique<ref::Waveshaper>(ref::Waveshaper::makeHardClip(0.5)); }), settings);
        const std::vector<double> fourier = ref::getHardClipHarmonics(1.0, 0.5, 10);
        double harmonics = 0.0;
        for (size_t index = 0; index < clipped.channels[0].harmonicOrders.size(); ++index)
        {
            const int order = clipped.channels[0].harmonicOrders[index];
            const double ratio = fourier[static_cast<size_t>(order)] / fourier[1];
            harmonics += ratio * ratio;
            if (ratio > 1.0e-6)
            {
                // the aliases of the harmonics above Nyquist fall between the harmonics (997 Hz and 48 kHz have no common divisor), not on them
                expectWithinAbsoluteError(clipped.channels[0].harmonicDb[index], toDb(ratio), 0.05, "clipper harmonic " + juce::String(order));
            }
        }
        expectWithinAbsoluteError(clipped.channels[0].thdDb, 10.0 * std::log10(harmonics), 0.05, "clipper THD");
        // THD+N is relative to the total, THD to the fundamental: N / (F + N) = r  ->  N / F = r / (1 - r)
        const double thdnRatio = std::pow(10.0, clipped.channels[0].thdnDb / 10.0);
        const double thdnToFundamentalDb = 10.0 * std::log10(thdnRatio / (1.0 - thdnRatio));
        const double expectedClipperThdn = getAliasedClipperThdn(clipped.frequencyHz, clipped.windowSamples);
        expectWithinAbsoluteError(clipped.channels[0].thdnDb, expectedClipperThdn, 0.01, "clipper THD+N with all harmonics and their aliases");
        m_lines.add("| hard clip at 0.5, 0 dBFS | | " + juce::String(10.0 * std::log10(harmonics), 3) + " (Fourier, 2nd ... 10th) | " + juce::String(clipped.channels[0].thdDb, 3)
                    + " | " + juce::String(expectedClipperThdn, 3) + " (all harmonics, aliased) | " + juce::String(clipped.channels[0].thdnDb, 3) + " (re fundamental " + juce::String(thdnToFundamentalDb, 3) + ") | | " + juce::String(clipped.channels[0].thdnNotchDb, 3) + " |");

        // white noise of power p spreads evenly over the N/2 bins; the band 20 Hz ... 20 kHz holds the share (20000 - 20) / 24000 of it
        const double bandShare = (20000.0 - 20.0) / 24000.0;
        settings.levelDbfs = -1.0;
        const double signalPower = std::pow(10.0, -1.0 / 10.0) / 2.0;
        const double step = std::pow(2.0, -15.0);
        const double quantizerNoise = step * step / 4.0 * bandShare;
        const measure::DistortionResult quantized = measure::measureDistortion(
            measure::makeProcessorDevice([](double) { return std::make_unique<ref::Quantizer>(16, true); }), settings);
        const double expectedQuantizer = 10.0 * std::log10(quantizerNoise / (signalPower + quantizerNoise));
        expectWithinAbsoluteError(quantized.channels[0].thdnDb, expectedQuantizer, 0.3, "quantizer THD+N");
        m_lines.add("| 16-bit quantizer, TPDF dither | -1 dBFS | | " + juce::String(quantized.channels[0].thdDb, 3) + " | " + juce::String(expectedQuantizer, 3) + " (q^2/4 in band) | "
                    + juce::String(quantized.channels[0].thdnDb, 3) + " | | " + juce::String(quantized.channels[0].thdnNotchDb, 3) + " |");

        settings.levelDbfs = -20.0;
        const double sinePower = std::pow(10.0, -20.0 / 10.0) / 2.0;
        const measure::DistortionResult noisy = measure::measureDistortion(
            measure::makeProcessorDevice([](double) { return std::make_unique<ref::NoiseAdder>(pluginlab::signals::NoiseColour::WhiteGaussian, -73.0103, 3); }),
            settings);
        // the noise adder's level is the rms of the noise; -73.01 dBFS rms (sample rms) = -70 dBFS in the AES17 sense (+3.01 dB)
        const double noiseRms = std::pow(10.0, -73.0103 / 20.0);
        const double expectedNoise = 10.0 * std::log10(noiseRms * noiseRms * bandShare / (sinePower + noiseRms * noiseRms * bandShare));
        expectWithinAbsoluteError(noisy.channels[0].thdnDb, expectedNoise, 0.3, "noise THD+N");
        m_lines.add("| white noise -70 dBFS (AES17) added | -20 dBFS | | " + juce::String(noisy.channels[0].thdDb, 3) + " | " + juce::String(expectedNoise, 3) + " | "
                    + juce::String(noisy.channels[0].thdnDb, 3) + " | | " + juce::String(noisy.channels[0].thdnNotchDb, 3) + " |");
    }

    void testAgainstLevelAndFrequency()
    {
        beginTest("THD against level (AES17 6.3.3, polynomial) and against frequency (6.3.2, Hammerstein: polynomial then a 4th-order low-pass)");
        measure::DistortionSettings settings;
        settings.channels = 1;
        const std::vector<double> levels = {0.0, -10.0, -20.0, -30.0, -40.0};
        const std::vector<measure::DistortionResult> byLevel = measure::measureDistortionVsLevel(makePolynomialDevice(), settings, levels);
        juce::String line = "THD against level (polynomial):";
        for (size_t index = 0; index < levels.size(); ++index)
        {
            const std::vector<double> closed = ref::getPolynomialHarmonics(kPolynomial, std::pow(10.0, levels[index] / 20.0), 10);
            double harmonics = 0.0;
            for (const int order : byLevel[index].channels[0].harmonicOrders)
            {
                harmonics += std::pow(closed[static_cast<size_t>(order)] / closed[1], 2.0);
            }
            expectWithinAbsoluteError(byLevel[index].channels[0].thdDb, 10.0 * std::log10(harmonics), 0.01, "THD at " + juce::String(levels[index]));
            line << " " << juce::String(levels[index]) << " dBFS: " << juce::String(byLevel[index].channels[0].thdDb, 2) << " dB;";
        }
        m_lines.add("\n" + line);

        const std::vector<ref::BiquadCoefficients> lowPass = ref::designButterworth(ref::Pass::Low, 4, kSampleRate, 2000.0);
        const measure::Device hammerstein = measure::makeProcessorDevice([lowPass](double rate) { return std::make_unique<pluginlab::test::Hammerstein>(kPolynomial, lowPass, rate); });
        settings.levelDbfs = -6.0;
        const std::vector<double> frequencies = {100.0, 300.0, 700.0, 1000.0, 1500.0};
        const std::vector<measure::DistortionResult> byFrequency = measure::measureDistortionVsFrequency(hammerstein, settings, frequencies);
        line = "Hammerstein, harmonic 2 / 3 against frequency (stepped):";
        const std::vector<double> closed = ref::getPolynomialHarmonics(kPolynomial, std::pow(10.0, -6.0 / 20.0), 10);
        for (const measure::DistortionResult& result : byFrequency)
        {
            const double f = result.frequencyHz;
            const double fundamental = closed[1] * std::abs(ref::getCascadeResponse(lowPass, f, kSampleRate));
            for (size_t index = 0; index < 2; ++index)
            {
                const int order = result.channels[0].harmonicOrders[index];
                const double expected = toDb(closed[static_cast<size_t>(order)] * std::abs(ref::getCascadeResponse(lowPass, order * f, kSampleRate)) / fundamental);
                expectWithinAbsoluteError(result.channels[0].harmonicDb[index], expected, 0.01, "Hammerstein harmonic " + juce::String(order) + " at " + juce::String(f));
            }
            line << " " << juce::String(f, 0) << " Hz: " << juce::String(result.channels[0].harmonicDb[0], 2) << " / " << juce::String(result.channels[0].harmonicDb[1], 2) << " dB;";
        }
        m_lines.add(line);
    }

    void testSweptHarmonics()
    {
        beginTest("harmonics from the synchronized sweep (AES17 A.4.7, Novak): polynomial (flat) and Hammerstein (falling with the low-pass)");
        measure::SweepResponseSettings sweep;
        sweep.levelDbfs = -6.0;
        sweep.channels = 1;
        const std::vector<double> frequencies = {100.0, 300.0, 700.0, 1000.0, 1500.0, 3000.0};
        const double amplitude = std::pow(10.0, -6.0 / 20.0);
        const std::vector<double> closed = ref::getPolynomialHarmonics(kPolynomial, amplitude, 5);

        const measure::HarmonicResponse flat = measure::measureSweptHarmonics(makePolynomialDevice(), sweep, 5, frequencies);
        const std::vector<ref::BiquadCoefficients> lowPass = ref::designButterworth(ref::Pass::Low, 4, kSampleRate, 2000.0);
        const measure::HarmonicResponse filtered = measure::measureSweptHarmonics(
            measure::makeProcessorDevice([lowPass](double rate) { return std::make_unique<pluginlab::test::Hammerstein>(kPolynomial, lowPass, rate); }), sweep, 5, frequencies);
        double flatError = 0.0;
        double filteredError = 0.0;
        juce::String line = "sweep, harmonic 2 / 3: ";
        for (size_t index = 0; index < frequencies.size(); ++index)
        {
            const double f = frequencies[index];
            for (int order = 2; order <= 3; ++order)
            {
                const double measuredFlat = flat.channels[0].harmonicDb[static_cast<size_t>(order - 2)][index];
                const double expectedFlat = toDb(closed[static_cast<size_t>(order)] / closed[1]);
                flatError = std::max(flatError, std::abs(measuredFlat - expectedFlat));
                const double expectedFiltered = toDb(closed[static_cast<size_t>(order)] * std::abs(ref::getCascadeResponse(lowPass, order * f, kSampleRate))
                                                     / (closed[1] * std::abs(ref::getCascadeResponse(lowPass, f, kSampleRate))));
                const double measuredFiltered = filtered.channels[0].harmonicDb[static_cast<size_t>(order - 2)][index];
                if (expectedFiltered > -80.0)
                {
                    filteredError = std::max(filteredError, std::abs(measuredFiltered - expectedFiltered));
                }
            }
            line << juce::String(f, 0) << " Hz: polynomial " << juce::String(flat.channels[0].harmonicDb[0][index], 2) << " / "
                 << juce::String(flat.channels[0].harmonicDb[1][index], 2) << ", Hammerstein " << juce::String(filtered.channels[0].harmonicDb[0][index], 2) << " / "
                 << juce::String(filtered.channels[0].harmonicDb[1][index], 2) << " dB; ";
        }
        logMessage("swept harmonics: largest error polynomial " + juce::String(flatError, 3) + " dB, Hammerstein " + juce::String(filteredError, 3) + " dB");
        m_lines.add(line + "\nlargest error: polynomial " + juce::String(flatError, 3) + " dB, Hammerstein " + juce::String(filteredError, 3) + " dB (above -80 dB)");
        expect(flatError < 0.05, "polynomial " + juce::String(flatError));
        expect(filteredError < 0.05, "Hammerstein " + juce::String(filteredError));
    }

    juce::StringArray m_lines;
};

static MeasureDistortionTests measureDistortionTests;
