# Reference processors: what an ideal processor does

Teaching material of pluginlab (work package W6.6). License: **CC BY-SA 4.0** (the text and the figures; the code of pluginlab is GPLv3).

Every reference processor of pluginlab (`src/reference/`, library `pluginlab_reference`) has an answer that is known exactly: a formula for
its frequency response, its harmonics or its noise. The tests of pluginlab check the code against that answer (`tests/ReferenceTests.cpp`,
`tests/NonlinearReferenceTests.cpp`) and against an independent implementation in Python (`tests/oracle/`). The same processors are in the three
reference plugins (PluginLab Reference EQ, Nonlinear, Utility), so everything on these pages can be heard and measured in a DAW.

| Page | What |
|---|---|
| [RBJ cookbook](rbj-cookbook.md) | the nine biquad types most EQ plugins use; analog prototype and bilinear transform |
| [Cramping, Orfanidis](orfanidis-and-cramping.md) | why a peak near Nyquist gets narrow, and a design that avoids it |
| [Zoelzer (DAFX)](zoelzer.md) | the shelving and peak filters of the textbook; boost and cut as mirror images |
| [State-variable filter](state-variable-filter.md) | one structure for all types (TPT / zero-delay feedback), safe under modulation |
| [Butterworth and Linkwitz-Riley](butterworth-linkwitz-riley.md) | slopes of 6 dB per octave and order; crossovers that sum to flat |
| [Linear-phase FIR](linear-phase-fir.md) | the same magnitude without phase shift: latency and pre-ringing |
| [Fractional delays](fractional-delays.md) | delays of a fraction of a sample: Thiran all-pass and Lagrange interpolation |
| [Waveshapers](waveshapers.md) | polynomial, hard and soft clipping: which harmonics, how strong |
| [Quantizer and dither](quantizer-and-dither.md) | 6.02 N + 1.76 dB, and what dither does |
| [Utilities](utilities.md) | gain, polarity, channel matrix (width, crosstalk), DC, hum, noise, tremolo |

The figures are computed with the library itself (`apps/teaching`, `PluginLabTeachingFigures docs/teaching/references/figures`), at 48 kHz unless
stated otherwise: what you see is what the tested code computes.

Notation: $f_s$ sample rate, $f_0$ corner or centre frequency, $\omega_0 = 2\pi f_0 / f_s$, $z^{-1}$ one sample of delay, $s$ the Laplace variable
of the analog prototype (normalised so that $s = j$ at $f_0$), gains in dB unless a factor is named.
