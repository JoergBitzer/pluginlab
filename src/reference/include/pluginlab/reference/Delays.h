#pragma once

#include "pluginlab/reference/DirectFormFilter.h"

namespace pluginlab::reference
{
// A delay of whole samples: H = e^(-j w D)
DirectFormFilter makeIntegerDelay(int samples, double sampleRate);

// A fractional delay with a Thiran all-pass of the given order (|H| = 1, maximally flat group delay at DC):
// a_k = (-1)^k C(N, k) prod_(n=0..N) (D - N + n) / (D - N + k + n), the all-pass delay D in [N - 0.5, N + 0.5), an integer delay takes the rest
DirectFormFilter makeThiranDelay(double samples, int order, double sampleRate);

// A fractional delay with a Lagrange interpolator (FIR) of the given order: h_k = prod_(n != k) (D - n) / (k - n), D near the middle of the taps,
// an integer delay takes the rest (maximally flat response at DC; the magnitude falls towards Nyquist)
DirectFormFilter makeLagrangeDelay(double samples, int order, double sampleRate);
}
