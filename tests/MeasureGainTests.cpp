#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Gain.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kPi = 3.14159265358979323846;

double toDb(double ratio)
{
    return 20.0 * std::log10(ratio);
}

// A device whose right channel is a little quieter: gain matching between channels
class ChannelOffset : public ref::Processor
{
public:
    explicit ChannelOffset(double rightDb)
        : m_right(std::pow(10.0, rightDb / 20.0))
    {
    }

    void reset() override
    {
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        if (buffer.getNumChannels() > 1)
        {
            buffer.applyGain(1, 0, buffer.getNumSamples(), static_cast<float>(m_right));
        }
    }

private:
    double m_right;
};
}

// W7.1: the analyzer of AES17 (level meter, standard low-pass filter) and the unit "level and gain" (docs/measurements/level-and-gain.md)
class MeasureGainTests : public juce::UnitTest
{
public:
    MeasureGainTests()
        : juce::UnitTest("Measure: level and gain", "pluginlab")
    {
    }

    void runTest() override
    {
        testDbfs();
        testStandardLowPass();
        testGainOfKnownDevices();
        testNonlinearAndNoisyDevices();
        logResults();
    }

private:
    void testDbfs()
    {
        beginTest("dBFS after AES17 3.12: a full-scale sine is 0 dBFS, a full-scale square wave +3.01 dBFS; tone amplitude and rms of whole periods");
        constexpr int kLength = 48000;
        std::vector<double> sine(kLength);
        std::vector<double> square(kLength);
        for (int index = 0; index < kLength; ++index)
        {
            sine[static_cast<size_t>(index)] = std::sin(2.0 * kPi * 997.0 * index / 48000.0);
            square[static_cast<size_t>(index)] = 1.0;
            if (sine[static_cast<size_t>(index)] < 0.0)
            {
                square[static_cast<size_t>(index)] = -1.0;
            }
        }
        expectWithinAbsoluteError(measure::rmsToDbfs(measure::getRms(sine, 0, kLength)), 0.0, 1.0e-9);
        expectWithinAbsoluteError(measure::rmsToDbfs(measure::getRms(square, 0, kLength)), 3.0103, 1.0e-3);
        expectWithinAbsoluteError(std::abs(measure::getToneAmplitude(sine, 0, kLength, 997.0, 48000.0)), 1.0, 1.0e-9);
        expectEquals(measure::getWholePeriodLength(997.0, 48000.0, 1.0), 48000);
        expect(measure::getWholePeriodLength(20.0, 48000.0, 0.0) >= static_cast<int>(0.025 * 48000.0), "at least 25 ms");
    }

    void testStandardLowPass()
    {
        beginTest("AES17 5.2.5 standard low-pass: identity at 44.1/48 kHz; at 88.2/96/192 kHz +-0.1 dB from 20 Hz to 20 kHz and >= 60 dB above 24 kHz");
        expect(measure::StandardLowPass(44100.0).isIdentity());
        expect(measure::StandardLowPass(48000.0).isIdentity());
        for (const double sampleRate : {88200.0, 96000.0, 192000.0})
        {
            const measure::StandardLowPass lowPass(sampleRate);
            expect(!lowPass.isIdentity());
            double ripple = 0.0;
            for (double f = 20.0; f <= 20000.0; f *= 1.02)
            {
                ripple = std::max(ripple, std::abs(toDb(std::abs(lowPass.getResponse(f)))));
            }
            ripple = std::max(ripple, std::abs(toDb(std::abs(lowPass.getResponse(20000.0)))));
            double stopBand = -1000.0;
            for (double f = 24000.0; f < sampleRate / 2.0; f += 25.0)
            {
                stopBand = std::max(stopBand, toDb(std::abs(lowPass.getResponse(f))));
            }
            logMessage("standard low-pass at " + juce::String(sampleRate / 1000.0) + " kHz: " + juce::String(lowPass.getTaps()) + " taps, passband ripple "
                       + juce::String(ripple, 4) + " dB, stop band at most " + juce::String(stopBand, 1) + " dB");
            expect(ripple <= 0.1, "passband");
            expect(stopBand <= -60.0, "stop band");
        }
    }

