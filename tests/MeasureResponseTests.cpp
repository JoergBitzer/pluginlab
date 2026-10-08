#include <cmath>
#include <complex>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/FrequencyResponse.h"
#include "pluginlab/reference/Delays.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/LinearPhaseFir.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kPi = 3.14159265358979323846;
constexpr double kPhaseFloorDb = -60.0;   // the phase is compared only where the exact magnitude is above this
constexpr double kCompareFloorDb = -80.0;  // the magnitude is compared only above this; below, the method's own floor (dynamic range) decides
constexpr double kDeepDb = -120.0;         // where the exact response is below this, the measured level shows the method's floor

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}

// A device with its exact response
struct KnownDevice
{
    juce::String name;
    double sampleRate;
    std::function<std::unique_ptr<ref::Processor>(double)> factory;
    std::function<std::complex<double>(double)> exact;
};

KnownDevice makeCascade(const juce::String& name, double sampleRate, const std::vector<ref::BiquadCoefficients>& sections)
{
    return {name, sampleRate, [sections](double rate) { return std::make_unique<ref::BiquadCascade>(sections, rate); },
            [sections, sampleRate](double f) { return ref::getCascadeResponse(sections, f, sampleRate); }};
}

KnownDevice makeDirectForm(const juce::String& name, double sampleRate, const std::function<ref::DirectFormFilter(double)>& make)
{
    const ref::DirectFormFilter filter = make(sampleRate);
    return {name, sampleRate, [make](double rate) { return std::make_unique<ref::DirectFormFilter>(make(rate)); },
            [filter](double f) { return filter.getResponse(f); }};
}

// The largest deviations of a measured response from the exact one, in the band of validity of the method
struct Deviation
{
    double decibels = 0.0;
    double degrees = 0.0;
    double relativeDecibels = 0.0;   // of the response relative to 997 Hz
    int points = 0;
    double worstHz = 0.0;            // where the magnitude deviates most
    double floorDb = -400.0;         // the highest measured level where the exact response is below kDeepDb (the method's floor); -400: none
};

Deviation compare(const measure::FrequencyResponse& response, const KnownDevice& device)
{
    Deviation deviation;
    const std::complex<double> reference = device.exact(response.referenceHz);
    const measure::ChannelResponse& channel = response.channels.front();
    for (size_t index = 0; index < response.frequencyHz.size(); ++index)
    {
        const double f = response.frequencyHz[index];
        if (f < response.validFromHz * 0.999 || f > response.validToHz * 1.001)
        {
            continue;
        }
        const std::complex<double> exact = device.exact(f);
        const double exactDb = toDb(std::abs(exact));
        if (exactDb < kDeepDb)
        {
            deviation.floorDb = std::max(deviation.floorDb, channel.magnitudeDb[index]);
        }
        if (exactDb < kCompareFloorDb)
        {
            continue;
        }
        if (std::abs(channel.magnitudeDb[index] - exactDb) > deviation.decibels)
        {
            deviation.decibels = std::abs(channel.magnitudeDb[index] - exactDb);
            deviation.worstHz = f;
        }
        deviation.relativeDecibels = std::max(deviation.relativeDecibels, std::abs(channel.relativeDb[index] - (exactDb - toDb(std::abs(reference)))));
        if (exactDb > kPhaseFloorDb)
        {
            deviation.degrees = std::max(deviation.degrees, std::abs(std::arg(channel.response[index] / exact)) * 180.0 / kPi);
        }
        ++deviation.points;
    }
    return deviation;
}
}

namespace
{
struct Tolerance
{
    double decibels;
    double degrees;
};

// The accuracy each method reaches against the exact responses (W7.2 results, with a margin)
Tolerance getTolerance(measure::ResponseMethod method)
{
    switch (method)
    {
        case measure::ResponseMethod::SteppedSine:
            return {0.01, 0.05};
        case measure::ResponseMethod::Multitone:
            return {1.0e-4, 1.0e-3};
        case measure::ResponseMethod::SweptSine:
            return {0.01, 0.05};
    }
    return {0.0, 0.0};
}

juce::String floorText(double floorDb)
{
    if (floorDb < -399.0)
    {
        return "-";
    }
    return juce::String(floorDb, 0) + " dB";
}
}

