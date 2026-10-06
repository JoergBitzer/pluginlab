#pragma once

#include <complex>

namespace pluginlab::reference
{
// The filter types of the RBJ cookbook (and of the state-variable filter, which realises the same transfer functions)
enum class FilterType
{
    LowPass,
    HighPass,
    BandPass,       // constant skirt gain: the peak gain is Q
    BandPassUnity,  // 0 dB peak gain
    Notch,
    AllPass,
    Peak,
    LowShelf,
    HighShelf
};

const char* getFilterTypeName(FilterType type);

// The analog prototypes of the RBJ cookbook at s = j f / f0, without discretisation (gain in dB for Peak and the shelves, A = 10^(gain/40)):
// low-pass 1 / (s^2 + s/Q + 1), high-pass s^2 / (...), band-pass s / (...), unity band-pass (s/Q) / (...), notch (s^2 + 1) / (...),
// all-pass (s^2 - s/Q + 1) / (...), peak (s^2 + s A/Q + 1) / (s^2 + s/(A Q) + 1),
// low shelf A (s^2 + s sqrt(A)/Q + A) / (A s^2 + s sqrt(A)/Q + 1), high shelf A (A s^2 + s sqrt(A)/Q + 1) / (s^2 + s sqrt(A)/Q + A).
std::complex<double> getAnalogResponse(FilterType type, double frequencyHz, double cornerHz, double gainDb, double q);

// The analog frequency the bilinear transform with pre-warping at the corner maps a digital frequency to: f0 tan(pi f / fs) / tan(pi f0 / fs).
// A digital design by the bilinear transform has at f exactly the response of its analog prototype at this frequency.
double getWarpedFrequency(double frequencyHz, double cornerHz, double sampleRate);
}