    measure::GainResult measureProcessor(const std::function<std::unique_ptr<ref::Processor>(double)>& factory, double sampleRate = 48000.0,
                                         double levelDbfs = -20.0)
    {
        measure::GainSettings settings;
        settings.sampleRate = sampleRate;
        settings.levelDbfs = levelDbfs;
        return measure::measureGain(measure::makeProcessorDevice(factory), settings);
    }

    void testGainOfKnownDevices()
    {
        beginTest("gain of known devices: -6 dB, polarity inverted, at 44.1/48/96 kHz; an RBJ peak equals |H(997 Hz)| and its phase; a channel 0.5 dB lower");
        for (const double sampleRate : {44100.0, 48000.0, 96000.0})
        {
            const measure::GainResult gain = measureProcessor([](double) { return std::make_unique<ref::Gain>(-6.0, false); }, sampleRate);
            for (const measure::ChannelGain& channel : gain.channels)
            {
                expectWithinAbsoluteError(channel.inputLevelDbfs, -20.0, 1.0e-4, "input level");
                expectWithinAbsoluteError(channel.gainDb, -6.0, 1.0e-4, "gain at " + juce::String(sampleRate));
                expectWithinAbsoluteError(channel.selectiveGainDb, -6.0, 1.0e-4);
                expectWithinAbsoluteError(channel.phaseDegrees, 0.0, 1.0e-4);
            }
            expectWithinAbsoluteError(gain.matchingDb, 0.0, 1.0e-9);
            m_lines.add("| gain -6 dB, " + juce::String(sampleRate / 1000.0) + " kHz | -6 dB, 0 degrees | " + juce::String(gain.channels[0].gainDb, 5) + " dB, "
                        + juce::String(gain.channels[0].phaseDegrees, 4) + " degrees |");
        }
        const measure::GainResult inverted = measureProcessor([](double) { return std::make_unique<ref::Gain>(-6.0, true); });
        expectWithinAbsoluteError(inverted.channels[0].gainDb, -6.0, 1.0e-4);
        expectWithinAbsoluteError(std::abs(inverted.channels[0].phaseDegrees), 180.0, 1.0e-4);
        m_lines.add("| gain -6 dB, polarity inverted | -6 dB, 180 degrees | " + juce::String(inverted.channels[0].gainDb, 5) + " dB, "
                    + juce::String(inverted.channels[0].phaseDegrees, 4) + " degrees |");

        const ref::BiquadCoefficients peak = ref::designRbj(ref::FilterType::Peak, 48000.0, 1000.0, 6.0, 2.0);
        const std::complex<double> exact = ref::getBiquadResponse(peak, measure::kStandardFrequencyHz, 48000.0);
        const measure::GainResult filtered = measureProcessor([peak](double sampleRate)
                                                              { return std::make_unique<ref::BiquadCascade>(std::vector<ref::BiquadCoefficients>{peak}, sampleRate); });
        expectWithinAbsoluteError(filtered.channels[0].gainDb, toDb(std::abs(exact)), 1.0e-4);
        expectWithinAbsoluteError(filtered.channels[0].selectiveGainDb, toDb(std::abs(exact)), 1.0e-4);
        expectWithinAbsoluteError(filtered.channels[0].phaseDegrees, std::arg(exact) * 180.0 / kPi, 1.0e-3);
        m_lines.add("| RBJ peak 1 kHz +6 dB Q 2 | |H(997 Hz)| = " + juce::String(toDb(std::abs(exact)), 5) + " dB, " + juce::String(std::arg(exact) * 180.0 / kPi, 4)
                    + " degrees | " + juce::String(filtered.channels[0].gainDb, 5) + " dB, " + juce::String(filtered.channels[0].phaseDegrees, 4) + " degrees |");

        const measure::GainResult offset = measureProcessor([](double) { return std::make_unique<ChannelOffset>(-0.5); });
        expectWithinAbsoluteError(offset.matchingDb, 0.5, 1.0e-4);
        m_lines.add("| right channel -0.5 dB (gain matching) | 0.5 dB | " + juce::String(offset.matchingDb, 5) + " dB |");
    }

