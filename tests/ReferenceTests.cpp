#include <cmath>
#include <complex>
#include <functional>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/reference/Analog.h"
#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Delays.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/LinearPhaseFir.h"
#include "pluginlab/reference/StateVariableFilter.h"

namespace
{
namespace ref = pluginlab::reference;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRates[] = {44100.0, 48000.0, 96000.0};
constexpr int kImpulseLength = 1 << 17;
constexpr double kToleranceDb = 0.001;           // the plan: < 0.001 dB and < 0.01 degree
constexpr double kToleranceDegrees = 0.01;
constexpr double kSmallestCompared = 1.0e-4;     // below -80 dB the complex difference decides instead of dB and degrees
constexpr double kToleranceAbsolute = 1.0e-7;
constexpr double kTailThreshold = 1.0e-20;       // the impulse response is summed up to its last sample above this

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-300));
}

// The impulse response of a processor (double precision), cut after its last significant sample
std::vector<double> getImpulseResponse(ref::LinearProcessor& processor)
{
    processor.reset();
    std::vector<double> response(kImpulseLength);
    for (int index = 0; index < kImpulseLength; ++index)
    {
        double input = 0.0;
        if (index == 0)
        {
            input = 1.0;
        }
        response[static_cast<size_t>(index)] = processor.processSample(0, input);
    }
    size_t last = response.size();
    while (last > 1 && std::abs(response[last - 1]) < kTailThreshold)
    {
        --last;
    }
    response.resize(last);
    return response;
}

// The spectrum of an impulse response at one frequency (DTFT; the phasor is rotated and renormalised)
std::complex<double> getSpectrum(const std::vector<double>& response, double frequencyHz, double sampleRate)
{
    const std::complex<double> step = std::polar(1.0, -2.0 * kPi * frequencyHz / sampleRate);
    std::complex<double> phasor = 1.0;
    std::complex<double> sum = 0.0;
    for (size_t index = 0; index < response.size(); ++index)
    {
        sum += response[index] * phasor;
        phasor *= step;
        if (index % 1024 == 1023)
        {
            phasor = std::polar(1.0, -2.0 * kPi * frequencyHz / sampleRate * static_cast<double>(index + 1));
        }
    }
    return sum;
}

// 40 log-spaced frequencies from 20 Hz to 0.98 of Nyquist
std::vector<double> getTestFrequencies(double sampleRate)
{
    std::vector<double> frequencies;
    constexpr int kCount = 40;
    const double highest = 0.98 * sampleRate / 2.0;
    for (int index = 0; index < kCount; ++index)
    {
        frequencies.push_back(20.0 * std::pow(highest / 20.0, index / (kCount - 1.0)));
    }
    return frequencies;
}

// The largest differences between two responses
struct ResponseError
{
    double decibels = 0.0;
    double degrees = 0.0;
    double absolute = 0.0;

    void add(std::complex<double> measured, std::complex<double> expected)
    {
        if (std::abs(expected) > kSmallestCompared)
        {
            decibels = std::max(decibels, std::abs(toDb(std::abs(measured)) - toDb(std::abs(expected))));
            degrees = std::max(degrees, std::abs(std::arg(measured / expected)) * 180.0 / kPi);
        }
        else
        {
            absolute = std::max(absolute, std::abs(measured - expected));
        }
    }

    bool isWithin(double toleranceDb, double toleranceDegrees) const
    {
        return decibels < toleranceDb && degrees < toleranceDegrees && absolute < kToleranceAbsolute;
    }

    juce::String describe() const
    {
        return juce::String(decibels, 2, true) + " dB, " + juce::String(degrees, 2, true) + " degrees, " + juce::String(absolute, 2, true) + " absolute";
    }
};

// The processing against the processor's own exact response: impulse response, DTFT, compared
ResponseError measureAgainstResponse(ref::LinearProcessor& processor)
{
    const std::vector<double> response = getImpulseResponse(processor);
    ResponseError error;
    for (const double frequency : getTestFrequencies(processor.getSampleRate()))
    {
        error.add(getSpectrum(response, frequency, processor.getSampleRate()), processor.getResponse(frequency));
    }
    return error;
}

struct FilterCase
{
    double frequencyHz;
    double gainDb;
    double q;
};

