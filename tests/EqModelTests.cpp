#include <cmath>
#include <complex>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/eqmodel/EqModel.h"
#include "pluginlab/reference/Analog.h"
#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/DirectFormFilter.h"
#include "pluginlab/reference/StateVariableFilter.h"

namespace
{
namespace ref = pluginlab::reference;
namespace eq = pluginlab::eqmodel;

constexpr double kPi = 3.14159265358979323846;

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-300));
}

// The impulse response of the realised band (double precision, sample by sample) and its spectrum at f: what a processor really does
std::complex<double> getRealisedResponse(const eq::Band& band, const eq::BandDesign& design, double sampleRate, double frequencyHz, int length)
{
    std::vector<double> impulse(static_cast<size_t>(length), 0.0);
    if (design.stateVariable)
    {
        ref::StateVariableFilter filter(band.type, sampleRate, design.frequencyHz, band.gainDb, band.q);
        filter.setNumChannels(1);
        double input = 1.0;
        for (int index = 0; index < length; ++index)
        {
            impulse[static_cast<size_t>(index)] = filter.processSample(0, input);
            input = 0.0;
        }
    }
    else if (! design.firTaps.empty())
    {
        for (size_t index = 0; index < design.firTaps.size() && index < impulse.size(); ++index)
        {
            impulse[index] = design.firTaps[index];
        }
    }
    else
    {
        ref::BiquadCascade cascade(design.sections, sampleRate);
        cascade.setNumChannels(1);
        double input = 1.0;
        for (int index = 0; index < length; ++index)
        {
            impulse[static_cast<size_t>(index)] = cascade.processSample(0, input);
            input = 0.0;
        }
    }
    std::complex<double> sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        sum += impulse[static_cast<size_t>(index)] * std::polar(1.0, -2.0 * kPi * frequencyHz * index / sampleRate);
    }
    return sum;
}
}

// W8.2: the EQ band model (docs/eqmodel/band-model.md)
class EqModelTests : public juce::UnitTest
{
public:
    EqModelTests()
        : juce::UnitTest("EQ band model", "pluginlab")
    {
    }

    void runTest() override
    {
        testFamilies();
        testWidths();
        testLinearPhase();
        testModel();
    }

private:
    void testFamilies()
    {
        beginTest("every family and type: the model's response equals the realised processor (impulse response, double precision) to 1e-6 dB");
        const std::vector<double> rates = {44100.0, 48000.0, 96000.0};
        const std::vector<double> frequencies = {31.5, 100.0, 315.0, 1000.0, 3150.0, 10000.0, 16000.0};
        double largest = 0.0;
        int bands = 0;
        for (const double rate : rates)
        {
            for (const eq::DesignFamily family : eq::getAllFamilies())
            {
                if (family == eq::DesignFamily::LinearPhase)
                {
                    continue; // its taps are its response (testLinearPhase)
                }
                for (int typeIndex = 0; typeIndex <= static_cast<int>(ref::FilterType::HighShelf); ++typeIndex)
                {
                    const auto type = static_cast<ref::FilterType>(typeIndex);
                    if (! eq::isTypeAvailable(family, type))
                    {
                        continue;
                    }
                    eq::Band band;
                    band.family = family;
                    band.type = type;
                    band.frequencyHz = 1200.0;
                    band.gainDb = 6.0;
                    band.q = 1.5;
                    band.order = 4;
                    const eq::BandDesign design = eq::designBand(band, rate);
                    const int length = static_cast<int>(rate); // one second: every tail has decayed
                    for (const double f : frequencies)
                    {
                        const std::complex<double> model = eq::getBandResponse(design, band, f, rate);
                        const std::complex<double> realised = getRealisedResponse(band, design, rate, f, length);
                        largest = std::max(largest, std::abs(toDb(std::abs(model)) - toDb(std::abs(realised))));
                    }
                    ++bands;
                }
            }
        }
        logMessage("model against realised processor: " + juce::String(bands) + " bands, largest difference " + juce::String(largest, 9) + " dB");
        expect(largest < 1.0e-6, juce::String(largest));
    }

