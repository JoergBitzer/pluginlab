#include "pluginlab/eqmodel/EqModel.h"

#include <cmath>

#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/LinearPhaseFir.h"

namespace pluginlab::eqmodel
{
namespace
{
namespace ref = pluginlab::reference;

constexpr double kHighestFrequencyRatio = 0.45;   // the corner stays below 0.45 fs
constexpr int kZoelzerSecondOrder = 2;
constexpr int kLinkwitzRileyOrders[] = {2, 4, 8};
constexpr int kLinearPhaseTapsAt48k = 2047;
constexpr double kReferenceRate = 48000.0;

bool isPass(ref::FilterType type)
{
    return type == ref::FilterType::LowPass || type == ref::FilterType::HighPass;
}

ref::Pass getPass(ref::FilterType type)
{
    if (type == ref::FilterType::HighPass)
    {
        return ref::Pass::High;
    }
    return ref::Pass::Low;
}

int getLinkwitzRileyOrder(int order)
{
    for (const int allowed : kLinkwitzRileyOrders)
    {
        if (order <= allowed)
        {
            return allowed;
        }
    }
    return kLinkwitzRileyOrders[2];
}

BandDesign makeUnsupported(const juce::String& reason)
{
    BandDesign design;
    design.supported = false;
    design.description = reason + " - the audio passes unchanged";
    return design;
}

std::complex<double> getFirResponse(const std::vector<double>& taps, double frequencyHz, double sampleRate)
{
    std::complex<double> sum = 0.0;
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequencyHz / sampleRate;
    for (size_t index = 0; index < taps.size(); ++index)
    {
        sum += taps[index] * std::polar(1.0, -omega * static_cast<double>(index));
    }
    return sum;
}
}

const char* getFamilyName(DesignFamily family)
{
    switch (family)
    {
        case DesignFamily::Rbj:
            return "RBJ cookbook";
        case DesignFamily::Orfanidis:
            return "Orfanidis";
        case DesignFamily::Zoelzer:
            return "Zoelzer (DAFX)";
        case DesignFamily::StateVariable:
            return "State variable (TPT)";
        case DesignFamily::Butterworth:
            return "Butterworth";
        case DesignFamily::LinkwitzRiley:
            return "Linkwitz-Riley";
        case DesignFamily::LinearPhase:
            return "Linear phase (RBJ magnitude)";
    }
    return "";
}

std::vector<DesignFamily> getAllFamilies()
{
    return {DesignFamily::Rbj, DesignFamily::Orfanidis, DesignFamily::Zoelzer, DesignFamily::StateVariable, DesignFamily::Butterworth,
            DesignFamily::LinkwitzRiley, DesignFamily::LinearPhase};
}

bool Band::operator==(const Band& other) const
{
    return family == other.family && type == other.type && juce::exactlyEqual(frequencyHz, other.frequencyHz) && juce::exactlyEqual(gainDb, other.gainDb)
           && juce::exactlyEqual(q, other.q) && order == other.order && enabled == other.enabled;
}

bool isTypeAvailable(DesignFamily family, ref::FilterType type)
{
    switch (family)
    {
        case DesignFamily::Rbj:
        case DesignFamily::StateVariable:
            return true;
        case DesignFamily::Orfanidis:
            return type == ref::FilterType::Peak;
        case DesignFamily::Zoelzer:
            return type == ref::FilterType::Peak || type == ref::FilterType::LowShelf || type == ref::FilterType::HighShelf;
        case DesignFamily::Butterworth:
        case DesignFamily::LinkwitzRiley:
            return isPass(type);
        case DesignFamily::LinearPhase:
            return type != ref::FilterType::AllPass; // an all-pass has no magnitude to keep: with linear phase it is a pure delay
    }
    return false;
}

ref::FilterType getDefaultType(DesignFamily family)
{
    if (family == DesignFamily::Butterworth || family == DesignFamily::LinkwitzRiley)
    {
        return ref::FilterType::LowPass;
    }
    return ref::FilterType::Peak;
}

int getLinearPhaseTaps(double sampleRate)
{
    const int taps = juce::roundToInt(kLinearPhaseTapsAt48k * sampleRate / kReferenceRate);
    if (taps % 2 == 0)
    {
        return taps + 1;
    }
    return taps;
}

BandDesign designBand(const Band& band, double sampleRate)
{
    if (! band.enabled)
    {
        return makeUnsupported("band off");
    }
    const double frequency = std::min(band.frequencyHz, kHighestFrequencyRatio * sampleRate);
    const juce::String typeName(ref::getFilterTypeName(band.type));
    BandDesign design;
    design.frequencyHz = frequency;
    switch (band.family)
    {
        case DesignFamily::Rbj:
            design.sections.push_back(ref::designRbj(band.type, sampleRate, frequency, band.gainDb, band.q));
            design.description = "RBJ cookbook " + typeName;
            return design;
        case DesignFamily::Orfanidis:
            if (! isTypeAvailable(band.family, band.type))
            {
                return makeUnsupported("Orfanidis offers the peak filter only");
            }
            design.sections.push_back(ref::designOrfanidisPeak(sampleRate, frequency, band.gainDb, band.q));
            design.description = "Orfanidis peak (prescribed Nyquist gain)";
            return design;
        case DesignFamily::Zoelzer:
        {
            ref::ZoelzerType zoelzer = ref::ZoelzerType::Peak;
            if (band.type == ref::FilterType::LowShelf)
            {
                zoelzer = ref::ZoelzerType::LowShelfFirstOrder;
                if (band.order >= kZoelzerSecondOrder)
                {
                    zoelzer = ref::ZoelzerType::LowShelf;
                }
            }
            else if (band.type == ref::FilterType::HighShelf)
            {
                zoelzer = ref::ZoelzerType::HighShelfFirstOrder;
                if (band.order >= kZoelzerSecondOrder)
                {
                    zoelzer = ref::ZoelzerType::HighShelf;
                }
            }
            else if (band.type != ref::FilterType::Peak)
            {
                return makeUnsupported("Zoelzer offers shelves and the peak only");
            }
            design.sections.push_back(ref::designZoelzer(zoelzer, sampleRate, frequency, band.gainDb, band.q));
            design.description = juce::String("Zoelzer ") + ref::getZoelzerTypeName(zoelzer);
            return design;
        }
        case DesignFamily::StateVariable:
            design.stateVariable = true;
            design.description = "state-variable filter (TPT) " + typeName;
            return design;
        case DesignFamily::Butterworth:
        {
            if (! isPass(band.type))
            {
                return makeUnsupported("Butterworth offers low-pass and high-pass only");
            }
            const int order = juce::jlimit(1, 8, band.order);
            design.sections = ref::designButterworth(getPass(band.type), order, sampleRate, frequency);
            design.description = "Butterworth " + typeName + ", order " + juce::String(order);
            return design;
        }
        case DesignFamily::LinkwitzRiley:
        {
            if (! isPass(band.type))
            {
                return makeUnsupported("Linkwitz-Riley offers low-pass and high-pass only");
            }
            const int order = getLinkwitzRileyOrder(band.order);
            design.sections = ref::designLinkwitzRiley(getPass(band.type), order, sampleRate, frequency);
            design.description = "Linkwitz-Riley " + typeName + ", order " + juce::String(order);
            return design;
        }
        case DesignFamily::LinearPhase:
        {
            if (! isTypeAvailable(band.family, band.type))
            {
                return makeUnsupported("an all-pass with linear phase is a pure delay");
            }
            const ref::BiquadCoefficients rbj = ref::designRbj(band.type, sampleRate, frequency, band.gainDb, band.q);
            const int taps = getLinearPhaseTaps(sampleRate);
            design.firTaps = ref::designLinearPhaseFir([rbj, sampleRate](double f) { return std::abs(ref::getBiquadResponse(rbj, f, sampleRate)); },
                                                       sampleRate, taps);
            design.latencySamples = (taps - 1) / 2;
            design.description = "linear phase " + typeName + " (RBJ magnitude, " + juce::String(taps) + " taps, latency " + juce::String(design.latencySamples)
                                 + " samples)";
            return design;
        }
    }
    return makeUnsupported("unknown family");
}

std::complex<double> getBandResponse(const BandDesign& design, const Band& band, double frequencyHz, double sampleRate)
{
    if (! design.supported)
    {
        return 1.0;
    }
    if (design.stateVariable)
    {
        // the state-variable filter realises the RBJ transfer function of the same type (tested in ReferenceTests)
        return ref::getBiquadResponse(ref::designRbj(band.type, sampleRate, design.frequencyHz, band.gainDb, band.q), frequencyHz, sampleRate);
    }
    if (! design.firTaps.empty())
    {
        return getFirResponse(design.firTaps, frequencyHz, sampleRate);
    }
    return ref::getCascadeResponse(design.sections, frequencyHz, sampleRate);
}

std::complex<double> getModelResponse(const EqModel& model, double frequencyHz, double sampleRate)
{
    std::complex<double> response = std::pow(10.0, model.outputGainDb / 20.0);
    for (const Band& band : model.bands)
    {
        response *= getBandResponse(designBand(band, sampleRate), band, frequencyHz, sampleRate);
    }
    return response;
}

int getModelLatency(const EqModel& model, double sampleRate)
{
    int latency = 0;
    for (const Band& band : model.bands)
    {
        latency += designBand(band, sampleRate).latencySamples;
    }
    return latency;
}

double getBandwidthFromQ(double q)
{
    return 2.0 / std::log(2.0) * std::asinh(1.0 / (2.0 * q));
}

double getQFromBandwidthAnalog(double octaves)
{
    return 1.0 / (2.0 * std::sinh(std::log(2.0) / 2.0 * octaves));
}

double getShelfSlopeFromQ(double q, double gainDb)
{
    // 1/Q^2 = (A + 1/A)(1/S - 1) + 2  ->  1/S = (1/Q^2 - 2) / (A + 1/A) + 1
    const double a = std::pow(10.0, gainDb / 40.0);
    return 1.0 / ((1.0 / (q * q) - 2.0) / (a + 1.0 / a) + 1.0);
}

double getQFromShelfSlope(double slope, double gainDb)
{
    const double a = std::pow(10.0, gainDb / 40.0);
    return 1.0 / std::sqrt((a + 1.0 / a) * (1.0 / slope - 1.0) + 2.0);
}
}
