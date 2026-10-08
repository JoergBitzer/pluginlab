#include "pluginlab/reference/Delays.h"

#include <cmath>

namespace pluginlab::reference
{
namespace
{
constexpr double kWholeSampleTolerance = 1.0e-12; // a delay this close to whole samples is a pure delay (the Thiran formula would be 0/0)

double getBinomial(int n, int k)
{
    double value = 1.0;
    for (int index = 1; index <= k; ++index)
    {
        value *= static_cast<double>(n - k + index) / index;
    }
    return value;
}
}

DirectFormFilter makeIntegerDelay(int samples, double sampleRate)
{
    return DirectFormFilter({1.0}, {1.0}, sampleRate, samples);
}

DirectFormFilter makeThiranDelay(double samples, int order, double sampleRate)
{
    const int integerDelay = static_cast<int>(std::lround(samples)) - order;
    jassert(integerDelay >= 0);
    const double delay = samples - integerDelay;
    if (std::abs(delay - order) < kWholeSampleTolerance)
    {
        return makeIntegerDelay(static_cast<int>(std::lround(samples)), sampleRate);
    }
    std::vector<double> denominator(static_cast<size_t>(order + 1));
    for (int k = 0; k <= order; ++k)
    {
        double product = 1.0;
        for (int n = 0; n <= order; ++n)
        {
            product *= (delay - order + n) / (delay - order + k + n);
        }
        double sign = 1.0;
        if (k % 2 == 1)
        {
            sign = -1.0;
        }
        denominator[static_cast<size_t>(k)] = sign * getBinomial(order, k) * product;
    }
    // the all-pass: the numerator is the denominator reversed
    const std::vector<double> numerator(denominator.rbegin(), denominator.rend());
    return DirectFormFilter(numerator, denominator, sampleRate, integerDelay);
}

DirectFormFilter makeLagrangeDelay(double samples, int order, double sampleRate)
{
    // the fractional delay D of the interpolator in the middle interval of its taps (it interpolates there and extrapolates outside):
    // odd orders [(N-1)/2, (N+1)/2), even orders [N/2 - 0.5, N/2 + 0.5); the integer delay takes the rest
    int integerDelay = static_cast<int>(std::lround(samples)) - order / 2;
    if (order % 2 == 1)
    {
        integerDelay = static_cast<int>(std::floor(samples)) - (order - 1) / 2;
    }
    jassert(integerDelay >= 0);
    const double delay = samples - integerDelay;
    std::vector<double> taps(static_cast<size_t>(order + 1));
    for (int k = 0; k <= order; ++k)
    {
        double product = 1.0;
        for (int n = 0; n <= order; ++n)
        {
            if (n != k)
            {
                product *= (delay - n) / (k - n);
            }
        }
        taps[static_cast<size_t>(k)] = product;
    }
    return DirectFormFilter(taps, {1.0}, sampleRate, integerDelay);
}
}
