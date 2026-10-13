# The matcher: the EQ domain

Unit of W9.2. Code: `src/match/EqDomain.cpp` (`MagnitudeProbe`,
`getCurveMagnitude`, `makeMagnitudeObjective`); tests `tests/MatchEqTests.cpp`
(reference processors, no plugin). Plan: `docs/design/W9-plan.md`, 4.4.

## The parts
| Part | What |
|---|---|
| `MagnitudeProbe` | a synchronized sweep (-12 dBFS, 0.5 s) through the device, the impulse response 0.25 s after its peak, the magnitude in dB on a log grid (12 points per octave) in the user's range (default 20 Hz ... 20 kHz, at most 0.45 fs), clamped at -100 dB |
| `getCurveMagnitude` | a band-model curve as the master (bands typed in or from a band fit): exact, no render, the same grid and floor |
| `makeMagnitudeObjective` | the dB difference, every grid point the same weight (the grid is logarithmic: every octave counts the same); within the tolerance when the largest difference is below it (default 0.1 dB, `planning.md`: "equal") |

**Tolerance levels** (0.58.0, on the author's suggestion: the just noticeable
difference of a level in a third-octave band is about 1 dB): `MatchTolerance`
exact (0.01 dB, for checking precision), **strict (0.1 dB, the default;
`planning.md`: "equal")**, medium (0.25 dB), loose (0.5 dB). A looser level ends
the search sooner: the RBJ twin below takes 31 renders at exact, 23 at loose.

Amplitude only (the author: phase later). The output gain is matched only if
the user releases a gain parameter for it.

The floor matters: without it, the stop band of a low-pass (-300 dB at the
floor of the logarithm) dominated the difference and the optimizer stopped at
a residual of 26 dB; with it, the same match takes 10 renders.

## Results
From `MatchEqTests` (2026-10-13, 0.57.0), Debug, reference processors as
targets (frequency log 20 Hz ... 20 kHz, gain -24 ... 24 dB, Q 0.1 ... 10, as a
typical EQ), the band-model curve as master, tolerance 0.01 dB:

| Master | Start (the user's) | Result | Renders, time |
|---|---|---|---|
| RBJ peak 1 kHz +6 dB Q 2 | RBJ peak 600 Hz +3 dB Q 1.2 | within 0.002 dB; frequency 0.1 %, gain 0.01 dB, Q 0.5 % | 31, 8.7 s |
| RBJ low shelf 200 Hz +4 dB Q 0.71 | 500 Hz +1 dB Q 1 | within 0.008 dB | 21, 5.9 s |
| Butterworth low-pass order 4, 2 kHz (frequency only) | 800 Hz | within 0.009 dB | 10, 2.8 s |
| two RBJ peaks (300 Hz +5 dB Q 1, 3 kHz -4 dB Q 2) | 200 Hz +2 dB Q 0.7, 4 kHz -1 dB Q 1 (six parameters) | within 0.01 dB | 67, 19 s |
| Orfanidis peak 10 kHz +6 dB Q 1 | an RBJ peak at the same values (tolerance 0.1 dB) | best RBJ 10436 Hz, +5.94 dB, Q 0.637; largest deviation -0.81 dB at 19.3 kHz, rms 0.12 dB: the teaching case, the designs differ near Nyquist | |

The probe agrees with the band-model curve within 0.005 dB.

## Limits
- **Speed:** a render through the probe takes about 0.28 s in the Debug build
  even for a biquad (the sweep and its analysis); the author's budget of
  10 ... 15 s per target needs the Release build and the fast render path of
  W9.3 (an impulse instead of the sweep, one live instance).
- **Far starts:** from a peak 1.7 octaves away (300 Hz for a 1 kHz master) LM
  ran into a local minimum (the band flattened: Q 0.11, +0.2 dB). A start
  within about an octave converges; a far start needs the start point from
  the model (band fit of the master and the target's mapping, W9.4).