constexpr FilterCase kCases[] = {{1000.0, 6.0, 0.7071}, {100.0, -12.0, 4.0}, {8000.0, 9.0, 2.0}};
constexpr ref::FilterType kTypes[] = {ref::FilterType::LowPass,  ref::FilterType::HighPass, ref::FilterType::BandPass,
                                      ref::FilterType::BandPassUnity, ref::FilterType::Notch, ref::FilterType::AllPass,
                                      ref::FilterType::Peak,     ref::FilterType::LowShelf, ref::FilterType::HighShelf};
constexpr ref::ZoelzerType kZoelzerTypes[] = {ref::ZoelzerType::LowShelfFirstOrder, ref::ZoelzerType::HighShelfFirstOrder,
                                              ref::ZoelzerType::LowShelf, ref::ZoelzerType::HighShelf, ref::ZoelzerType::Peak};

juce::String describeCase(const juce::String& name, double sampleRate, const FilterCase& filterCase)
{
    return name + " " + juce::String(filterCase.frequencyHz) + " Hz " + juce::String(filterCase.gainDb) + " dB Q " + juce::String(filterCase.q)
           + " at " + juce::String(sampleRate / 1000.0) + " kHz";
}

// The frequency between low and high where the magnitude in dB crosses a level (bisection on a log scale; the magnitude is monotonic there)
double findCrossing(const std::function<double(double)>& levelDb, double low, double high, double level)
{
    const bool risingAtLow = levelDb(low) < level;
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        const double middle = std::sqrt(low * high);
        const bool below = levelDb(middle) < level;
        if (below == risingAtLow)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return std::sqrt(low * high);
}
}

// The linear reference processors of W6.2, each against its exact response and against an independent formula
class ReferenceTests : public juce::UnitTest
{
public:
    ReferenceTests()
        : juce::UnitTest("Reference processors", "pluginlab")
    {
    }

    void runTest() override
    {
        testRbj();
        testRbjWidths();
        testOrfanidis();
        testZoelzer();
        testButterworthAndLinkwitzRiley();
        testStateVariableFilter();
        testLinearPhaseFir();
        testDelays();
        testFloatProcessing();
    }

private:
    void expectResponse(const ResponseError& error, const juce::String& what, ResponseError& worst)
    {
        expect(error.isWithin(kToleranceDb, kToleranceDegrees), what + ": " + error.describe());
        worst.decibels = std::max(worst.decibels, error.decibels);
        worst.degrees = std::max(worst.degrees, error.degrees);
        worst.absolute = std::max(worst.absolute, error.absolute);
    }

