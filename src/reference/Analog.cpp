#include "pluginlab/reference/Analog.h"

#include "ReferenceMath.h"

namespace pluginlab::reference
{
namespace
{
constexpr double kShelfDecibelsPerDecade = 40.0;
}

const char* getFilterTypeName(FilterType type)
{
    switch (type)
    {
        case FilterType::LowPass:
            return "low-pass";
        case FilterType::HighPass:
            return "high-pass";
        case FilterType::BandPass:
            return "band-pass";
        case FilterType::BandPassUnity:
            return "band-pass (0 dB peak)";
        case FilterType::Notch:
            return "notch";
        case FilterType::AllPass:
            return "all-pass";
        case FilterType::Peak:
            return "peak";
        case FilterType::LowShelf:
            return "low shelf";
        case FilterType::HighShelf:
            return "high shelf";
    }
    return "";
}

std::complex<double> getAnalogResponse(FilterType type, double frequencyHz, double cornerHz, double gainDb, double q)
{
    const std::complex<double> s(0.0, frequencyHz / cornerHz);
    const double a = std::pow(10.0, gainDb / kShelfDecibelsPerDecade);
    const double rootA = std::sqrt(a);
    const std::complex<double> denominator = s * s + s / q + 1.0;
    switch (type)
    {
        case FilterType::LowPass:
            return 1.0 / denominator;
        case FilterType::HighPass:
            return s * s / denominator;
        case FilterType::BandPass:
            return s / denominator;
        case FilterType::BandPassUnity:
            return s / q / denominator;
        case FilterType::Notch:
            return (s * s + 1.0) / denominator;
        case FilterType::AllPass:
            return (s * s - s / q + 1.0) / denominator;
        case FilterType::Peak:
            return (s * s + s * a / q + 1.0) / (s * s + s / (a * q) + 1.0);
        case FilterType::LowShelf:
            return a * (s * s + s * rootA / q + a) / (a * s * s + s * rootA / q + 1.0);
        case FilterType::HighShelf:
            return a * (a * s * s + s * rootA / q + 1.0) / (s * s + s * rootA / q + a);
    }
    return 1.0;
}

double getWarpedFrequency(double frequencyHz, double cornerHz, double sampleRate)
{
    return cornerHz * std::tan(detail::kPi * frequencyHz / sampleRate) / std::tan(detail::kPi * cornerHz / sampleRate);
}
}
