#include <cmath>
#include <complex>
#include <map>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "MeasureTestDevices.h"
#include "pluginlab/measure/Intermodulation.h"
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

using SparseSpectrum = std::map<int, std::complex<double>>;   // bin (mod N) -> complex amplitude of e^{j 2 pi k n / N}

struct Tone
{
    double frequencyHz;
    double amplitude;
};

// The exact spectrum of y = sum c_n x^n for x = sum a_i sin(2 pi f_i n / fs) seen from the window start (f_i on bins of the window of N samples):
// each sine is two exponentials, the powers are cyclic convolutions (mod N: the aliases land where the sampled signal puts them), and a filter after
// the curve multiplies each bin by its response
SparseSpectrum getExactSpectrum(const std::vector<double>& coefficients, const std::vector<Tone>& tones, int start, int length,
                                const std::vector<ref::BiquadCoefficients>& filter = {})
{
    SparseSpectrum x;
    for (const Tone& tone : tones)
    {
        const int bin = static_cast<int>(std::lround(tone.frequencyHz * length / kSampleRate));
        const double phase = 2.0 * kPi * static_cast<double>(bin) * start / length;
        const std::complex<double> positive = tone.amplitude / std::complex<double>(0.0, 2.0) * std::polar(1.0, phase);
        x[bin] += positive;
        x[length - bin] += std::conj(positive);
    }
    SparseSpectrum y;
    SparseSpectrum power = {{0, 1.0}};
    for (size_t order = 0; order < coefficients.size(); ++order)
    {
        if (order > 0)
        {
            SparseSpectrum next;
            for (const auto& [first, a] : power)
            {
                for (const auto& [second, b] : x)
                {
                    next[(first + second) % length] += a * b;
                }
            }
            power = next;
        }
        for (const auto& [bin, value] : power)
        {
            y[bin] += coefficients[order] * value;
        }
    }
    if (! filter.empty())
    {
        for (auto& [bin, value] : y)
        {
            value *= ref::getCascadeResponse(filter, static_cast<double>(bin) * kSampleRate / length, kSampleRate);
        }
    }
    return y;
}

// the same band-pass as the unit: the rms of all bins within +- bandwidth / 2 (2 |Y_k|^2 per bin of a real signal)
double getBandRms(const SparseSpectrum& spectrum, int length, double centreHz, double bandwidthHz)
{
    const double binWidth = kSampleRate / length;
    const int first = std::max(1, static_cast<int>(std::ceil((centreHz - 0.5 * bandwidthHz) / binWidth)));
    const int last = std::min(length / 2 - 1, static_cast<int>(std::floor((centreHz + 0.5 * bandwidthHz) / binWidth)));
    double power = 0.0;
    for (const auto& [bin, value] : spectrum)
    {
        if (bin >= first && bin <= last)
        {
            power += 2.0 * std::norm(value);
        }
    }
    return std::sqrt(power);
}

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}

measure::Device makePolynomialDevice(std::vector<double> coefficients)
{
    return measure::makeProcessorDevice([coefficients](double) { return std::make_unique<ref::Waveshaper>(ref::Waveshaper::makePolynomial(coefficients)); });
}

juce::String formatDb(double value)
{
    if (std::isnan(value))
    {
        return "-";
    }
    return juce::String(value, 3);
}
}

// W7.6: intermodulation distortion (AES17-2015 6.3.5, 6.3.6) against the exact products of polynomials (docs/measurements/intermodulation.md)
class MeasureIntermodulationTests : public juce::UnitTest
{
public:
    MeasureIntermodulationTests()
        : juce::UnitTest("Measure: intermodulation", "pluginlab")
    {
    }

    void runTest() override
    {
        testDifferenceFrequency();
        testModulation();
        testNoise();
        logMessage("results for docs/measurements/intermodulation.md:\n" + m_lines.joinIntoString("\n"));
    }

private:
    struct DifferenceCase
    {
        juce::String name;
        std::vector<double> coefficients;
        double levelDbfs;
        std::vector<ref::BiquadCoefficients> filter;
    };

