#pragma once

#include <complex>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/reference/Analog.h"
#include "pluginlab/reference/Biquad.h"

namespace pluginlab::eqmodel
{
// The EQ band model (W8.2, docs/eqmodel/band-model.md): a band is a filter type, a frequency, a gain, a width (Q) and an order, realised by a
// design family. The model computes the exact response of every family that pluginlab_reference implements; the Reference EQ plugin uses the same
// designs, so model and plugin cannot drift apart.

enum class DesignFamily
{
    Rbj,             // R. Bristow-Johnson's cookbook (all types)
    Orfanidis,       // the peak with prescribed Nyquist gain
    Zoelzer,         // DAFX shelves (1st and 2nd order) and the peak
    StateVariable,   // TPT state-variable filter: the RBJ transfer function, modulation-safe
    Butterworth,     // low-pass and high-pass, order 1 ... 8
    LinkwitzRiley,   // low-pass and high-pass, order 2, 4, 8
    LinearPhase      // the magnitude of the RBJ design with linear phase (a symmetric FIR, latency (taps - 1) / 2)
};

const char* getFamilyName(DesignFamily family);
std::vector<DesignFamily> getAllFamilies();

struct Band
{
    DesignFamily family = DesignFamily::Rbj;
    reference::FilterType type = reference::FilterType::Peak;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 0.71;
    int order = 2;                 // Butterworth, Linkwitz-Riley, Zoelzer shelves
    bool enabled = true;

    bool operator==(const Band& other) const;
};

// Which types a family offers, and the type to fall back to
bool isTypeAvailable(DesignFamily family, reference::FilterType type);
reference::FilterType getDefaultType(DesignFamily family);

// The design of one band at a sample rate
struct BandDesign
{
    bool supported = true;          // false: the family does not offer the type (the band passes the audio unchanged)
    bool stateVariable = false;     // realised by the state-variable structure (its transfer function: the RBJ design)
    double frequencyHz = 1000.0;    // after the limit to 0.45 fs
    std::vector<reference::BiquadCoefficients> sections;
    std::vector<double> firTaps;    // linear phase
    int latencySamples = 0;
    juce::String description;
};

BandDesign designBand(const Band& band, double sampleRate);

// The exact response of a band (1 for a disabled or unsupported band)
std::complex<double> getBandResponse(const BandDesign& design, const Band& band, double frequencyHz, double sampleRate);

// The linear-phase FIR length at a sample rate: 2047 taps at 48 kHz (latency 1023 samples, 21 ms), scaled with the rate (odd)
int getLinearPhaseTaps(double sampleRate);

// An EQ: bands in series and an output gain
struct EqModel
{
    std::vector<Band> bands;
    double outputGainDb = 0.0;
};

std::complex<double> getModelResponse(const EqModel& model, double frequencyHz, double sampleRate);
int getModelLatency(const EqModel& model, double sampleRate);

// Widths. RBJ's Q is defined on the analog prototype: bandwidth in octaves between the -3 dB points of a band-pass (half-gain points in dB of a
// peak) BW = (2 / ln 2) asinh(1 / (2 Q)), and the shelf slope S with 1/Q = sqrt((A + 1/A)(1/S - 1) + 2), A = 10^(gain/40). These are the analog
// relations; reference::getQFromBandwidth also compensates the warping of the digital design.
double getBandwidthFromQ(double q);
double getQFromBandwidthAnalog(double octaves);
double getShelfSlopeFromQ(double q, double gainDb);
double getQFromShelfSlope(double slope, double gainDb);
}
