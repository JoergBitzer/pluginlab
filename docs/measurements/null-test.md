# Null test with alignment

Unit of W7.10. Code: `src/measure/NullTest.cpp` (`measureNull`); tests `tests/MeasureNullTests.cpp`. Conventions: [README](README.md); delay
and fractional delays: [delay-and-polarity.md](delay-and-polarity.md).

## Purpose
The most direct answer to "do these two plugins do the same?": feed both the same signal, line up the outputs and subtract. What is left (the
residual) is everything in which they differ. The mission "divide the myth from reality" rests on this test: two EQs at "the same" setting, a plugin
before and after an update, a plugin against its reference implementation, an "analogue" plugin against a plain filter. The alignment matters: a
latency difference of one sample or a gain difference of 0.1 dB would otherwise leave a large residual that says nothing about the sound.

## Standard and sources
- **No standard** describes a null test of two devices; AES17 compares a device with its input, not two devices. The method here follows the usual
  practice of a difference measurement: equal stimulus, alignment of time and level, difference, level of the residual.
- Time-delay estimation: C. H. Knapp, G. C. Carter, "The generalized correlation method for estimation of time delay", IEEE Trans. Acoustics, Speech,
  and Signal Processing 24(4), 1976, pp. 320-327 (the integer delay from the peak of the cross-correlation). The fractional part from the slope of
  the cross-spectrum's phase, weighted least squares: the same model ($A(\omega) = g\,e^{-j\omega d}B(\omega)$) that the generalized correlation uses.
- Fractional delays in the test devices: T. I. Laakso, V. Välimäki, M. Karjalainen, U. K. Laine, "Splitting the unit delay", IEEE Signal Processing
  Magazine 13(1), 1996, pp. 30-60 (Thiran and Lagrange).
- The gain is the least-squares solution of $\min_g \sum |A - gB'|^2$: $g = \mathrm{Re}\sum A\overline{B'} / \sum |B'|^2$ (a real gain; its sign is the
  polarity).

## Stimulus
White Gaussian noise at -20 dBFS (AES17 dBFS: rms re a full-scale sine), independent per channel (seed 17), long enough for 0.5 s settling, the window
(131072 samples = 2.7 s at 48 kHz) and the largest delay looked for (0.25 s). Any other stimulus (music) can be given instead (`settings.stimulus`).
White noise weights all frequencies alike, so the null depth is the mean over the band; music weights them like music.

## Routine and analysis
1. Render A and B (each from a fresh state).
2. **Integer delay**: the cross-correlation $r[k] = \sum_n a[n]\,b[n+k]$ of A's window with B for $k = -0.25 \dots +0.25$ s (by FFT); the k of the largest
   $|r|$ (the sign does not matter: an inverted B correlates negatively).