    void testWidths()
    {
        beginTest("widths: Q and bandwidth in octaves (the half-gain points of the analog peak), Q and shelf slope (the cookbook's relation)");
        expectWithinAbsoluteError(eq::getBandwidthFromQ(std::sqrt(2.0)), 1.0, 1.0e-3, "Q sqrt(2) is about one octave");
        double largest = 0.0;
        for (const double q : {0.5, 0.71, 1.0, 2.0, 4.0, 10.0})
        {
            expectWithinAbsoluteError(eq::getQFromBandwidthAnalog(eq::getBandwidthFromQ(q)), q, 1.0e-12, "inverse");
            // the half-gain points (in dB) of RBJ's analog peak lie getBandwidthFromQ(q) octaves apart
            const double gain = 12.0;
            const auto halfGainDistance = [&](double lowSide)
            {
                double low = 1.0;
                double high = 1000.0;
                if (lowSide > 0.0)
                {
                    low = 1.0e-3;
                    high = 1.0;
                }
                for (int step = 0; step < 200; ++step)
                {
                    const double middle = std::sqrt(low * high);
                    const double level = toDb(std::abs(ref::getAnalogResponse(ref::FilterType::Peak, middle, 1.0, gain, q)));
                    const bool aboveHalf = level > 0.5 * gain;
                    // on the low side the level rises with f, on the high side it falls
                    if ((lowSide > 0.0) == aboveHalf)
                    {
                        high = middle;
                    }
                    else
                    {
                        low = middle;
                    }
                }
                return std::sqrt(low * high);
            };
            const double octaves = std::log2(halfGainDistance(-1.0) / halfGainDistance(1.0));
            largest = std::max(largest, std::abs(octaves - eq::getBandwidthFromQ(q)));
        }
        logMessage("bandwidth from Q against the half-gain points of the analog peak: largest difference " + juce::String(largest, 9) + " octaves");
        expect(largest < 1.0e-6, juce::String(largest));
        for (const double gain : {-12.0, 6.0, 15.0})
        {
            for (const double slope : {0.5, 1.0})
            {
                expectWithinAbsoluteError(eq::getQFromShelfSlope(slope, gain), ref::getQFromShelfSlope(slope, gain), 1.0e-12, "shelf slope as the reference");
                expectWithinAbsoluteError(eq::getShelfSlopeFromQ(eq::getQFromShelfSlope(slope, gain), gain), slope, 1.0e-12, "shelf slope inverse");
            }
        }
    }

    void testLinearPhase()
    {
        beginTest("linear-phase band: exactly linear phase, latency (taps - 1) / 2, the RBJ magnitude within a stated tolerance");
        eq::Band band;
        band.family = eq::DesignFamily::LinearPhase;
        band.type = ref::FilterType::Peak;
        band.frequencyHz = 1000.0;
        band.gainDb = 6.0;
        band.q = 2.0;
        const double rate = 48000.0;
        const eq::BandDesign design = eq::designBand(band, rate);
        expectEquals(static_cast<int>(design.firTaps.size()), 2047);
        expectEquals(design.latencySamples, 1023);
        const ref::BiquadCoefficients rbj = ref::designRbj(ref::FilterType::Peak, rate, 1000.0, 6.0, 2.0);
        double phaseError = 0.0;
        double magnitudeError = 0.0;
        for (double f = 50.0; f < 20000.0; f *= 1.05)
        {
            const std::complex<double> response = eq::getBandResponse(design, band, f, rate);
            // remove the latency: what is left is real (zero phase), positive where the magnitude is positive
            const std::complex<double> zeroPhase = response * std::polar(1.0, 2.0 * kPi * f * design.latencySamples / rate);
            phaseError = std::max(phaseError, std::abs(zeroPhase.imag()) / std::abs(zeroPhase));
            magnitudeError = std::max(magnitudeError, std::abs(toDb(std::abs(response)) - toDb(std::abs(ref::getBiquadResponse(rbj, f, rate)))));
        }
        logMessage("linear-phase peak 1 kHz +6 dB Q 2 (2047 taps): phase error " + juce::String(phaseError, 12) + " (relative imaginary part), magnitude within "
                   + juce::String(magnitudeError, 4) + " dB of RBJ from 50 Hz to 20 kHz");
        expect(phaseError < 1.0e-9, "linear phase");
        expect(magnitudeError < 0.05, "magnitude " + juce::String(magnitudeError));
        expect(! eq::isTypeAvailable(eq::DesignFamily::LinearPhase, ref::FilterType::AllPass), "no linear-phase all-pass");
    }

    void testModel()
    {
        beginTest("a model of several bands: the product of the bands and the output gain; latency the sum");
        eq::EqModel model;
        eq::Band low;
        low.type = ref::FilterType::LowShelf;
        low.frequencyHz = 200.0;
        low.gainDb = 4.0;
        eq::Band mid;
        mid.family = eq::DesignFamily::Orfanidis;
        mid.frequencyHz = 3000.0;
        mid.gainDb = -5.0;
        mid.q = 3.0;
        eq::Band high;
        high.family = eq::DesignFamily::LinearPhase;
        high.type = ref::FilterType::HighShelf;
        high.frequencyHz = 8000.0;
        high.gainDb = 2.0;
        eq::Band off;
        off.gainDb = 12.0;
        off.enabled = false;
        model.bands = {low, mid, high, off};
        model.outputGainDb = -1.5;
        const double rate = 48000.0;
        double largest = 0.0;
        for (const double f : {50.0, 500.0, 3000.0, 12000.0})
        {
            std::complex<double> product = std::pow(10.0, -1.5 / 20.0);
            for (const eq::Band& band : {low, mid, high})
            {
                product *= eq::getBandResponse(eq::designBand(band, rate), band, f, rate);
            }
            largest = std::max(largest, std::abs(eq::getModelResponse(model, f, rate) - product));
        }
        expect(largest < 1.0e-12, juce::String(largest));
        expectEquals(eq::getModelLatency(model, rate), 1023);
    }
};

static EqModelTests eqModelTests;
