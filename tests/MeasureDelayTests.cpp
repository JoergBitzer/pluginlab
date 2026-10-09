#include <cmath>
#include <complex>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Delay.h"
#include "pluginlab/reference/Delays.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/LinearPhaseFir.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;

// A chain of reference processors as one processor
class Chain : public ref::Processor
{
public:
    explicit Chain(std::vector<std::unique_ptr<ref::Processor>> stages)
        : m_stages(std::move(stages))
    {
    }

    void reset() override
    {
        for (const auto& stage : m_stages)
        {
            stage->reset();
        }
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        for (const auto& stage : m_stages)
        {
            stage->process(buffer);
        }
    }

private:
    std::vector<std::unique_ptr<ref::Processor>> m_stages;
};

struct KnownDelay
{
    juce::String name;
    std::function<std::unique_ptr<ref::Processor>(double)> factory;
    std::function<std::complex<double>(double)> exact;   // H(f), for the phase delay
    int peak;                                             // the expected peak of the impulse response
    double delay;                                         // the expected phase delay at 100 Hz (samples)
    bool inverting;
    bool checkPeak;                                       // false where the peak is not the delay (fractional delays: band-limited impulse)
};

juce::String describePolarity(bool inverting)
{
    if (inverting)
    {
        return "inverting";
    }
    return "non-inverting";
}

std::unique_ptr<ref::Processor> makeDelayLine(int samples, double sampleRate)
{
    return std::make_unique<ref::DirectFormFilter>(ref::makeIntegerDelay(samples, sampleRate));
}
}

// W7.3: delay and polarity (AES17-2015 6.8.2, 6.2.8) against devices with a known delay (docs/measurements/delay-and-polarity.md)
class MeasureDelayTests : public juce::UnitTest
{
public:
    MeasureDelayTests()
        : juce::UnitTest("Measure: delay and polarity", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("delay by impulse response (peak, interpolated, phase delay) and by cross-correlation; polarity by impulse and by the asymmetric signal");
        std::vector<KnownDelay> devices;
        for (const int samples : {0, 37, 1000})
        {
            devices.push_back({"integer delay " + juce::String(samples), [samples](double rate) { return makeDelayLine(samples, rate); },
                               [samples](double f) { return std::polar(1.0, -2.0 * kPi * f * samples / kSampleRate); }, samples, static_cast<double>(samples), false,
                               true});
        }
        const ref::DirectFormFilter thiran = ref::makeThiranDelay(37.25, 3, kSampleRate);
        devices.push_back({"Thiran 37.25 samples (order 3)", [](double rate) { return std::make_unique<ref::DirectFormFilter>(ref::makeThiranDelay(37.25, 3, rate)); },
                           [thiran](double f) { return thiran.getResponse(f); }, 37, 37.25, false, false});
        const ref::DirectFormFilter lagrange = ref::makeLagrangeDelay(37.5, 3, kSampleRate);
        devices.push_back({"Lagrange 37.5 samples (order 3)", [](double rate) { return std::make_unique<ref::DirectFormFilter>(ref::makeLagrangeDelay(37.5, 3, rate)); },
                           [lagrange](double f) { return lagrange.getResponse(f); }, 37, 37.5, false, false});
        devices.push_back({"gain -6 dB, polarity inverted", [](double) { return std::make_unique<ref::Gain>(-6.0, true); },
                           [](double) { return std::complex<double>(-0.5, 0.0); }, 0, 0.0, true, true});
        devices.push_back({"inverted, delay 100", [](double rate)
                           {
                               std::vector<std::unique_ptr<ref::Processor>> stages;
                               stages.push_back(std::make_unique<ref::Gain>(0.0, true));
                               stages.push_back(makeDelayLine(100, rate));
                               return std::make_unique<Chain>(std::move(stages));
                           },
                           [](double f) { return -std::polar(1.0, -2.0 * kPi * f * 100.0 / kSampleRate); }, 100, 100.0, true, true});
        const ref::BiquadCoefficients target = ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 1.0);
        const auto targetMagnitude = [target](double f) { return std::abs(ref::getBiquadResponse(target, f, kSampleRate)); };
        const ref::DirectFormFilter fir = ref::makeLinearPhaseFir(targetMagnitude, kSampleRate, 255);
        devices.push_back({"linear-phase FIR 255 taps (latency 127)", [targetMagnitude](double rate)
                           { return std::make_unique<ref::DirectFormFilter>(ref::makeLinearPhaseFir(targetMagnitude, rate, 255)); },
                           [fir](double f) { return fir.getResponse(f); }, 127, 127.0, false, true});
        for (const ref::FilterType type : {ref::FilterType::LowPass, ref::FilterType::HighPass})
        {
            const ref::BiquadCoefficients section = ref::designRbj(type, kSampleRate, 1000.0, 0.0, 0.71);
            // the exact peak of the impulse response and the exact phase delay at 100 Hz
            ref::BiquadCascade probe({section}, kSampleRate);
            int peak = 0;
            double largest = 0.0;
            for (int index = 0; index < 4800; ++index)
            {
                double input = 0.0;
                if (index == 0)
                {
                    input = 1.0;
                }
                const double value = probe.processSample(0, input);
                if (std::abs(value) > largest)
                {
                    largest = std::abs(value);
                    peak = index;
                }
            }
            const std::complex<double> h = ref::getBiquadResponse(section, 100.0, kSampleRate);
            const double omega = 2.0 * kPi * 100.0 / kSampleRate;
            const double phaseDelay = peak - std::arg(h * std::polar(1.0, omega * peak)) / omega;
            devices.push_back({juce::String("RBJ ") + ref::getFilterTypeName(type) + " 1 kHz Q 0.71",
                               [section](double rate) { return std::make_unique<ref::BiquadCascade>(std::vector<ref::BiquadCoefficients>{section}, rate); },
                               [section](double f) { return ref::getBiquadResponse(section, f, kSampleRate); }, peak, phaseDelay, false, true});
        }