    void testRbj()
    {
        beginTest("RBJ cookbook: the processing equals H(e^jw); H(e^jw) equals the analog prototype at the pre-warped frequency; stable; 44.1/48/96 kHz");
        ResponseError worstProcessing;
        ResponseError worstDesign;
        for (const double sampleRate : kSampleRates)
        {
            for (const ref::FilterType type : kTypes)
            {
                for (const FilterCase& filterCase : kCases)
                {
                    const juce::String what = describeCase(ref::getFilterTypeName(type), sampleRate, filterCase);
                    const ref::BiquadCoefficients section = ref::designRbj(type, sampleRate, filterCase.frequencyHz, filterCase.gainDb, filterCase.q);
                    expect(ref::isStable(section), what + ": stable");
                    ref::BiquadCascade cascade({section}, sampleRate);
                    expectResponse(measureAgainstResponse(cascade), what + " processing", worstProcessing);
                    ResponseError design;
                    for (const double frequency : getTestFrequencies(sampleRate))
                    {
                        const double warped = ref::getWarpedFrequency(frequency, filterCase.frequencyHz, sampleRate);
                        design.add(cascade.getResponse(frequency),
                                   ref::getAnalogResponse(type, warped, filterCase.frequencyHz, filterCase.gainDb, filterCase.q));
                    }
                    expectResponse(design, what + " against the analog prototype", worstDesign);
                }
            }
        }
        logMessage("RBJ, largest error of the processing: " + worstProcessing.describe());
        logMessage("RBJ, largest error against the analog prototype: " + worstDesign.describe());

        beginTest("RBJ cookbook: peak gain at f0, shelf gains at DC and Nyquist, notch zero at f0, all-pass magnitude 1");
        constexpr double kSampleRate = 48000.0;
        const ref::BiquadCoefficients peak = ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 9.0, 2.0);
        expectWithinAbsoluteError(toDb(std::abs(ref::getBiquadResponse(peak, 1000.0, kSampleRate))), 9.0, 1.0e-9);
        const ref::BiquadCoefficients low = ref::designRbj(ref::FilterType::LowShelf, kSampleRate, 200.0, -6.0, 0.7071);
        expectWithinAbsoluteError(toDb(std::abs(ref::getBiquadResponse(low, 0.0, kSampleRate))), -6.0, 1.0e-9);
        expectWithinAbsoluteError(toDb(std::abs(ref::getBiquadResponse(low, 200.0, kSampleRate))), -3.0, 1.0e-9);
        const ref::BiquadCoefficients high = ref::designRbj(ref::FilterType::HighShelf, kSampleRate, 5000.0, 6.0, 0.7071);
        expectWithinAbsoluteError(toDb(std::abs(ref::getBiquadResponse(high, kSampleRate / 2.0, kSampleRate))), 6.0, 1.0e-9);
        const ref::BiquadCoefficients notch = ref::designRbj(ref::FilterType::Notch, kSampleRate, 1000.0, 0.0, 2.0);
        expect(std::abs(ref::getBiquadResponse(notch, 1000.0, kSampleRate)) < 1.0e-12);
        const ref::BiquadCoefficients allPass = ref::designRbj(ref::FilterType::AllPass, kSampleRate, 1000.0, 0.0, 2.0);
        for (const double frequency : getTestFrequencies(kSampleRate))
        {
            expectWithinAbsoluteError(std::abs(ref::getBiquadResponse(allPass, frequency, kSampleRate)), 1.0, 1.0e-12);
        }
    }

    void testRbjWidths()
    {
        beginTest("RBJ cookbook: bandwidth in octaves (between the half-gain points in dB of the peak; the warping compensation is approximate: exact at low "
                  "frequencies, about 1 % narrow at 10 kHz); shelf slope S = 1 gives Q = 1/sqrt(2)");
        for (const double sampleRate : kSampleRates)
        {
            for (const double frequency : {1000.0, 10000.0})
            {
                constexpr double kGainDb = 12.0;
                constexpr double kOctaves = 1.0;
                const double q = ref::getQFromBandwidth(kOctaves, frequency, sampleRate);
                const ref::BiquadCoefficients peak = ref::designRbj(ref::FilterType::Peak, sampleRate, frequency, kGainDb, q);
                const auto level = [&peak, sampleRate](double f)
                {
                    return toDb(std::abs(ref::getBiquadResponse(peak, f, sampleRate)));
                };
                const double lower = findCrossing(level, frequency / 8.0, frequency, kGainDb / 2.0);
                const double upper = findCrossing(level, frequency, std::min(frequency * 8.0, 0.4999 * sampleRate), kGainDb / 2.0);
                const double octaves = std::log2(upper / lower);
                logMessage("1 octave at " + juce::String(frequency) + " Hz, " + juce::String(sampleRate / 1000.0) + " kHz: Q " + juce::String(q, 4)
                           + ", measured " + juce::String(octaves, 4) + " octaves");
                double tolerance = 0.001;
                if (frequency > 5000.0)
                {
                    tolerance = 0.015;
                }
                expectWithinAbsoluteError(octaves, kOctaves, tolerance);
            }
        }
        expectWithinAbsoluteError(ref::getQFromShelfSlope(1.0, 6.0), 1.0 / std::sqrt(2.0), 1.0e-12);
        expectWithinAbsoluteError(ref::getQFromShelfSlope(1.0, -15.0), 1.0 / std::sqrt(2.0), 1.0e-12);
    }

    void testOrfanidis()
    {
        beginTest("Orfanidis: G at f0, 1 at DC, the analog gain at Nyquist; follows the analog peak closer than RBJ near Nyquist (band below Nyquist)");
        for (const double sampleRate : kSampleRates)
        {
            for (const FilterCase& filterCase : {FilterCase{10000.0, 12.0, 1.0}, FilterCase{3000.0, -9.0, 2.0}, FilterCase{15000.0, 6.0, 2.0}})
            {
                const juce::String what = describeCase("Orfanidis peak", sampleRate, filterCase);
                const ref::BiquadCoefficients orfanidis = ref::designOrfanidisPeak(sampleRate, filterCase.frequencyHz, filterCase.gainDb, filterCase.q);
                const ref::BiquadCoefficients rbj = ref::designRbj(ref::FilterType::Peak, sampleRate, filterCase.frequencyHz, filterCase.gainDb, filterCase.q);
                expect(ref::isStable(orfanidis), what + ": stable");
                const auto analog = [&filterCase](double f)
                {
                    return ref::getAnalogResponse(ref::FilterType::Peak, f, filterCase.frequencyHz, filterCase.gainDb, filterCase.q);
                };
                const double nyquist = sampleRate / 2.0;
                expectWithinAbsoluteError(toDb(std::abs(ref::getBiquadResponse(orfanidis, filterCase.frequencyHz, sampleRate))), filterCase.gainDb, 1.0e-6,
                                          what + " at f0");
                expectWithinAbsoluteError(toDb(std::abs(ref::getBiquadResponse(orfanidis, 0.0, sampleRate))), 0.0, 1.0e-9, what + " at DC");
                expectWithinAbsoluteError(toDb(std::abs(ref::getBiquadResponse(orfanidis, nyquist, sampleRate))), toDb(std::abs(analog(nyquist))), 1.0e-6,
                                          what + " at Nyquist");
                double orfanidisDeviation = 0.0;
                double rbjDeviation = 0.0;
                for (const double frequency : getTestFrequencies(sampleRate))
                {
                    const double analogDb = toDb(std::abs(analog(frequency)));
                    orfanidisDeviation = std::max(orfanidisDeviation, std::abs(toDb(std::abs(ref::getBiquadResponse(orfanidis, frequency, sampleRate))) - analogDb));
                    rbjDeviation = std::max(rbjDeviation, std::abs(toDb(std::abs(ref::getBiquadResponse(rbj, frequency, sampleRate))) - analogDb));
                }
                logMessage(what + ": largest deviation from the analog peak: Orfanidis " + juce::String(orfanidisDeviation, 3) + " dB, RBJ "
                           + juce::String(rbjDeviation, 3) + " dB");
                expect(orfanidisDeviation < rbjDeviation, what + ": closer to the analog curve than RBJ");
                ref::BiquadCascade cascade({orfanidis}, sampleRate);
                ResponseError worst;
                expectResponse(measureAgainstResponse(cascade), what + " processing", worst);
            }
        }
    }

    void testZoelzer()
    {
        beginTest("Zoelzer (DAFX): the processing equals H(e^jw); H(e^jw) equals the analog prototype at the pre-warped frequency; cut = mirror of boost");
        ResponseError worstProcessing;
        ResponseError worstDesign;
        for (const double sampleRate : kSampleRates)
        {
            for (const ref::ZoelzerType type : kZoelzerTypes)
            {
                for (const FilterCase& filterCase : kCases)
                {
                    const juce::String what = describeCase(juce::String("Zoelzer ") + ref::getZoelzerTypeName(type), sampleRate, filterCase);
                    const ref::BiquadCoefficients section = ref::designZoelzer(type, sampleRate, filterCase.frequencyHz, filterCase.gainDb, filterCase.q);
                    const ref::BiquadCoefficients mirror = ref::designZoelzer(type, sampleRate, filterCase.frequencyHz, -filterCase.gainDb, filterCase.q);
                    expect(ref::isStable(section), what + ": stable");
                    ref::BiquadCascade cascade({section}, sampleRate);
                    expectResponse(measureAgainstResponse(cascade), what + " processing", worstProcessing);
                    ResponseError design;
                    double mirrorError = 0.0;
                    for (const double frequency : getTestFrequencies(sampleRate))
                    {
                        const double warped = ref::getWarpedFrequency(frequency, filterCase.frequencyHz, sampleRate);
                        const std::complex<double> response = cascade.getResponse(frequency);
                        design.add(response, ref::getZoelzerAnalogResponse(type, warped, filterCase.frequencyHz, filterCase.gainDb, filterCase.q));
                        mirrorError = std::max(mirrorError, std::abs(toDb(std::abs(response)) + toDb(std::abs(ref::getBiquadResponse(mirror, frequency, sampleRate)))));
                    }
                    expectResponse(design, what + " against the analog prototype", worstDesign);
                    expect(mirrorError < 1.0e-9, what + ": cut = -boost in dB, " + juce::String(mirrorError));
                }
            }
        }
        logMessage("Zoelzer, largest error of the processing: " + worstProcessing.describe());
        logMessage("Zoelzer, largest error against the analog prototype: " + worstDesign.describe());
    }

    void testButterworthAndLinkwitzRiley()
    {
        beginTest("Butterworth 1st-8th order: |H|^2 = 1 / (1 + W^2N) at the pre-warped frequency, -3.01 dB at the corner, the processing equals H(e^jw)");
        ResponseError worst;
        for (const double sampleRate : kSampleRates)
        {
            for (const ref::Pass pass : {ref::Pass::Low, ref::Pass::High})
            {
                for (int order = 1; order <= 8; ++order)
                {
                    constexpr double kCorner = 1000.0;
                    juce::String passName = " LP";
                    if (pass == ref::Pass::High)
                    {
                        passName = " HP";
                    }
                    const juce::String what = "Butterworth " + juce::String(order) + passName + " at " + juce::String(sampleRate / 1000.0) + " kHz";
                    const std::vector<ref::BiquadCoefficients> sections = ref::designButterworth(pass, order, sampleRate, kCorner);
                    for (const ref::BiquadCoefficients& section : sections)
                    {
                        expect(ref::isStable(section), what + ": stable");
                    }
                    ref::BiquadCascade cascade(sections, sampleRate);
                    expectResponse(measureAgainstResponse(cascade), what + " processing", worst);
                    ResponseError design;
                    double magnitudeError = 0.0;
                    for (const double frequency : getTestFrequencies(sampleRate))
                    {
                        const double warped = ref::getWarpedFrequency(frequency, kCorner, sampleRate);
                        design.add(cascade.getResponse(frequency), ref::getAnalogButterworthResponse(pass, order, warped, kCorner));
                        double ratio = warped / kCorner;
                        if (pass == ref::Pass::High)
                        {
                            ratio = kCorner / warped;
                        }
                        const double expected = 1.0 / std::sqrt(1.0 + std::pow(ratio, 2.0 * order));
                        magnitudeError = std::max(magnitudeError, std::abs(std::abs(cascade.getResponse(frequency)) - expected));
                    }
                    expectResponse(design, what + " against the analog prototype", worst);
                    expect(magnitudeError < 1.0e-12, what + ": |H|^2 = 1 / (1 + W^2N), " + juce::String(magnitudeError));
                    expectWithinAbsoluteError(toDb(std::abs(cascade.getResponse(kCorner))), -3.0103, 1.0e-4, what + " at the corner");
                }
            }
        }

        beginTest("Linkwitz-Riley 2/4/8: -6.02 dB at the corner; LP + (-1)^(N/2) HP is an all-pass");
        for (const double sampleRate : kSampleRates)
        {
            for (const int order : {2, 4, 8})
            {
                constexpr double kCorner = 2000.0;
                const juce::String what = "Linkwitz-Riley " + juce::String(order) + " at " + juce::String(sampleRate / 1000.0) + " kHz";
                const std::vector<ref::BiquadCoefficients> low = ref::designLinkwitzRiley(ref::Pass::Low, order, sampleRate, kCorner);
                const std::vector<ref::BiquadCoefficients> high = ref::designLinkwitzRiley(ref::Pass::High, order, sampleRate, kCorner);
                expectWithinAbsoluteError(toDb(std::abs(ref::getCascadeResponse(low, kCorner, sampleRate))), -6.0206, 1.0e-4, what + " LP at the corner");
                expectWithinAbsoluteError(toDb(std::abs(ref::getCascadeResponse(high, kCorner, sampleRate))), -6.0206, 1.0e-4, what + " HP at the corner");
                double sign = 1.0;
                if ((order / 2) % 2 == 1)
                {
                    sign = -1.0;
                }
                double sumError = 0.0;
                ResponseError design;
                for (const double frequency : getTestFrequencies(sampleRate))
                {
                    const std::complex<double> sum = ref::getCascadeResponse(low, frequency, sampleRate) + sign * ref::getCascadeResponse(high, frequency, sampleRate);
                    sumError = std::max(sumError, std::abs(std::abs(sum) - 1.0));
                    const double warped = ref::getWarpedFrequency(frequency, kCorner, sampleRate);
                    design.add(ref::getCascadeResponse(low, frequency, sampleRate), ref::getAnalogLinkwitzRileyResponse(ref::Pass::Low, order, warped, kCorner));
                }
                expect(sumError < 1.0e-12, what + ": the sum is an all-pass, " + juce::String(sumError));
                expectResponse(design, what + " against the analog prototype", worst);
                ref::BiquadCascade cascade(high, sampleRate);
                expectResponse(measureAgainstResponse(cascade), what + " HP processing", worst);
            }
        }
        logMessage("Butterworth and Linkwitz-Riley, largest error: " + worst.describe());
    }

    void testStateVariableFilter()
    {
        beginTest("state-variable filter (TPT): the structure realises the RBJ transfer function of every type; 44.1/48/96 kHz");
        ResponseError worst;
        for (const double sampleRate : kSampleRates)
        {
            for (const ref::FilterType type : kTypes)
            {
                for (const FilterCase& filterCase : kCases)
                {
                    const juce::String what = describeCase(juce::String("SVF ") + ref::getFilterTypeName(type), sampleRate, filterCase);
                    ref::StateVariableFilter filter(type, sampleRate, filterCase.frequencyHz, filterCase.gainDb, filterCase.q);
                    expectResponse(measureAgainstResponse(filter), what, worst);
                }
            }
        }
        logMessage("SVF against RBJ, largest error: " + worst.describe());

        beginTest("state-variable filter (TPT): the cutoff modulated every sample (100 Hz ... 10 kHz, 200 Hz rate, Q 10) stays bounded");
        constexpr double kSampleRate = 48000.0;
        ref::StateVariableFilter filter(ref::FilterType::LowPass, kSampleRate, 1000.0, 0.0, 10.0);
        juce::Random random(1);
        double peak = 0.0;
        bool finite = true;
        for (int index = 0; index < 96000; ++index)
        {
            const double position = 0.5 + 0.5 * std::sin(2.0 * kPi * 200.0 * index / kSampleRate);
            filter.setParameters(ref::FilterType::LowPass, 100.0 * std::pow(100.0, position), 0.0, 10.0);
            const double output = filter.processSample(0, random.nextDouble() * 2.0 - 1.0);
            finite = finite && std::isfinite(output);
            peak = std::max(peak, std::abs(output));
        }
        logMessage("modulated SVF, peak of the output for noise of peak 1: " + juce::String(peak, 2));
        expect(finite && peak < 50.0);
    }

    void testLinearPhaseFir()
    {
        beginTest("linear-phase FIR: symmetric taps (exactly linear phase, latency (N-1)/2), the target magnitude within 0.05 dB, processing equals H(e^jw)");
        constexpr double kSampleRate = 48000.0;
        constexpr int kTaps = 2047;
        const ref::BiquadCoefficients peak = ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 1.0);
        const auto target = [&peak](double f)
        {
            return std::abs(ref::getBiquadResponse(peak, f, kSampleRate));
        };
        ref::DirectFormFilter fir = ref::makeLinearPhaseFir(target, kSampleRate, kTaps);
        expectEquals(fir.getLatencySamples(), (kTaps - 1) / 2);
        const std::vector<double>& taps = fir.getNumerator();
        double asymmetry = 0.0;
        for (size_t index = 0; index < taps.size(); ++index)
        {
            asymmetry = std::max(asymmetry, std::abs(taps[index] - taps[taps.size() - 1 - index]));
        }
        expectEquals(asymmetry, 0.0);
        double magnitudeError = 0.0;
        double phaseError = 0.0;
        for (const double frequency : getTestFrequencies(kSampleRate))
        {
            const std::complex<double> response = fir.getResponse(frequency);
            const std::complex<double> withoutDelay = response * std::polar(1.0, 2.0 * kPi * frequency / kSampleRate * fir.getLatencySamples());
            phaseError = std::max(phaseError, std::abs(withoutDelay.imag()) / std::abs(withoutDelay));
            if (frequency >= 100.0 && frequency <= 20000.0)
            {
                magnitudeError = std::max(magnitudeError, std::abs(toDb(std::abs(response)) - toDb(target(frequency))));
            }
        }
        logMessage("linear-phase FIR, 2047 taps: largest error against the target 100 Hz ... 20 kHz " + juce::String(magnitudeError, 4) + " dB");
        expect(phaseError < 1.0e-9, "linear phase");
        expect(magnitudeError < 0.05);
        ResponseError worst;
        expectResponse(measureAgainstResponse(fir), "linear-phase FIR processing", worst);
    }

    void testDelays()
    {
        beginTest("delays: integer delay exact; Thiran all-passes |H| = 1 and the delay at DC; Lagrange the delay and gain 1 at DC; processing equals H(e^jw)");
        ResponseError worst;
        for (const double sampleRate : kSampleRates)
        {
            ref::DirectFormFilter integer = ref::makeIntegerDelay(37, sampleRate);
            expectResponse(measureAgainstResponse(integer), "integer delay", worst);
            const std::vector<double> response = getImpulseResponse(integer);
            expectEquals(response[37], 1.0);
            for (int order = 1; order <= 4; ++order)
            {
                for (const double delay : {10.3, 10.5, 10.75})
                {
                    const juce::String suffix = " order " + juce::String(order) + ", " + juce::String(delay) + " samples";
                    ref::DirectFormFilter thiran = ref::makeThiranDelay(delay, order, sampleRate);
                    ref::DirectFormFilter lagrange = ref::makeLagrangeDelay(delay, order, sampleRate);
                    expectResponse(measureAgainstResponse(thiran), "Thiran" + suffix, worst);
                    expectResponse(measureAgainstResponse(lagrange), "Lagrange" + suffix, worst);
                    double allPassError = 0.0;
                    for (const double frequency : getTestFrequencies(sampleRate))
                    {
                        allPassError = std::max(allPassError, std::abs(std::abs(thiran.getResponse(frequency)) - 1.0));
                    }
                    expect(allPassError < 1.0e-12, "Thiran" + suffix + ": all-pass");
                    // the phase delay at 1 Hz (equal to the group delay at DC up to terms in w^2)
                    constexpr double kLowFrequency = 1.0;
                    const double omega = 2.0 * kPi * kLowFrequency / sampleRate;
                    expectWithinAbsoluteError(-std::arg(thiran.getResponse(kLowFrequency)) / omega, delay, 1.0e-6, "Thiran" + suffix + ": delay at DC");
                    expectWithinAbsoluteError(-std::arg(lagrange.getResponse(kLowFrequency)) / omega, delay, 1.0e-6, "Lagrange" + suffix + ": delay at DC");
                    expectWithinAbsoluteError(std::abs(lagrange.getResponse(0.0)), 1.0, 1.0e-12, "Lagrange" + suffix + ": gain at DC");
                    // an interpolator (D in the middle interval of the taps) never amplifies; an extrapolating one does
                    double largestGain = 0.0;
                    for (const double frequency : getTestFrequencies(sampleRate))
                    {
                        largestGain = std::max(largestGain, std::abs(lagrange.getResponse(frequency)));
                    }
                    expect(largestGain <= 1.0 + 1.0e-9, "Lagrange" + suffix + ": never above 1, largest " + juce::String(largestGain, 6));
                }
            }
        }
        logMessage("delays, largest error of the processing: " + worst.describe());
    }

    void testFloatProcessing()
    {
        beginTest("processing float buffers gives the double processing within float precision, every channel with its own state");
        constexpr double kSampleRate = 48000.0;
        const ref::BiquadCoefficients section = ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 12.0, 4.0);
        ref::BiquadCascade floatCascade({section}, kSampleRate);
        ref::BiquadCascade doubleCascade({section}, kSampleRate);
        juce::AudioBuffer<float> buffer(2, 4800);
        juce::Random random(7);
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            buffer.setSample(0, index, random.nextFloat() - 0.5f);
            buffer.setSample(1, index, 0.0f);
        }
        juce::AudioBuffer<double> reference;
        reference.makeCopyOf(buffer);
        floatCascade.process(buffer);
        doubleCascade.process(reference);
        double largest = 0.0;
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            largest = std::max(largest, std::abs(static_cast<double>(buffer.getSample(0, index)) - reference.getSample(0, index)));
        }
        expect(largest < 1.0e-6, juce::String(largest));
        expectEquals(buffer.getMagnitude(1, 0, buffer.getNumSamples()), 0.0f);
    }
};

static ReferenceTests referenceTests;
