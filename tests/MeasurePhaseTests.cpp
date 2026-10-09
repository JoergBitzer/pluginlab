#include <cmath>
#include <complex>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Phase.h"
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
constexpr double kStepHz = 0.01;           // for the exact group delay by a central difference of the exact phase
constexpr double kCompareFloorDb = -60.0;  // the phase is compared only where the exact magnitude is above this
constexpr double kGroupDelayFloorDb = -40.0; // the group delay only above this (the passband of the unit; below, float precision limits the derivative)
constexpr double kLowBandHz = 50.0;        // the group delay error is reported below and above this (the window limits the lowest frequencies)

// Different processors on the two channels (for the inter-channel phase)
class PerChannel : public ref::Processor
{
public:
    PerChannel(std::unique_ptr<ref::Processor> left, std::unique_ptr<ref::Processor> right)
        : m_left(std::move(left)), m_right(std::move(right))
    {
    }

    void reset() override
    {
        m_left->reset();
        m_right->reset();
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        juce::AudioBuffer<float> left(buffer.getArrayOfWritePointers(), 1, buffer.getNumSamples());
        m_left->process(left);
        if (buffer.getNumChannels() > 1)
        {
            juce::AudioBuffer<float> right(buffer.getArrayOfWritePointers() + 1, 1, buffer.getNumSamples());
            m_right->process(right);
        }
    }

private:
    std::unique_ptr<ref::Processor> m_left;
    std::unique_ptr<ref::Processor> m_right;
};

struct KnownPhase
{
    juce::String name;
    std::function<std::unique_ptr<ref::Processor>(double)> factory;
    std::function<std::complex<double>(double)> exact;
};

double exactGroupDelay(const std::function<std::complex<double>(double)>& h, double f)
{
    return -std::arg(h(f + kStepHz) / h(f - kStepHz)) / (2.0 * kPi * 2.0 * kStepHz / kSampleRate);
}

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-30));
}

KnownPhase makeCascade(const juce::String& name, const std::vector<ref::BiquadCoefficients>& sections)
{
    return {name, [sections](double rate) { return std::make_unique<ref::BiquadCascade>(sections, rate); },
            [sections](double f) { return ref::getCascadeResponse(sections, f, kSampleRate); }};
}

KnownPhase makeDirectForm(const juce::String& name, const std::function<ref::DirectFormFilter(double)>& make)
{
    const ref::DirectFormFilter filter = make(kSampleRate);
    return {name, [make](double rate) { return std::make_unique<ref::DirectFormFilter>(make(rate)); }, [filter](double f) { return filter.getResponse(f); }};
}
}

// W7.4: phase response and group delay (AES17-2015 6.8.3, 6.8.4, 6.2.7) against devices with an exact response
// (docs/measurements/phase-and-group-delay.md)
class MeasurePhaseTests : public juce::UnitTest
{
public:
    MeasurePhaseTests()
        : juce::UnitTest("Measure: phase and group delay", "pluginlab")
    {
    }

