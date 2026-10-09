# Delay and polarity

Unit of W7.3. Code: `src/measure/Delay.cpp` (`measureDelay`), the impulse response `src/measure/SweptImpulse.cpp` (shared with the frequency
response); tests `tests/MeasureDelayTests.cpp`. Conventions: [README](README.md); the sweep and its window: [frequency-response.md](frequency-response.md).

## Purpose
How late a plugin's output is (its latency, the delay a host has to compensate), and whether it inverts the polarity. Both matter for every comparison
of two plugins (null test, A/B listening) and for the fingerprint's question "reports the plugin its latency correctly?".

## Standard and sources
- **AES17-2015, 6.8.2 Delay through the EUT** (optional, "advanced equipment"): signal level -20 dB re the maximum input level; two methods:
  (a) **impulse response method**: "The impulse response of the EUT is derived from the inverse Fourier transform of the complex division of the EUT output
  spectrum by the test signal spectrum. The delay through the EUT is the time value corresponding to the absolute peak in the impulse response."
  (b) **cross-correlation method**: "The test signal shall be a wideband signal, such as pseudorandom noise, or a swept sine wave. The input and output
  signals shall be cross-correlated... The delay through the EUT is the time value corresponding to the absolute peak in the correlation function."
  The note of 6.8.2: the delay of a converter consists of a large constant part (measured here) and a small frequency-dependent part (6.8.3 phase response).
- **AES17-2015, 6.2.8 Polarity**: (a) a burst, (b) "an asymmetrical signal ... a 997 Hz sine wave summed with a 1 994 Hz sine wave whose phase is
  shifted by minus a quarter cycle relative to the main tone. If the output of the EUT has the single peak positive, the EUT is non-inverting",
  (c) "the impulse response of the EUT is measured. If the peak of the impulse response is positive, the EUT is non-inverting."
- Cross-correlation for time-delay estimation: C. H. Knapp, G. C. Carter, "The generalized correlation method for estimation of time delay", IEEE Trans.
  Acoustics, Speech, and Signal Processing 24(4), 1976, pp. 320-327 (here the plain correlation; the noise is white, so the PHAT weighting would change
  nothing).
- Phase delay $\tau_p(\omega) = -\varphi(\omega)/\omega$ and group delay $\tau_g = -d\varphi/d\omega$: A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal
  Processing*, 3rd ed., section 5.1.

## Stimuli
- (a) the synchronized sweep of the frequency response (5 Hz ... 0.95 Nyquist, about 4 s, -20 dBFS peak);
- (b) white Gaussian noise (seed 31), 1 s at -20 dBFS rms, followed by 0.5 s of silence (the largest delay looked for);
- polarity (b): $x = \frac A2\left(\sin(2\pi\,997\,t) + \sin(2\pi\,1994\,t - \tfrac\pi2)\right)$, $A$ = -20 dBFS, 0.5 s. Its largest positive value is about 1.78
  times its largest negative value (2 against -1.125 for a continuous waveform; the samples miss the exact peaks).

## Routine and analysis
1. **Impulse response** (6.8.2 a): deconvolve the sweep response (see frequency-response.md); the delay is the index of the absolute peak
   (`impulsePeakSamples`). Also given:
   - `impulsePeakInterpolated`: the vertex of the parabola through the peak and its two neighbours;
   - `phaseDelaySamples`: the phase delay at 100 Hz, $\tau_p = -\arg H(f)/(2\pi f/f_s)$, unwrapped around the peak:
     $\tau_p = n_\text{peak} - \arg\!\left(H(f)\,e^{j\omega n_\text{peak}}\right)/\omega$ (an inversion is removed first: it is not a delay).
2. **Cross-correlation** (6.8.2 b): $r[k] = \sum_n x[n]\,y[n+k]$ for $k = 0 \dots$ 0.5 s (by FFT); the delay is the index of its absolute peak, also interpolated.
3. **Polarity** (6.2.8 c): the sign of the impulse response at its peak. (6.2.8 b): the last half of the response to the asymmetric signal; the ratio of the
   largest positive to the largest negative value; above 1: non-inverting.

## Band of validity and limits
- Delays from 0 to half the deconvolution buffer (several seconds) by the impulse response, 0 ... 0.5 s by correlation (`maximumDelaySeconds`).
  A plugin with "negative" delay (the output before the input) cannot exist in a host; it would show up as a peak at the end of the buffer.
