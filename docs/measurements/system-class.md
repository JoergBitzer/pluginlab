# System class

Unit of W8.1. Code: `src/measure/SystemClass.cpp` (`classifySystem`), the
harmonic responses `getHarmonicResponse` in `src/measure/Distortion.cpp`;
tests `tests/MeasureSystemClassTests.cpp`. Plan: `docs/design/W8-plan.md`,
section 2.

## Purpose
Most tools assume a linear, time-invariant system: a frequency response, a band
model, a matcher. Many plugins are not LTI (an "analogue" EQ with drive, a
compressor, a chorus). The class says which measurements mean what. It is the
first row of the AES17 measurement report.

## Classes and tests
All tests use synchronized sweeps (Novak et al. 2015) at three levels: -40, -20
and -6 dBFS. The deconvolution gives the linear response H_1(f) and the
harmonic responses H_n(f), n = 2 ... 5.

| Order | Class | Test (passes) | Tolerance |
|---|---|---|---|
| 1 | time-variant | the same sweep, 0.37 s later in the render, gives another H_1 | 0.1 dB |
| 2 | LTI | every H_n below -80 dB and H_1 the same at every level | 0.05 dB |
| 3 | other (dynamics) | H_1 changes with level more than odd harmonics allow | 0.05 dB |
| 4 | memoryless | H_1 and every significant H_n flat over frequency | 0.05 / 0.2 dB |
| 5 | Hammerstein | H_n(f) / H_1(n f) constant: one filter after the curve | 0.2 dB |
| 6 | Wiener | H_n(f) / H_1(f)^n constant: one filter before the curve | 0.3 dB |
| 7 | generalized Hammerstein | each H_n keeps its shape between the two highest levels | 1.0 dB |
| 8 | other | none of the above | - |

The first test that passes decides. "Significant": above -80 dB re H_1
somewhere at the highest level. "Odd harmonics allow": a curve changes its
gain with the level through its odd terms; for a polynomial
|delta H_1| <= 3 |H_3| + 10 |H_5| (the cubic and fifth-order terms), with the
largest ratios over frequency.

## Sources
- A. Novak, P. Lotton, L. Simon, "Synchronized swept-sine: theory,
  application, and implementation", J. Audio Eng. Soc. 63(10), 2015,
  pp. 786-798.
- A. Novak, L. Simon, F. Kadlec, P. Lotton, "Nonlinear system identification
  using exponential swept-sine signal", IEEE Trans. Instrumentation and
  Measurement 59(8), 2010, pp. 2220-2229 (generalized Hammerstein).

## Band of validity and limits
- Frequencies: 50 Hz to 0.9 x Nyquist / 5 (4.3 kHz at 48 kHz), so that every
  harmonic up to the 5th stays below Nyquist. Aliased harmonics of the sweep
  are descending chirps that the deconvolution smears into every window.
- A sweep that stops at Nyquist / 5 avoids aliasing completely, but its abrupt
  end makes the linear pulse ring into the harmonic windows (-46 dB of
  "harmonics" for a pure gain). The full-band sweep with a limited grid is
  the better choice.
- Harmonic values below -105 dB re H_1 are near the noise and are left out of
  the shape tests (each test checks this at the level it uses).
- The generalized Hammerstein test is the leading-order shape test, not the
  full identification of the branch filters (Novak et al. 2010). A
  Wiener-Hammerstein system also keeps its shapes at leading order, so this
  class means "shape-preserving, but neither Hammerstein nor Wiener".
- Time variance is found if it changes H_1 within the render (an LFO). A
  plugin that is random from render to render is found by the fingerprint
  (determinism), not here.
- The class is decided on channel 1 only.

## Results for known test signals
From `MeasureSystemClassTests` (2026-10-11, 0.43.0), 48 kHz. All nine devices
are classified correctly. Evidence in dB:

| Device | Class | Time var. | Largest H_n | H_1 levels | Hammer. | Wiener | Shape |
|---|---|---|---|---|---|---|---|
| gain -6 dB | LTI | 0.000 | -96.3 | 0.000 | 0.000 | 0.000 | 0.000 |
| RBJ peak 1 kHz +6 dB | LTI | 0.000 | -96.2 | 0.000 | 0.000 | 0.000 | 0.000 |
| tremolo 3 Hz, depth 0.3 | time-variant | 1.054 | -98.7 | 0.000 | - | - | - |
| x + 0.1x^2 + 0.05x^3 | memoryless | 0.000 | -32.1 | 0.081 | 0.036 | 0.200 | 0.193 |
| tanh(2x) | memoryless | 0.000 | -23.5 | 1.824 | 0.125 | 0.126 | 0.521 |
| polynomial, then LP 1.5 kHz | Hammerstein | 0.000 | -32.1 | 0.081 | 0.049 | 23.361 | 0.248 |
| LP 1.5 kHz, then polynomial | Wiener | 0.000 | -32.1 | 0.081 | 23.559 | 0.201 | 0.194 |
| x + LP(0.1x^2) + HP(0.05x^3) | gen. Hammerstein | 0.000 | -32.0 | 0.076 | 32.764 | 30.893 | 0.815 |
| compressor -30 dBFS 4:1 | other | 0.000 | -44.5 | 18.001 | - | - | 0.018 |

Flatness over frequency (H_1 / H_n): polynomial 0.000 / 0.035, tanh
0.006 / 0.125; the filtered devices are not flat (17.6 dB and more).

**What the results teach**
- A memoryless curve changes its gain with the level (tanh: 1.8 dB between
  -40 and -6 dBFS) without being "dynamic": the odd harmonics explain it.
  A compressor changes it by 18 dB with small harmonics: that is dynamics.
- Where the filter sits shows in one ratio: after the curve, H_n(f) / H_1(n f)
  is constant; before it, H_n(f) / H_1(f)^n.

## Implementation
```cpp
pluginlab::measure::SystemClassSettings settings;   // levels, maximumHarmonic,
                                                    // tolerances, band
pluginlab::measure::SystemClassResult result =
    pluginlab::measure::classifySystem(device, settings);
// result.systemClass, result.reason, result.evidence (all numbers above)
// getSystemClassName(), getSystemClassMeaning()
```