3. Hann-windowed spectra of A's window and of B's window shifted by the integer delay.
4. **Fractional delay**: the sign of $\sum \mathrm{Re}(A\overline{B})$ tells the polarity; the phase of $\pm A\overline{B}$ is fitted by a line through the
   origin, weighted by $|A\overline{B}|$, over 20 Hz ... 20 kHz: $\delta = -\sum w\,\omega\,\varphi / \sum w\,\omega^2$. B is delayed by it in the frequency domain
   ($B' = B\,e^{-j\omega\delta}$).
5. **Gain**: $g = \mathrm{Re}\sum A\overline{B'} / \sum |B'|^2$ over the band.
6. **Null depth**: $10\log_{10}\left(\sum |A - gB'|^2 / \sum |A|^2\right)$ over the bins from 20 Hz to 20 kHz; also the residual's level in dBFS, the residual
   per standard third-octave band relative to A, and the null without any alignment (A - B, same window) for comparison.

## Band of validity and limits
- The alignment is linear (one delay, one gain). Two devices that differ in their frequency response cannot null completely; the result is then the best
  null that a delay and a gain can reach: for an RBJ high shelf against nothing -10.7 dB, for a +3 dB against a +3.5 dB peak -37.7 dB.
- The delay that aligns a whole band best is not always the latency or the low-frequency phase delay: for a Thiran delay of 37.25 samples it is 37.171
  (its group delay rises towards Nyquist, see phase-and-group-delay.md). The unit reports the delay it used; the test checks that it is the optimum.
- The floor: identical devices give exactly zero (-300 dB, the clamp); a delayed and scaled copy -152 dB (float rounding of the scaled samples).
  Noise and time-varying processing (modulation, dither) do not null and set the floor of a real comparison: two dithered 16-bit paths null to -70 dB at
  -20 dBFS.
- Delays beyond +-0.25 s (`maximumDelaySeconds`) are not found. A device whose latency changes during the render does not align.
- The fractional shift is applied to the Hann-windowed spectrum (a circular shift of the windowed signal); this is an approximation whose floor was not
  measured separately. In the cases here the null is limited by the devices themselves (the measured nulls equal the exact expectations to 0.1 dB).

## Results for known test signals
From `MeasureNullTests` (2026-10-09, 0.35.0), 48 kHz, white noise -20 dBFS, one channel.

**Exact nulls** (delay and gain known by construction):

| A (against B) | Expected delay / gain | Measured delay (samples) | Measured gain | Inverted | Null, aligned | Null, unaligned |
|---|---|---|---|---|---|---|
| RBJ peak against itself | 0 / 0 dB | 0.0000 | 0.0000 dB | no | -300.0 dB | -300.0 dB |
| B delayed by 37 samples and -0.5 dB (B = identity) | 37 / -0.5 dB | 37.0000 | -0.5000 dB | no | -151.9 dB | +3.2 dB |
| B = 1000 samples later than A | -1000 / 0 dB | -1000.0000 | 0.0000 dB | no | -300.0 dB | +3.0 dB |
| B inverted, +3 dB | 0 / +3 dB | 0.0000 | 3.0000 dB | yes | -152.0 dB | +4.6 dB |

Without alignment, a delay of 37 samples on white noise leaves a residual louder than the signal (+3 dB: two uncorrelated signals add).

**Devices that differ by a known amount** (expected null: $\sum |H_A - g\,e^{-j\omega d}H_B|^2 / \sum |H_A|^2$ over the same bins with the found g and d; best
possible: the smallest null over a grid of delays (0.001 samples) with the least-squares gain for each):

| A against B | Measured delay | Measured gain | Expected null | Measured null | Best possible null (at delay) |
|---|---|---|---|---|---|
| RBJ peak 1 kHz Q 2: +3 dB against +3.5 dB | -0.0013 | -0.0283 dB | -37.70 dB | -37.79 dB | -37.70 dB (-0.001) |
| RBJ high shelf 5 kHz +6 dB against nothing | -0.0922 | +4.4771 dB | -10.70 dB | -10.72 dB | -10.70 dB (-0.092) |
| Thiran 37.25 samples (order 3) against nothing | 37.1709 | -0.0265 dB | -22.10 dB | -22.15 dB | -22.10 dB (37.171) |
| Lagrange 37.25 samples (order 3) against nothing | 37.1986 | -0.8795 dB | -17.66 dB | -17.65 dB | -17.66 dB (37.199) |

**Noise**: two 16-bit quantizers with independent TPDF dither: null -70.30 dB, expected $10\log_{10}(2\,(q^2/4)/\sigma^2)$ = -70.31 dB (both noises add);
residual -91.11 dBFS; every third-octave band within 2.9 dB of the broadband value (white residual, estimated from few bins at low frequencies).

Tolerances in the test: exact cases delay 0.001 samples, gain 1e-4 dB, null below -120 dB; known differences: null within 0.5 dB of the expectation and
at most 0.3 dB above the best possible null; noise 0.2 dB.

**What the results teach**
- Alignment is the whole point: unaligned, a pure delay of 37 samples looks like a residual of +3 dB; aligned, -152 dB.
- A null test of two "identical" plugins can only be as deep as their noise and their time-variance allow (-70 dB for two dithered 16-bit paths at
  -20 dBFS). A residual at that level is not a difference in sound.
- The difference of a 3 dB and a 3.5 dB peak is a -38 dB residual: small differences in settings are measurable long before they are audible as
  "different".

## Implementation
```cpp
pluginlab::measure::NullTestSettings settings;     // sampleRate, levelDbfs (-20), seed, stimulus (empty: noise), settleSeconds, windowSamples (2^17),
                                                   // maximumDelaySeconds (0.25), alignDelay, alignGain, channels
pluginlab::measure::NullTestResult result = pluginlab::measure::measureNull(deviceA, deviceB, settings);
// result.channels[c]: delaySamples, gainDb, inverted, nullDepthDb, unalignedNullDb, residualDbfs, thirdOctaveHz, thirdOctaveResidualDb
```