- **The peak is the delay only for a pure delay** (and for a linear-phase filter: its centre). For a fractional delay the band-limited impulse has its
  largest sample at the nearest whole sample; the parabola is biased (37.10 for 37.25). For a fractional delay the **phase delay at a low frequency** is
  the right number (exact to 1e-4 samples here).
- **For a minimum-phase filter "the delay" is not one number**: the peak, the phase delay and the correlation peak differ (RBJ low-pass 1 kHz: 9, 10.78 and
  8 samples); AES17's note expects this part to be small compared with the constant delay of a converter. For a plugin that reports latency, the
  peak (impulse response) is what a host compensates.
- **A high-pass has no meaningful phase delay at low frequencies** (its phase leads by almost 180 degrees: -229 samples at 100 Hz).
- **Polarity**: the impulse method (c) and the asymmetric signal (b) agree for delays, gains, inversions and linear-phase filters. **A filter that shifts the
  phase of 997 Hz and 1994 Hz differently changes the shape of the asymmetric signal**: the RBJ high-pass at 1 kHz makes it look inverted (ratio 0.78)
  although its impulse response starts positive. Method (b) is only valid where the phase response between 997 and 1994 Hz is flat (or linear).

## Results for known test signals
From `MeasureDelayTests` (2026-10-09, 0.28.0), 48 kHz:

| Device | Expected peak / phase delay | Impulse peak | Interpolated | Phase delay (100 Hz) | Correlation peak | Interpolated | Polarity (impulse) | Polarity (asymmetric signal, ratio) |
|---|---|---|---|---|---|---|---|---|
| integer delay 0 | 0 / 0 | 0 | 0.000 | 0.0000 | 0 | -0.001 | non-inverting | non-inverting (1.78) |
| integer delay 37 | 37 / 37 | 37 | 37.000 | 37.0000 | 37 | 37.000 | non-inverting | non-inverting (1.78) |
| integer delay 1000 | 1000 / 1000 | 1000 | 1000.000 | 1000.0000 | 1000 | 1000.000 | non-inverting | non-inverting (1.78) |
| Thiran 37.25 samples (order 3) | (37) / 37.25 | 37 | 37.100 | 37.2500 | 37 | 37.090 | non-inverting | non-inverting (1.78) |
| Lagrange 37.5 samples (order 3) | (37 or 38) / 37.5 | 38 | 37.500 | 37.5000 | 37 | 37.500 | non-inverting | non-inverting (1.78) |
| gain -6 dB, polarity inverted | 0 / 0 | 0 | 0.000 | 0.0000 | 0 | -0.001 | inverting | inverting (0.56) |
| inverted, delay 100 | 100 / 100 | 100 | 100.000 | 100.0000 | 100 | 100.000 | inverting | inverting (0.56) |
| linear-phase FIR 255 taps (latency 127) | 127 / 127 | 127 | 127.000 | 127.0000 | 127 | 127.000 | non-inverting | non-inverting (1.95) |
| RBJ low-pass 1 kHz Q 0.71 | 9 / 10.781 | 9 | 8.543 | 10.7806 | 8 | 8.497 | non-inverting | non-inverting (1.37) |
| RBJ high-pass 1 kHz Q 0.71 | 0 / -229.22 | 0 | -0.047 | -229.2199 | 0 | -0.042 | non-inverting | **inverting (0.78)** |

Expected values: the delays by construction; for the RBJ filters the peak of their impulse response (computed directly) and $-\arg H(100\,\text{Hz})/\omega$
from the exact response. Tolerances in the test: impulse peak exact (not for fractional delays), phase delay 0.001 samples, correlation peak within 1 sample,
polarity by impulse always right, polarity by the asymmetric signal right for delays, gains and the linear-phase FIR.

**What the results teach**
- A latency is a whole number only for a pure delay. For fractional delays and filters, three honest methods give three different numbers; the report
  must say which one it means.
- AES17's polarity methods disagree for filters that shift the phase; the impulse response decides.

## Implementation
```cpp
pluginlab::measure::DelaySettings settings;       // sampleRate, levelDbfs (-20), noiseSeconds, maximumDelaySeconds, phaseDelayHz (100), channels
pluginlab::measure::DelayResult result = pluginlab::measure::measureDelay(device, settings);
// result.channels[c]: impulsePeakSamples, impulsePeakInterpolated, phaseDelaySamples, correlationPeakSamples, correlationPeakInterpolated,
//                     invertingByImpulse, invertingByTwoTone, twoToneRatio
```