    void testNonlinearAndNoisyDevices()
    {
        beginTest("a distorting and a noisy device: the broadband gain counts harmonics and noise, the selective gain only the tone (closed forms)");
        // y = x + 0.1 x^2 + 0.05 x^3 at -20 dBFS (A = 0.1): tone c1 A + 3/4 c3 A^3, DC c2 A^2 / 2, harmonics c2 A^2 / 2 and c3 A^3 / 4
        const std::vector<double> coefficients = {0.0, 1.0, 0.1, 0.05};
        const double amplitude = std::pow(10.0, -20.0 / 20.0);
        const std::vector<double> harmonics = ref::getPolynomialHarmonics(coefficients, amplitude, 3);
        double outputPower = harmonics[0] * harmonics[0];
        for (size_t harmonic = 1; harmonic < harmonics.size(); ++harmonic)
        {
            outputPower += harmonics[harmonic] * harmonics[harmonic] / 2.0;
        }
        const double expectedBroadband = toDb(std::sqrt(outputPower) / (amplitude / std::sqrt(2.0)));
        const double expectedSelective = toDb(harmonics[1] / amplitude);
        const measure::GainResult shaped = measureProcessor([coefficients](double)
                                                            { return std::make_unique<ref::Waveshaper>(ref::Waveshaper::makePolynomial(coefficients)); });
        expectWithinAbsoluteError(shaped.channels[0].gainDb, expectedBroadband, 1.0e-4);
        expectWithinAbsoluteError(shaped.channels[0].selectiveGainDb, expectedSelective, 1.0e-4);
        m_lines.add("| x + 0.1 x^2 + 0.05 x^3, -20 dBFS | broadband " + juce::String(expectedBroadband, 5) + " dB, selective " + juce::String(expectedSelective, 5)
                    + " dB | " + juce::String(shaped.channels[0].gainDb, 5) + " dB, " + juce::String(shaped.channels[0].selectiveGainDb, 5) + " dB |");

        // 8-bit quantizer with TPDF dither: the error is independent of the signal and adds q^2/12 + q^2/6 = q^2/4 to the output power, the tone stays
        const double step = std::pow(2.0, -7.0);
        const double signalPower = amplitude * amplitude / 2.0;
        const double expectedNoisy = 10.0 * std::log10((signalPower + step * step / 4.0) / signalPower);
        const measure::GainResult dithered = measureProcessor([](double) { return std::make_unique<ref::Quantizer>(8, true); });
        expectWithinAbsoluteError(dithered.channels[0].gainDb, expectedNoisy, 0.002);
        expectWithinAbsoluteError(dithered.channels[0].selectiveGainDb, 0.0, 0.002);
        m_lines.add("| 8-bit quantizer with TPDF dither, -20 dBFS | broadband +" + juce::String(expectedNoisy, 4) + " dB (q^2/4), selective 0 dB | "
                    + juce::String(dithered.channels[0].gainDb, 5) + " dB, " + juce::String(dithered.channels[0].selectiveGainDb, 5) + " dB |");
        // without dither the error repeats with the sine and contains a part at the fundamental: even the selective gain moves (an observation)
        const measure::GainResult plain = measureProcessor([](double) { return std::make_unique<ref::Quantizer>(8, false); });
        expect(std::abs(plain.channels[0].selectiveGainDb) > 0.01, "undithered: the error is correlated with the signal");
        m_lines.add("| 8-bit quantizer without dither, -20 dBFS | (no closed form: the error is correlated with the sine) | " + juce::String(plain.channels[0].gainDb, 5)
                    + " dB, " + juce::String(plain.channels[0].selectiveGainDb, 5) + " dB |");
    }

    void logResults()
    {
        logMessage("results for docs/measurements/level-and-gain.md:\n| device | expected | measured |\n|---|---|---|\n" + m_lines.joinIntoString("\n"));
    }

    juce::StringArray m_lines;
};

static MeasureGainTests measureGainTests;
