#include "pluginlab/reference/LinearPhaseFir.h"

#include "ReferenceMath.h"

namespace pluginlab::reference
{
namespace
{
constexpr int kGridPerTap = 8;           // the frequency grid has at least this many points per tap
constexpr double kBlackmanA0 = 0.42;
constexpr double kBlackmanA1 = 0.5;
constexpr double kBlackmanA2 = 0.08;

int getGridSize(int taps)
{
    int size = 1;
    while (size < kGridPerTap * taps)
    {
        size *= 2;
    }
    return size;
}
}

std::vector<double> designLinearPhaseFir(const std::function<double(double)>& magnitude, double sampleRate, int taps)
{
    jassert(taps % 2 == 1);
    const int gridSize = getGridSize(taps);
    const int half = gridSize / 2;
    std::vector<double> samples(static_cast<size_t>(half + 1));
    for (int bin = 0; bin <= half; ++bin)
    {
        samples[static_cast<size_t>(bin)] = magnitude(sampleRate * bin / gridSize);
    }
    // h[m] = 1/M (A0 + 2 sum A_k cos(2 pi k m / M) + A_(M/2) cos(pi m)), m = -(taps-1)/2 ... (taps-1)/2: the inverse DFT of the real, even target
    // computed for the centre and the right half, mirrored to the left: the taps are exactly symmetric
    const int centre = (taps - 1) / 2;
    std::vector<double> coefficients(static_cast<size_t>(taps));
    for (int m = 0; m <= centre; ++m)
    {
        double sum = samples.front() + samples.back() * std::cos(detail::kPi * m);
        for (int bin = 1; bin < half; ++bin)
        {
            sum += 2.0 * samples[static_cast<size_t>(bin)] * std::cos(detail::kTwoPi * bin * m / gridSize);
        }
        const double phase = detail::kTwoPi * (centre + m) / (taps - 1);
        const double window = kBlackmanA0 - kBlackmanA1 * std::cos(phase) + kBlackmanA2 * std::cos(2.0 * phase);
        coefficients[static_cast<size_t>(centre + m)] = sum / gridSize * window;
        coefficients[static_cast<size_t>(centre - m)] = sum / gridSize * window;
    }
    return coefficients;
}

DirectFormFilter makeLinearPhaseFir(const std::function<double(double)>& magnitude, double sampleRate, int taps)
{
    return DirectFormFilter(designLinearPhaseFir(magnitude, sampleRate, taps), {1.0}, sampleRate, 0, (taps - 1) / 2);
}
}