// W7.2: the frequency response by stepped sine, multitone and synchronized sweep against the exact response of known devices
// (docs/measurements/frequency-response.md)
class MeasureResponseTests : public juce::UnitTest
{
public:
    MeasureResponseTests()
        : juce::UnitTest("Measure: frequency response", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("three methods against the exact response: RBJ peak and shelf, Butterworth 8, linear-phase FIR, integer delay; 44.1/48/96 kHz");
        const double fs = 48000.0;
        const auto peak = [](double rate) { return std::vector<ref::BiquadCoefficients>{ref::designRbj(ref::FilterType::Peak, rate, 1000.0, 6.0, 2.0)}; };
        std::vector<KnownDevice> devices;
        devices.push_back(makeCascade("RBJ peak 1 kHz +6 dB Q 2", fs, peak(fs)));
        devices.push_back(makeCascade("RBJ low shelf 200 Hz -9 dB Q 0.71", fs, {ref::designRbj(ref::FilterType::LowShelf, fs, 200.0, -9.0, 0.71)}));
        devices.push_back(makeCascade("Butterworth low-pass 8th order 1 kHz", fs, ref::designButterworth(ref::Pass::Low, 8, fs, 1000.0)));
        const ref::BiquadCoefficients target = ref::designRbj(ref::FilterType::Peak, fs, 1000.0, 6.0, 1.0);
        devices.push_back(makeDirectForm("linear-phase FIR 2047 taps (latency 1023)", fs, [target](double rate)
                                         { return ref::makeLinearPhaseFir([target, rate](double f) { return std::abs(ref::getBiquadResponse(target, f, rate)); }, rate, 2047); }));
        devices.push_back(makeDirectForm("integer delay 100 samples", fs, [](double rate) { return ref::makeIntegerDelay(100, rate); }));
        devices.push_back(makeCascade("RBJ peak 1 kHz +6 dB Q 2, 44.1 kHz", 44100.0, peak(44100.0)));
        devices.push_back(makeCascade("RBJ peak 1 kHz +6 dB Q 2, 96 kHz", 96000.0, peak(96000.0)));

        juce::StringArray table;
        for (const KnownDevice& device : devices)
        {
            const measure::Device dut = measure::makeProcessorDevice(device.factory);
            measure::SteppedResponseSettings stepped;
            stepped.sampleRate = device.sampleRate;
            stepped.channels = 1;
            measure::MultitoneResponseSettings multitone;
            multitone.sampleRate = device.sampleRate;
            multitone.channels = 1;
            measure::SweepResponseSettings sweep;
            sweep.sampleRate = device.sampleRate;
            sweep.channels = 1;
            const measure::FrequencyResponse responses[] = {measure::measureSteppedResponse(dut, stepped), measure::measureMultitoneResponse(dut, multitone),
                                                            measure::measureSweptResponse(dut, sweep)};
            for (const measure::FrequencyResponse& response : responses)
            {
                const Deviation deviation = compare(response, device);
                table.add("| " + device.name + " | " + getResponseMethodName(response.method) + " | " + juce::String(deviation.points) + " | "
                          + juce::String(deviation.decibels, 2, true) + " | " + juce::String(deviation.relativeDecibels, 2, true) + " | "
                          + juce::String(deviation.degrees, 2, true) + " | " + floorText(deviation.floorDb) + " |");
                const Tolerance tolerance = getTolerance(response.method);
                expect(deviation.decibels < tolerance.decibels && deviation.degrees < tolerance.degrees,
                       device.name + ", " + getResponseMethodName(response.method) + ": " + juce::String(deviation.decibels, 2, true) + " dB, "
                           + juce::String(deviation.degrees, 2, true) + " degrees at worst (" + juce::String(deviation.worstHz, 1) + " Hz)");
            }
        }
        logMessage("results for docs/measurements/frequency-response.md:\n| device | method | points | magnitude (dB) | re 997 Hz (dB) | phase (degrees) | floor |\n"
                   "|---|---|---|---|---|---|---|\n" + table.joinIntoString("\n"));
    }
};

static MeasureResponseTests measureResponseTests;
