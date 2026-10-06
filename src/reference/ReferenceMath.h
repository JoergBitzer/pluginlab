#pragma once

#include <cmath>
#include <complex>

// Small helpers shared by the reference processors (private to the library)
namespace pluginlab::reference::detail
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDecibelsPerAmplitudeDecade = 20.0;

inline double dbToGain(double decibels)
{
    return std::pow(10.0, decibels / kDecibelsPerAmplitudeDecade);
}

// z^-1 = e^(-j w) at a frequency
inline std::complex<double> getInverseZ(double frequencyHz, double sampleRate)
{
    return std::polar(1.0, -kTwoPi * frequencyHz / sampleRate);
}

// The polynomial c0 + c1 z^-1 + c2 z^-2 + ... at z^-1
inline std::complex<double> evaluatePolynomial(const double* coefficients, int count, std::complex<double> inverseZ)
{
    std::complex<double> sum = 0.0;
    std::complex<double> power = 1.0;
    for (int index = 0; index < count; ++index)
    {
        sum += coefficients[index] * power;
        power *= inverseZ;
    }
    return sum;
}
}
