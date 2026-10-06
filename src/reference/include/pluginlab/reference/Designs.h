#pragma once

#include <complex>
#include <vector>

#include "pluginlab/reference/Analog.h"
#include "pluginlab/reference/Biquad.h"

namespace pluginlab::reference
{
// RBJ audio EQ cookbook (R. Bristow-Johnson): the bilinear transform of getAnalogResponse() with pre-warping at the corner frequency
BiquadCoefficients designRbj(FilterType type, double sampleRate, double frequencyHz, double gainDb, double q);

// The cookbook's other ways to give the width: bandwidth in octaves (between the -3 dB points of band-pass and notch, between the half-gain points
// in dB of the peak; the warping is compensated approximately: 1 octave at fs/48 gives 1.000, at fs/4.8 0.988 octaves) and the shelf slope S (S = 1: the steepest slope without overshoot)
double getQFromBandwidth(double octaves, double frequencyHz, double sampleRate);
double getQFromShelfSlope(double slope, double gainDb);

// Orfanidis, "Digital parametric equalizer design with prescribed Nyquist-frequency gain" (JAES 45(6), 1997): the peak filter whose gain at
// Nyquist equals that of the analog prototype (no cramping). The width: Q = f0 / bandwidth, the bandwidth between the half-gain points in dB,
// so that the analog prototype is RBJ's analog peak (getAnalogResponse(FilterType::Peak, ...) at the digital frequency itself, unwarped).
// The design assumes the band lies below Nyquist (roughly f0 (1 + 1/(2Q)) < fs/2); a wider band near Nyquist is matched worse than by RBJ.
BiquadCoefficients designOrfanidisPeak(double sampleRate, double frequencyHz, double gainDb, double q);

// The filters of Zoelzer (DAFX, 2nd ed., tables 2.2-2.4): first-order shelves (all-pass form), second-order shelves (Q = 1/sqrt(2)) and the peak;
// boost and cut are mirror images (the cut is the inverse of the boost with the same corner)
enum class ZoelzerType
{
    LowShelfFirstOrder,
    HighShelfFirstOrder,
    LowShelf,
    HighShelf,
    Peak
};

const char* getZoelzerTypeName(ZoelzerType type);
BiquadCoefficients designZoelzer(ZoelzerType type, double sampleRate, double frequencyHz, double gainDb, double q);

// The analog prototypes of the Zoelzer filters at s = j f / fc, V = 10^(|gain|/20): boost low shelf (s + V) / (s + 1) and
// (s^2 + sqrt(2 V) s + V) / (s^2 + sqrt(2) s + 1), boost high shelf (V s + 1) / (s + 1) and (V s^2 + sqrt(2 V) s + 1) / (s^2 + sqrt(2) s + 1),
// boost peak (s^2 + (V/Q) s + 1) / (s^2 + s/Q + 1); a cut is 1 / (the boost).
std::complex<double> getZoelzerAnalogResponse(ZoelzerType type, double frequencyHz, double cornerHz, double gainDb, double q);

enum class Pass
{
    Low,
    High
};

// Butterworth of order 1 ... 8 (the corner at -3 dB): second-order sections with Q_k = 1 / (2 sin((2k + 1) pi / (2N))) and a first-order
// section for odd orders, by the bilinear transform with pre-warping at the corner
std::vector<BiquadCoefficients> designButterworth(Pass pass, int order, double sampleRate, double cornerHz);
std::complex<double> getAnalogButterworthResponse(Pass pass, int order, double frequencyHz, double cornerHz);

// Linkwitz-Riley of order 2, 4 or 8: the Butterworth of half the order twice (-6 dB at the corner); low-pass + (-1)^(order/2) high-pass is an all-pass
std::vector<BiquadCoefficients> designLinkwitzRiley(Pass pass, int order, double sampleRate, double cornerHz);
std::complex<double> getAnalogLinkwitzRileyResponse(Pass pass, int order, double frequencyHz, double cornerHz);
}
