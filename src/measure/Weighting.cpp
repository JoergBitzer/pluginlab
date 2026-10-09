#include "pluginlab/measure/Weighting.h"

#include <cmath>

namespace pluginlab::measure
{
namespace
{
constexpr double kCcirRmsOffsetDb = -5.63;       // AES17 5.2.7

// The magnitude of the BS.468-4 network up to a constant: f / |h1 + j h2| with the polynomials of the network's transfer function
double getBs468Shape(double f)
{
    const double h1 = -4.737338981378384e-24 * std::pow(f, 6.0) + 2.043828333606125e-15 * std::pow(f, 4.0) - 1.363894795463638e-07 * f * f + 1.0;
    const double h2 = 1.306612257412824e-19 * std::pow(f, 5.0) - 2.118150887518656e-11 * std::pow(f, 3.0) + 5.559488023498642e-04 * f;
    return f / std::sqrt(h1 * h1 + h2 * h2);
}

// IEC 61672-1 annex E: the pole frequencies f1 (twice), f2, f3, f4 (twice); normalised to 0 dB at 1 kHz (the standard writes the same as A1000 = -2.000 dB)
double getAShape(double f)
{
    const double f1 = 20.598997;
    const double f2 = 107.65265;
    const double f3 = 737.86223;
    const double f4 = 12194.217;
    const double f2squared = f * f;
    return f4 * f4 * f2squared * f2squared / ((f2squared + f1 * f1) * std::sqrt((f2squared + f2 * f2) * (f2squared + f3 * f3)) * (f2squared + f4 * f4));
}
}

double getBs468Gain(double frequencyHz)
{
    if (frequencyHz <= 0.0)
    {
        return 0.0;
    }
    return getBs468Shape(frequencyHz) / getBs468Shape(1000.0);
}

double getAWeightingGain(double frequencyHz)
{
    if (frequencyHz <= 0.0)
    {
        return 0.0;
    }
    return getAShape(frequencyHz) / getAShape(1000.0);
}

double getWeightingGain(Weighting weighting, double frequencyHz)
{
    switch (weighting)
    {
        case Weighting::Unweighted:
            return 1.0;
        case Weighting::Band:
            if (frequencyHz >= 20.0 && frequencyHz <= 20000.0)
            {
                return 1.0;
            }
            return 0.0;
        case Weighting::CcirRms:
            return getBs468Gain(frequencyHz) * std::pow(10.0, kCcirRmsOffsetDb / 20.0);
        case Weighting::A:
            return getAWeightingGain(frequencyHz);
    }
    return 1.0;
}
}