    void testDifferenceFrequency()
    {
        beginTest("difference-frequency distortion (6.3.5, 18 + 20 kHz): polynomials and a Hammerstein system equal the exact products");
        const std::vector<ref::BiquadCoefficients> lowPass = ref::designButterworth(ref::Pass::Low, 4, kSampleRate, 10000.0);
        const std::vector<DifferenceCase> cases = {
            {"x + 0.1 x^2", {0.0, 1.0, 0.1}, 0.0, {}},
            {"x + 0.05 x^3", {0.0, 1.0, 0.0, 0.05}, 0.0, {}},
            {"x + 0.1x^2 + 0.05x^3 + 0.02x^5", kPolynomial, 0.0, {}},
            {"x + 0.1x^2 + 0.05x^3 + 0.02x^5", kPolynomial, -10.0, {}},
            {"x + 0.1x^2 + 0.05x^3 + 0.02x^5", kPolynomial, -20.0, {}},
            {"the polynomial, then Butterworth low-pass 4th order 10 kHz", kPolynomial, 0.0, lowPass},
        };
        m_lines.add("difference-frequency distortion, 48 kHz, levels: peak of the sum; dB re the lower fundamental (expected / measured)");
        m_lines.add("| device | level | 2nd order f2-f1 | lower 3rd 2f1-f2 | upper 3rd 2f2-f1 | DFD ratio |\n|---|---|---|---|---|---|");
        for (const DifferenceCase& item : cases)
        {
            measure::DifferenceFrequencySettings settings;
            settings.levelDbfs = item.levelDbfs;
            settings.channels = 1;
            measure::Device device = makePolynomialDevice(item.coefficients);
            if (! item.filter.empty())
            {
                const std::vector<ref::BiquadCoefficients> filter = item.filter;
                const std::vector<double> coefficients = item.coefficients;
                device = measure::makeProcessorDevice([filter, coefficients](double rate)
                                                      { return std::make_unique<pluginlab::test::Hammerstein>(coefficients, filter, rate); });
            }
            const measure::DifferenceFrequencyResult result = measure::measureDifferenceFrequency(device, settings);
            const measure::ChannelDifferenceFrequency& measured = result.channels[0];
            const double amplitude = 0.5 * std::pow(10.0, item.levelDbfs / 20.0);
            const SparseSpectrum exact = getExactSpectrum(item.coefficients, {{result.lowerHz, amplitude}, {result.upperHz, amplitude}}, result.windowStart,
                                                          result.windowSamples, item.filter);
            const double difference = result.upperHz - result.lowerHz;
            const auto band = [&](double centre) { return getBandRms(exact, result.windowSamples, centre, settings.bandwidthHz); };
            const double fundamental = band(result.lowerHz);
            const double second = band(difference);
            const double lowerThird = band(result.lowerHz - difference);
            const double upperThird = band(result.upperHz + difference);
            const double ratio = toDb(std::sqrt(second * second + lowerThird * lowerThird + upperThird * upperThird) / fundamental);
            const juce::String label = item.name + " at " + juce::String(item.levelDbfs) + " dBFS";
            expectWithinAbsoluteError(measured.lowerFundamentalDbfs, toDb(fundamental * std::sqrt(2.0)), 0.001, label + ": fundamental");
            expectProduct(measured.secondOrderDb, toDb(second / fundamental), label + ": 2nd order");
            expectProduct(measured.lowerThirdOrderDb, toDb(lowerThird / fundamental), label + ": lower 3rd order");
            expectProduct(measured.upperThirdOrderDb, toDb(upperThird / fundamental), label + ": upper 3rd order");
            expectProduct(measured.ratioDb, ratio, label + ": ratio");
            m_lines.add("| " + item.name + " | " + juce::String(item.levelDbfs) + " dBFS | " + formatDb(toDb(second / fundamental)) + " / " + formatDb(measured.secondOrderDb)
                        + " | " + formatDb(toDb(lowerThird / fundamental)) + " / " + formatDb(measured.lowerThirdOrderDb) + " | " + formatDb(toDb(upperThird / fundamental))
                        + " / " + formatDb(measured.upperThirdOrderDb) + " | " + formatDb(ratio) + " / " + formatDb(measured.ratioDb) + " |");
        }

        // a linear device has no products: the ratio is the float floor
        measure::DifferenceFrequencySettings settings;
        settings.channels = 1;
        const measure::DifferenceFrequencyResult linear = measure::measureDifferenceFrequency(makePolynomialDevice({0.0, 0.5}), settings);
        expect(linear.channels[0].ratioDb < -120.0, "linear: " + juce::String(linear.channels[0].ratioDb));
        m_lines.add("| gain 0.5 (linear) | 0 dBFS | | | | floor " + formatDb(linear.channels[0].ratioDb) + " |");
    }