    void runTest() override
    {
        testKnownDevices();
        testInterChannel();
    }

private:
    void testKnownDevices()
    {
        beginTest("phase (delay removed), deviation from linear, exact group delay and AES17's difference group delay against the exact response");
        std::vector<KnownPhase> devices;
        devices.push_back(makeCascade("RBJ all-pass 1 kHz Q 4", {ref::designRbj(ref::FilterType::AllPass, kSampleRate, 1000.0, 0.0, 4.0)}));
        devices.push_back(makeCascade("RBJ peak 1 kHz +6 dB Q 2", {ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 2.0)}));
        devices.push_back(makeCascade("Butterworth low-pass 4th order 1 kHz", ref::designButterworth(ref::Pass::Low, 4, kSampleRate, 1000.0)));
        const ref::BiquadCoefficients target = ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 1.0);
        devices.push_back(makeDirectForm("linear-phase FIR 255 taps (latency 127)", [target](double rate)
                                         { return ref::makeLinearPhaseFir([target, rate](double f) { return std::abs(ref::getBiquadResponse(target, f, rate)); }, rate, 255); }));
        devices.push_back(makeDirectForm("Thiran 37.25 samples (order 3)", [](double rate) { return ref::makeThiranDelay(37.25, 3, rate); }));
        devices.push_back({"inverted, delay 100", [](double rate)
                           { return std::make_unique<PerChannel>(std::make_unique<ref::DirectFormFilter>(ref::DirectFormFilter({-1.0}, {1.0}, rate, 100)),
                                                                 std::make_unique<ref::DirectFormFilter>(ref::DirectFormFilter({-1.0}, {1.0}, rate, 100))); },
                           [](double f) { return -std::polar(1.0, -2.0 * kPi * f * 100.0 / kSampleRate); }});

        juce::StringArray table;
        for (const KnownPhase& device : devices)
        {
            measure::PhaseSettings settings;
            settings.channels = 1;
            const measure::PhaseResponse result = measure::measurePhaseResponse(measure::makeProcessorDevice(device.factory), settings);
            const measure::ChannelPhase& phase = result.channels.front();
            double phaseError = 0.0;
            double groupDelayError = 0.0;
            double differenceError = 0.0;
            double worstHz = 0.0;
            double lowBandError = 0.0;
            for (size_t index = 0; index < result.frequencyHz.size(); ++index)
            {
                const double f = result.frequencyHz[index];
                const std::complex<double> exact = device.exact(f);
                if (f < result.validFromHz || f > result.validToHz || toDb(std::abs(exact)) < kCompareFloorDb)
                {
                    continue;
                }
                const double exactDegrees = std::arg(exact * std::polar(1.0, 2.0 * kPi * f * phase.delaySamples / kSampleRate)) * 180.0 / kPi;
                phaseError = std::max(phaseError, std::abs(std::remainder(phase.phaseDegrees[index] - exactDegrees, 360.0)));
                if (toDb(std::abs(exact)) < kGroupDelayFloorDb)
                {
                    continue;
                }
                const double error = std::abs(phase.groupDelaySamples[index] - exactGroupDelay(device.exact, f));
                if (f < kLowBandHz)
                {
                    lowBandError = std::max(lowBandError, error);
                }
                else if (error > groupDelayError)
                {
                    groupDelayError = error;
                    worstHz = f;
                }
            }
            for (size_t index = 0; index < result.differenceFrequencyHz.size(); ++index)
            {
                const double f = result.differenceFrequencyHz[index];
                if (f < result.validFromHz || f > result.validToHz || toDb(std::abs(device.exact(f))) < kGroupDelayFloorDb)
                {
                    continue;
                }
                differenceError = std::max(differenceError, std::abs(phase.groupDelayDifferenceSamples[index] - exactGroupDelay(device.exact, f)));
            }
            expect(phaseError < 0.05, device.name + ": phase " + juce::String(phaseError, 4) + " degrees");
            // in time: below 50 Hz within 10 microseconds (0.48 samples), above within 1 microsecond (0.048 samples)
            expect(lowBandError < 0.48, device.name + ": group delay below 50 Hz " + juce::String(lowBandError, 4) + " samples");
            expect(groupDelayError < 0.048, device.name + ": group delay above 50 Hz " + juce::String(groupDelayError, 4) + " samples");
            table.add("| " + device.name + " | " + juce::String(phase.delaySamples) + " | " + juce::String(phase.fitDelaySamples, 3) + " | +"
                      + juce::String(phase.deviationMaxDegrees, 3) + "/" + juce::String(phase.deviationMinDegrees, 3) + " | " + juce::String(phaseError, 2, true)
                        + " | " + juce::String(lowBandError, 2, true) + " | " + juce::String(groupDelayError, 2, true) + " (" + juce::String(worstHz, 0) + " Hz) | " + juce::String(differenceError, 2, true) + " |");
            if (device.name.startsWith("linear"))
            {
                expectWithinAbsoluteError(phase.fitDelaySamples, 127.0, 0.001, "linear phase: the fitted delay");
                expect(std::max(std::abs(phase.deviationMaxDegrees), std::abs(phase.deviationMinDegrees)) < 0.01, "linear phase: no deviation");
            }
        }
        logMessage("results for docs/measurements/phase-and-group-delay.md:\n| device | delay removed (peak) | fitted delay | deviation from linear (degrees) | "
                   "phase error (degrees) | group delay error below 50 Hz (samples) | above 50 Hz (samples, where) | difference method error (samples) |\n|---|---|---|---|---|---|---|---|\n"
                   + table.joinIntoString("\n"));
    }

    void testInterChannel()
    {
        beginTest("inter-channel phase (AES17 6.2.7): the right channel one sample later, or through an all-pass");
        const ref::BiquadCoefficients allPass = ref::designRbj(ref::FilterType::AllPass, kSampleRate, 1000.0, 0.0, 0.71);
        struct Case
        {
            juce::String name;
            std::function<std::unique_ptr<ref::Processor>(double)> factory;
            std::function<std::complex<double>(double)> exact;
        };
        const Case cases[] = {
            {"right channel delayed by 1 sample", [](double rate)
             { return std::make_unique<PerChannel>(std::make_unique<ref::Gain>(0.0, false), std::make_unique<ref::DirectFormFilter>(ref::makeIntegerDelay(1, rate))); },
             [](double f) { return std::polar(1.0, -2.0 * kPi * f / kSampleRate); }},
            {"right channel through an RBJ all-pass 1 kHz", [allPass](double rate)
             { return std::make_unique<PerChannel>(std::make_unique<ref::Gain>(0.0, false),
                                                   std::make_unique<ref::BiquadCascade>(std::vector<ref::BiquadCoefficients>{allPass}, rate)); },
             [allPass](double f) { return ref::getBiquadResponse(allPass, f, kSampleRate); }}};
        for (const Case& testCase : cases)
        {
            measure::PhaseSettings settings;
            const measure::PhaseResponse result = measure::measurePhaseResponse(measure::makeProcessorDevice(testCase.factory), settings);
            double largest = 0.0;
            for (size_t index = 0; index < result.frequencyHz.size(); ++index)
            {
                const double f = result.frequencyHz[index];
                const double exact = std::arg(testCase.exact(f)) * 180.0 / kPi;
                largest = std::max(largest, std::abs(std::remainder(result.channels[1].interChannelDegrees[index] - exact, 360.0)));
                expectWithinAbsoluteError(result.channels[0].interChannelDegrees[index], 0.0, 1.0e-9);
            }
            logMessage(testCase.name + ": inter-channel phase within " + juce::String(largest, 2, true) + " degrees of the exact one");
            expect(largest < 0.05, testCase.name);
        }
    }
};

static MeasurePhaseTests measurePhaseTests;