        juce::StringArray table;
        for (const KnownDelay& device : devices)
        {
            measure::DelaySettings settings;
            settings.channels = 1;
            const measure::DelayResult result = measure::measureDelay(measure::makeProcessorDevice(device.factory), settings);
            const measure::ChannelDelay& delay = result.channels.front();
            const juce::String what = device.name;
            if (device.checkPeak)
            {
                expectEquals(delay.impulsePeakSamples, device.peak, what + ": impulse peak");
            }
            expectWithinAbsoluteError(delay.phaseDelaySamples, device.delay, 0.001, what + ": phase delay");
            expect(std::abs(delay.correlationPeakSamples - device.peak) <= 1, what + ": correlation peak " + juce::String(delay.correlationPeakSamples));
            expect(delay.invertingByImpulse == device.inverting, what + ": polarity by impulse");
            table.add("| " + device.name + " | " + juce::String(device.peak) + " / " + juce::String(device.delay, 3) + " | " + juce::String(delay.impulsePeakSamples)
                      + " | " + juce::String(delay.impulsePeakInterpolated, 3) + " | " + juce::String(delay.phaseDelaySamples, 4) + " | "
                      + juce::String(delay.correlationPeakSamples) + " | " + juce::String(delay.correlationPeakInterpolated, 3) + " | "
                      + describePolarity(delay.invertingByImpulse) + " | "
                      + describePolarity(delay.invertingByTwoTone) + " (" + juce::String(delay.twoToneRatio, 2) + ") |");
            if (device.name.startsWith("integer") || device.name.startsWith("gain") || device.name.startsWith("inverted") || device.name.startsWith("linear"))
            {
                // pure delays (and the symmetric FIR, whose magnitude hardly changes 997 Hz against 1994 Hz): the asymmetric signal decides correctly
                expect(delay.invertingByTwoTone == device.inverting, what + ": polarity by the asymmetric signal");
            }
        }
        logMessage("results for docs/measurements/delay-and-polarity.md:\n| device | expected peak / phase delay | impulse peak | interpolated | phase delay (100 Hz) | "
                   "correlation peak | interpolated | polarity (impulse) | polarity (asymmetric signal, ratio) |\n|---|---|---|---|---|---|---|---|---|\n"
                   + table.joinIntoString("\n"));
    }
};

static MeasureDelayTests measureDelayTests;