    void testModulation()
    {
        beginTest("modulation distortion (6.3.6, 41 Hz + 7993 Hz, 4:1): the first sidebands see the even orders, odd orders appear at f2 +- 2 f1");
        const std::vector<std::pair<juce::String, std::vector<double>>> cases = {
            {"x + 0.1 x^2", {0.0, 1.0, 0.1}},
            {"x + 0.05 x^3", {0.0, 1.0, 0.0, 0.05}},
            {"x + 0.1x^2 + 0.05x^3 + 0.02x^5", kPolynomial},
        };
        m_lines.add("\nmodulation distortion, 48 kHz, 0 dBFS peak; dB re the upper fundamental (expected / measured)");
        m_lines.add("| device | sidebands +-41 Hz (lower / upper) | +-82 Hz | +-123 Hz | AES17 ratio (+-41 Hz) | all sidebands |\n|---|---|---|---|---|---|");
        for (const auto& [caseName, coefficients] : cases)
        {
            measure::ModulationSettings settings;
            settings.channels = 1;
            const measure::ModulationResult result = measure::measureModulation(makePolynomialDevice(coefficients), settings);
            const measure::ChannelModulation& measured = result.channels[0];
            const SparseSpectrum exact = getExactSpectrum(coefficients, {{result.lowHz, 0.8}, {result.highHz, 0.2}}, result.windowStart, result.windowSamples);
            const auto band = [&](double centre) { return getBandRms(exact, result.windowSamples, centre, settings.bandwidthHz); };
            const double fundamental = band(result.highHz);
            expectWithinAbsoluteError(measured.upperFundamentalDbfs, toDb(fundamental * std::sqrt(2.0)), 0.001, caseName + ": fundamental");
            juce::String line = "| " + caseName + " |";
            double first = 0.0;
            double all = 0.0;
            for (int order = 1; order <= settings.sidebandOrders; ++order)
            {
                const double lower = band(result.highHz - order * result.lowHz);
                const double upper = band(result.highHz + order * result.lowHz);
                expectProduct(measured.lowerSidebandDb[static_cast<size_t>(order - 1)], toDb(lower / fundamental), caseName + ": lower sideband " + juce::String(order));
                expectProduct(measured.upperSidebandDb[static_cast<size_t>(order - 1)], toDb(upper / fundamental), caseName + ": upper sideband " + juce::String(order));
                line << " " << formatDb(toDb(lower / fundamental)) << " / " << formatDb(measured.lowerSidebandDb[static_cast<size_t>(order - 1)]) << ", "
                     << formatDb(toDb(upper / fundamental)) << " / " << formatDb(measured.upperSidebandDb[static_cast<size_t>(order - 1)]) << " |";
                all += lower * lower + upper * upper;
                if (order == 1)
                {
                    first = lower * lower + upper * upper;
                }
            }
            expectProduct(measured.ratioDb, toDb(std::sqrt(first) / fundamental), caseName + ": ratio");
            expectProduct(measured.allSidebandsDb, toDb(std::sqrt(all) / fundamental), caseName + ": all sidebands");
            line << " " << formatDb(toDb(std::sqrt(first) / fundamental)) << " / " << formatDb(measured.ratioDb) << " | " << formatDb(toDb(std::sqrt(all) / fundamental))
                 << " / " << formatDb(measured.allSidebandsDb) << " |";
            m_lines.add(line);
        }
    }

    void testNoise()
    {
        beginTest("dithered 16-bit quantizer: the bands hold the noise of their width in Hz, independent of the window length (AES17 annex B.5)");
        // white noise of power q^2/4 (quantization q^2/12 plus TPDF dither q^2/6) spreads evenly over the N/2 bins
        const double step = std::pow(2.0, -15.0);
        const double noisePower = step * step / 4.0;
        bool first = true;
        for (const double seconds : {1.0, 4.0})
        {
            measure::DifferenceFrequencySettings settings;
            settings.channels = 1;
            settings.measureSeconds = seconds;
            const measure::DifferenceFrequencyResult result = measure::measureDifferenceFrequency(
                measure::makeProcessorDevice([](double) { return std::make_unique<ref::Quantizer>(16, true); }), settings);
            const double binWidth = kSampleRate / result.windowSamples;
            int bins = 0;
            const double difference = result.upperHz - result.lowerHz;
            for (const double centre : {difference, result.lowerHz - difference, result.upperHz + difference})
            {
                bins += static_cast<int>(std::floor((centre + 250.0) / binWidth)) - static_cast<int>(std::ceil((centre - 250.0) / binWidth)) + 1;
            }
            const double expected = 10.0 * std::log10(noisePower * bins / (result.windowSamples / 2.0) / (0.5 * 0.5 / 2.0));
            expectWithinAbsoluteError(result.channels[0].ratioDb, expected, 0.3, "quantizer, window " + juce::String(result.windowSamples));
            juce::String gap;
            if (first)
            {
                gap = "\n";
            }
            first = false;
            m_lines.add(gap + juce::String("16-bit quantizer, TPDF dither, 0 dBFS, window ") + juce::String(result.windowSamples) + ": DFD "
                        + formatDb(result.channels[0].ratioDb) + " dB, expected (noise in 3 x 500 Hz) " + formatDb(expected) + " dB");
        }
    }

    // products far below the float noise of the samples cannot be compared
    void expectProduct(double measured, double expected, const juce::String& label)
    {
        if (std::isnan(expected) || std::isnan(measured))
        {
            expect(std::isnan(expected) == std::isnan(measured), label + ": NaN");
            return;
        }
        if (expected < -130.0)
        {
            expect(measured < -110.0, label + ": " + juce::String(measured) + " (expected below the floor)");
            return;
        }
        expectWithinAbsoluteError(measured, expected, 0.01, label);
    }

    juce::StringArray m_lines;
};

static MeasureIntermodulationTests measureIntermodulationTests;
