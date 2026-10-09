#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "MeasureTestDevices.h"
#include "pluginlab/measure/Linearity.h"
#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;

// THD+N (bins 20 Hz ... 20 kHz except the fundamental, relative to all of them; as measureDistortion) of a sine of amplitude A through a hard clipper
// at c, from its Fourier series up to the 20001st harmonic folded below Nyquist (aliases on one bin add with their signs)
double getClipperThdnDb(double amplitude, double threshold, double frequency, int length)
{
    if (amplitude <= threshold)
    {
        return -1000.0;
    }
    const double a = std::asin(threshold / amplitude);
    const double scale = 4.0 / kPi;
    std::vector<double> bins(static_cast<size_t>(length / 2 + 1), 0.0);
    const double binWidth = kSampleRate / length;
    for (int order = 1; order <= 20001; order += 2)
    {
        const double n = order;
        double coefficient = scale * (amplitude * (a / 2.0 - std::sin(2.0 * a) / 4.0) + threshold * std::cos(a));
        if (order > 1)
        {
            coefficient = scale * (amplitude / 2.0 * (std::sin((n - 1.0) * a) / (n - 1.0) - std::sin((n + 1.0) * a) / (n + 1.0)) + threshold * std::cos(n * a) / n);
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

// the rms of the sampled curve output over the window (the device sees exactly these samples; the standard low-pass is the identity at 48 kHz)
double getOutputRms(const std::function<double(double)>& curve, double amplitude, double frequency, int length)
{
    double sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        const double value = curve(amplitude * std::sin(2.0 * kPi * frequency * index / kSampleRate));
        sum += value * value;
    }
    return std::sqrt(sum / length);
}

// the level (dBFS) in [low, high] where a rising function crosses the limit, by bisection
double solve(const std::function<double(double)>& function, double limit, double low, double high)
{
    for (int step = 0; step < 60; ++step)
    {
        const double middle = 0.5 * (low + high);
        if (function(middle) >= limit)
        {
            high = middle;
        }
        else
        {
            low = middle;
        }
    }
    return 0.5 * (low + high);
}

double toAmplitude(double dbfs)
{
    return std::pow(10.0, dbfs / 20.0);
}

measure::Device makeDevice(std::function<std::unique_ptr<ref::Processor>(double)> factory)
{
    return measure::makeProcessorDevice(std::move(factory));
}
}

// W7.9: maximum input and output level, overload, gain non-linearity (AES17-2015 6.2.1, 6.2.6, 6.6.8, 6.3.7) (docs/measurements/maximum-level-and-linearity.md)
class MeasureLinearityTests : public juce::UnitTest
{
public:
    MeasureLinearityTests()
        : juce::UnitTest("Measure: maximum level and linearity", "pluginlab")
    {
    }

    void runTest() override
    {
        testThdnMethod();
        testCompressionMethod();
        testLinearity();
        logMessage("results for docs/measurements/maximum-level-and-linearity.md:\n" + m_lines.joinIntoString("\n"));
    }

private:
    void testThdnMethod()
    {
        beginTest("maximum input level, THD+N method (6.2.1 a): hard clippers and the quantizer's full scale at the closed-form level; overload and rollover (6.6.8)");
        m_lines.add("| device | method | expected max. input (dBFS) | measured | expected max. output | measured | overload THD+N (+3 dB) | rollover |\n|---|---|---|---|---|---|---|---|");
        struct Case
        {
            juce::String name;
            std::function<std::unique_ptr<ref::Processor>(double)> factory;
            double inputScale;                   // the clipper sees inputScale x the input
            double threshold;                    // ... and clips at this
        };
        const double step16 = std::pow(2.0, -15.0);
        const std::vector<Case> cases = {
            {"hard clip at 0.5 (-6.02 dBFS)", [](double) { return std::make_unique<ref::Waveshaper>(ref::Waveshaper::makeHardClip(0.5)); }, 1.0, 0.5},
            {"gain +6 dB, then hard clip at 1", [](double)
             {
                 auto chain = std::make_unique<pluginlab::test::Chain>();
                 chain->add(std::make_unique<ref::Gain>(6.0, false));
                 chain->add(std::make_unique<ref::Waveshaper>(ref::Waveshaper::makeHardClip(1.0)));
                 return chain;
             },
             std::pow(10.0, 6.0 / 20.0), 1.0},
            {"16-bit quantizer, no dither (clips at 1 - q)", [](double) { return std::make_unique<ref::Quantizer>(16, false); }, 1.0, 1.0 - step16},
        };
        for (const Case& item : cases)
        {
            measure::MaximumLevelSettings settings;
            const measure::MaximumLevelResult result = measure::measureMaximumLevel(makeDevice(item.factory), settings);
            const auto thdn = [&](double level) { return getClipperThdnDb(item.inputScale * toAmplitude(level), item.threshold, result.frequencyHz, result.windowSamples); };
            const double expected = solve(thdn, -40.0, -30.0, 10.0);
            const auto clip = [&item](double x) { return std::max(-item.threshold, std::min(item.threshold, item.inputScale * x)); };
            const double expectedOutput = measure::rmsToDbfs(getOutputRms(clip, toAmplitude(result.maximumInputDbfs), result.frequencyHz, result.windowSamples));
            expect(result.found, item.name + ": found");
            expectWithinAbsoluteError(result.maximumInputDbfs, expected, 0.02, item.name + ": maximum input");
            expectWithinAbsoluteError(result.maximumOutputDbfs, expectedOutput, 0.01, item.name + ": maximum output");
            expect(! result.rollover, item.name + ": no rollover");
            m_lines.add("| " + item.name + " | THD+N -40 dB | " + juce::String(expected, 3) + " | " + juce::String(result.maximumInputDbfs, 3) + " | " + juce::String(expectedOutput, 3)
                        + " | " + juce::String(result.maximumOutputDbfs, 3) + " | " + juce::String(result.overloadThdnDb, 2) + " dB | no |");
        }

        measure::MaximumLevelSettings settings;
        const measure::MaximumLevelResult linear = measure::measureMaximumLevel(makeDevice([](double) { return std::make_unique<ref::Gain>(-6.0, false); }), settings);
        expect(! linear.found, "a gain never distorts (up to +24 dBFS)");
        m_lines.add("| gain -6 dB | THD+N -40 dB | none up to +24 | none | | | | |");

        const measure::MaximumLevelResult noisy = measure::measureMaximumLevel(
            makeDevice([](double) { return std::make_unique<ref::NoiseAdder>(pluginlab::signals::NoiseColour::WhiteGaussian, -40.0, 5); }), settings);
        expect(! noisy.found && noisy.exceededEverywhere, "noise at -40 dB rms: THD+N above -40 dB at every level");
        m_lines.add("| white noise -40 dB rms added | THD+N -40 dB | none (noise alone exceeds) | none | | | | |");

        const measure::MaximumLevelResult wrapped = measure::measureMaximumLevel(makeDevice([](double) { return std::make_unique<pluginlab::test::Wraparound>(); }), settings);
        // at 0 dBFS one sample of the coherent sine is exactly +1, which wraps to -1: the criterion is met at full scale (within the 0.01 dB resolution)
        expect(wrapped.found && std::abs(wrapped.maximumInputDbfs) < 0.01, "wraparound: at full scale " + juce::String(wrapped.maximumInputDbfs));
        expect(wrapped.rollover, "wraparound: rollover " + juce::String(wrapped.overloadThdnDb));
        m_lines.add("| wraparound beyond +-1 (integer overflow) | THD+N -40 dB | 0 (+1 wraps to -1) | " + juce::String(wrapped.maximumInputDbfs, 3) + " | | "
                    + juce::String(wrapped.maximumOutputDbfs, 3) + " | " + juce::String(wrapped.overloadThdnDb, 2) + " dB | **yes** |");
    }

    void testCompressionMethod()
    {
        beginTest("maximum input level, compression method (6.2.1 b): tanh at the level where the gain has fallen by 0.3 dB; the THD+N method on the same curve");
        const auto curve = [](double x) { return std::tanh(x); };
        measure::MaximumLevelSettings settings;
        settings.method = measure::MaximumLevelMethod::Compression;
        const measure::Device device = makeDevice([](double) { return std::make_unique<ref::Waveshaper>(ref::Waveshaper::makeSoftClip(1.0)); });
        const measure::MaximumLevelResult result = measure::measureMaximumLevel(device, settings);
        const auto gain = [&](double level)
        {
            return measure::rmsToDbfs(getOutputRms(curve, toAmplitude(level), result.frequencyHz, result.windowSamples)) - level;
        };
        const double reference = gain(settings.referenceLevelDbfs);
        const double expected = solve([&](double level) { return reference - gain(level); }, 0.3, -30.0, 10.0);
        expect(result.found, "tanh compression: found");
        expectWithinAbsoluteError(result.maximumInputDbfs, expected, 0.02, "tanh compression");
        expectWithinAbsoluteError(result.gainAtMaximumDb, gain(result.maximumInputDbfs), 0.01, "tanh gain at the maximum");

        settings.method = measure::MaximumLevelMethod::ThdN;
        const measure::MaximumLevelResult byThdn = measure::measureMaximumLevel(device, settings);
        // tanh: the harmonics (no closed form; numerical Fourier coefficients) fall fast, so harmonics above the 20th and aliases do not matter at -40 dB
        const auto thdn = [](double level)
        {
            const std::vector<double> harmonics = ref::getShaperHarmonics(ref::Waveshaper::makeSoftClip(1.0), toAmplitude(level), 20);
            double residual = 0.0;
            for (size_t order = 2; order < harmonics.size(); ++order)
            {
                residual += harmonics[order] * harmonics[order];
            }
            return 10.0 * std::log10(residual / (residual + harmonics[1] * harmonics[1]));
        };
        const double expectedThdn = solve(thdn, -40.0, -30.0, 10.0);
        expectWithinAbsoluteError(byThdn.maximumInputDbfs, expectedThdn, 0.02, "tanh THD+N");
        m_lines.add("| tanh(x) | compression 0.3 dB (re -40 dBFS) | " + juce::String(expected, 3) + " | " + juce::String(result.maximumInputDbfs, 3) + " | | "
                    + juce::String(result.maximumOutputDbfs, 3) + " | " + juce::String(result.overloadThdnDb, 2) + " dB | no |");
        m_lines.add("| tanh(x) | THD+N -40 dB | " + juce::String(expectedThdn, 3) + " | " + juce::String(byThdn.maximumInputDbfs, 3) + " | | " + juce::String(byThdn.maximumOutputDbfs, 3)
                    + " | " + juce::String(byThdn.overloadThdnDb, 2) + " dB | no |");
    }

    void testLinearity()
    {
        beginTest("gain non-linearity (6.3.7): a gain is linear to -140 dBFS; dither keeps the quantizer linear to its noise; without dither small signals vanish");
        measure::LinearitySettings settings;
        const measure::LinearityResult gain = measure::measureGainLinearity(makeDevice([](double) { return std::make_unique<ref::Gain>(-6.0, false); }), settings);
        double largest = 0.0;
        for (const double deviation : gain.deviationDb)
        {
            largest = std::max(largest, std::abs(deviation));
        }
        expectWithinAbsoluteError(gain.inputDbfs.back(), -140.0, 1.0e-9, "gain: down to the lowest level");
        expect(largest < 1.0e-4, "gain: deviation " + juce::String(largest));
        m_lines.add("\ngain non-linearity, 48 kHz, 996.83 Hz (65536 samples), steps of 5 dB from -5 dBFS, band 500 Hz:");
        m_lines.add("gain -6 dB: idle noise silent, 28 steps to -140 dBFS, largest deviation " + juce::String(largest, 6) + " dB");

        // dithered: the band holds the tone plus the noise of 500 Hz (q^2/4 white): deviation = 10 lg(1 + noise / tone)
        const measure::LinearityResult dithered = measure::measureGainLinearity(makeDevice([](double) { return std::make_unique<ref::Quantizer>(16, true); }), settings);
        const double step = std::pow(2.0, -15.0);
        const double binWidth = kSampleRate / dithered.windowSamples;
        const int bins = static_cast<int>(std::floor((dithered.frequencyHz + 250.0) / binWidth)) - static_cast<int>(std::ceil((dithered.frequencyHz - 250.0) / binWidth)) + 1;
        const double noise = step * step / 4.0 * bins / (dithered.windowSamples / 2.0);
        juce::String line = "16-bit quantizer, TPDF dither: idle noise " + juce::String(dithered.idleNoiseDbfs, 2) + " dBFS CCIR-RMS; deviation (expected / measured):";
        for (size_t index = 0; index < dithered.inputDbfs.size(); ++index)
        {
            const double tone = std::pow(toAmplitude(dithered.inputDbfs[index]), 2.0) / 2.0;
            const double expected = 10.0 * std::log10(1.0 + noise / tone);
            expectWithinAbsoluteError(dithered.deviationDb[index], expected, 0.02, "dithered at " + juce::String(dithered.inputDbfs[index]));
            if (index % 3 == 0 || index + 1 == dithered.inputDbfs.size())
            {
                line << " " << juce::String(dithered.inputDbfs[index], 0) << " dBFS " << juce::String(expected, 4) << "/" << juce::String(dithered.deviationDb[index], 4) << ";";
            }
        }
        expect(dithered.outputDbfs.back() < dithered.idleNoiseDbfs + 5.0, "dithered: stops within 5 dB of the idle noise");
        m_lines.add(line);

        const measure::LinearityResult plain = measure::measureGainLinearity(makeDevice([](double) { return std::make_unique<ref::Quantizer>(16, false); }), settings);
        line = "16-bit quantizer, no dither: idle silent; deviation:";
        for (size_t index = 0; index < plain.inputDbfs.size(); ++index)
        {
            const double amplitude = toAmplitude(plain.inputDbfs[index]);
            if (amplitude < step / 2.0)
            {
                // every sample rounds to zero below half a step (-96.33 dBFS)
                expect(plain.outputDbfs[index] < -300.0, "undithered below half a step at " + juce::String(plain.inputDbfs[index]));
            }
            if (plain.inputDbfs[index] >= -60.0)
            {
                expect(std::abs(plain.deviationDb[index]) < 0.01, "undithered at " + juce::String(plain.inputDbfs[index]) + ": " + juce::String(plain.deviationDb[index]));
            }
            line << " " << juce::String(plain.inputDbfs[index], 0) << " dBFS ";
            if (plain.outputDbfs[index] < -300.0)
            {
                line << "silent;";
            }
            else
            {
                line << juce::String(plain.deviationDb[index], 3) << ";";
            }
        }
        m_lines.add(line);
    }

    juce::StringArray m_lines;
};

static MeasureLinearityTests measureLinearityTests;
