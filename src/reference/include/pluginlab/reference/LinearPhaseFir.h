#pragma once

#include <functional>
#include <vector>

#include "pluginlab/reference/DirectFormFilter.h"

namespace pluginlab::reference
{
// A linear-phase FIR filter with an odd number of taps from a target magnitude |H(f)| (frequency sampling on a dense grid, zero phase,
// truncated to the taps and shaped with a Blackman window). The taps are symmetric: the phase is exactly linear, the delay (taps - 1) / 2.
std::vector<double> designLinearPhaseFir(const std::function<double(double)>& magnitude, double sampleRate, int taps);

// The filter that runs these taps, reporting the latency (taps - 1) / 2
DirectFormFilter makeLinearPhaseFir(const std::function<double(double)>& magnitude, double sampleRate, int taps);
}
