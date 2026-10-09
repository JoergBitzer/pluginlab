# THD+N and THD

Unit of W7.5. Code: `src/measure/Distortion.cpp` (`measureDistortion`, `measureDistortionVsLevel`, `measureDistortionVsFrequency`,
`measureSweptHarmonics`), the FFT `getSpectrum` in `src/measure/Analyzer.cpp`, the sweep impulse response `src/measure/SweptImpulse.cpp`;
tests `tests/MeasureDistortionTests.cpp`, oracle case `polynomial_thd_48k` in `tests/OracleTests.cpp`. Conventions: [README](README.md); the sweep:
[frequency-response.md](frequency-response.md).

## Purpose
How much a plugin adds to a sine that was not there: the harmonics (THD: saturation, waveshaping, "analogue" colour), and everything else that is not
the sine (THD+N: harmonics, noise, aliases, quantization, dither). Against the level it shows where a plugin starts to saturate; against the frequency it
shows whether the distortion is followed by filtering (a Hammerstein structure, typical for "analogue-modelled" plugins) or whether harmonics alias.

## Standard and sources
- **AES17-2015, 6.3.1 THD+N ratio**: the output is measured once through the standard low-pass filter (the total) and once through the standard notch
  filter that removes the test frequency (the residual: harmonics plus noise); THD+N is the ratio of the two rms levels in dB, at 997 Hz. The notch is
  the analyzer's standard notch (5.2.8, Q between 1.2 and 3).
- **AES17-2015, 6.3.2 THD+N against frequency** and **6.3.3 THD+N against level** (the same measurement over a list of frequencies or levels; 6.3.2
  names -1 dB and -20 dB re the maximum input level).
- **AES17-2015, annex A.3.6** (the FFT methods): total distortion and noise from a synchronous FFT as the power of all bins in the passband except the
  stimulus bins relative to the total. **Annex A.4.7**: the harmonic distortion against frequency from the exponential sweep, by the harmonic impulse
  responses that the deconvolution places before the linear one.
- **IEC 60268-3** (Sound system equipment, Part 3: Amplifiers): the definition of total harmonic distortion as the rms of the harmonics relative to
  the fundamental, and of the n-th order harmonic distortion. THD here follows this definition: $\mathrm{THD} = \sqrt{\sum_{n \ge 2} H_n^2}\,/\,H_1$.
- Synchronized swept sine: A. Novak, P. Lotton, L. Simon, "Synchronized swept-sine: theory, application, and implementation", J. Audio Eng. Soc.
  63(10), 2015, pp. 786-798 (the harmonic $n$ is the sweep itself shifted by $L \ln n$ when $f_1 L$ is an integer); the exponential sweep:
  A. Farina, "Simultaneous measurement of impulse response and distortion with a swept-sine technique", AES 108th Convention, 2000, preprint 5093.
- Coherent (synchronous) sampling, no window function: every harmonic falls on its own bin, no leakage (the same principle as AES17 annex A.3).

## Stimuli
- **Sine** at the test frequency, made **coherent** with the analysis window: the window is the next power of two above `measureSeconds`
  (65536 samples = 1.37 s at 48 kHz) and the frequency is moved to the nearest bin (997 Hz -> 996.83 Hz, bin 1361). Level -1 dBFS (default), 0.5 s
  settling before the window.
- **Synchronized sweep** of the frequency response (5 Hz ... 0.95 Nyquist, about 4 s) for the harmonics against frequency.

## Routine and analysis
1. Render the sine; the output goes through the standard low-pass filter (identity at 44.1 and 48 kHz; AES17 5.2.5).
2. **Spectrum** of the window (radix-2 FFT in double precision, rectangular window). Amplitude of bin $k$: $A_k = 2|X_k|/N$.
3. **Harmonics** (IEC 60268-3): $H_n = A_{n k_1}$ for $n = 2 \dots 10$ while $n f \le$ 20 kHz; reported as $20\log_{10}(H_n/H_1)$.
   **THD** $= 10 \log_{10}\left(\sum_n H_n^2 / H_1^2\right)$ dB and in percent.
4. **THD+N in the frequency domain** (annex A.3.6): $10\log_{10}\left(\sum_{k \ne k_1} |X_k|^2 \big/ \sum_k |X_k|^2\right)$ over the bins from 20 Hz to 20 kHz.
   It is relative to the **total** (fundamental plus residual), as AES17 measures it; THD is relative to the fundamental. For small values both are the
   same; for large ones convert with $N/F = r/(1-r)$.
5. **THD+N with the notch** (6.3.1): the low-passed output through the standard notch (RBJ notch at the test frequency, Q 2), the rms of the window
   (mean removed: the passband starts at 20 Hz) relative to the rms without the notch.
6. **Against level / frequency** (6.3.3 / 6.3.2): steps 1 ... 5 for each level or each frequency (each made coherent).
7. **Harmonics from the sweep** (A.4.7): the deconvolved response contains the harmonic impulse response $h_n$ at $-L \ln n$ before the linear one.
   Each is cut out with a window that reaches half way to its neighbours ($\frac12 L\ln\frac{n+1}{n}$ before, $\frac12 L \ln\frac{n}{n-1}$ after), is flat
   over the inner half of each side and tapers with a raised cosine. Its spectrum at $n f$ divided by the reference channel is the amplitude of the $n$-th
   harmonic for the input frequency $f$; relative to the linear response at $f$.

## Band of validity and limits
- Harmonics count up to 20 kHz (the band edge); at 997 Hz that is the 10th (set by `maximumHarmonic`), at 5 kHz only the 2nd ... 4th. Harmonics above
  the band edge, and their **aliases**, count in THD+N but not in THD.
- **The two THD+N methods differ by design**: the notch also attenuates the 2nd and 3rd harmonic a little (Q 2 at 999.76 Hz: $|N(2f)| = -0.45$ dB,
  $|N(3f)| = -0.15$ dB; the polynomial's THD+N is 0.42 ... 0.45 dB lower with the notch), and at 44.1/48 kHz the standard low-pass is the identity, so the notch method
  also counts the noise between 20 kHz and Nyquist (white noise: +0.80 dB at 48 kHz; measured +0.66 dB with the notch's own share). The frequency-domain
  value is the one to compare with a closed form; the notch value is what an AES17 analyzer would read.
- Float output: harmonics below about -140 dBFS are under the float quantization of the samples and are not compared in the tests.
- The swept harmonics hold for systems that the sweep describes: a memoryless curve followed or preceded by filters (Hammerstein, Wiener-like).
  For systems with memory in the non-linearity (compressors, slow saturation with envelope) the harmonic levels depend on the sweep rate; use the stepped
  measurement. A harmonic whose frequency $n f$ lies above the sweep's band of validity is reported as NaN.
- AES17's -1 dBFS level: a plugin with a gain above 0 dB may clip at its output; that is part of what it does (W7.9 measures the maximum input level).

## Results for known test signals
From `MeasureDistortionTests` (2026-10-09, 0.30.0), 48 kHz, one channel, 999.76 Hz (coherent, bin 1365 of 65536) unless noted.
Expected values: the polynomial $y = x + 0.1x^2 + 0.05x^3 + 0.02x^5$ from its closed-form harmonics (`getPolynomialHarmonics`), the notch value with the
exact notch response at each harmonic; the hard clipper from its Fourier series, for THD+N with every harmonic up to the 20001st folded below Nyquist
(aliases on the same bin added with their signs); the noise cases from the noise power in the band 20 Hz ... 20 kHz (the share 19980/24000 of white
noise).

| Device | Level | Expected THD (dB) | THD | Expected THD+N (dB) | THD+N (bins) | Expected THD+N (notch) | THD+N (notch) |
|---|---|---|---|---|---|---|---|
| polynomial | -1 dBFS | -26.939 | -26.939 | -26.948 | -26.948 | -27.371 | -27.371 |
| polynomial | -10 dBFS | -36.024 | -36.024 | -36.025 | -36.025 | -36.475 | -36.475 |
| polynomial | -20 dBFS | -46.021 | -46.021 | -46.021 | -46.021 | -46.472 | -46.472 |
| hard clip at 0.5 | 0 dBFS | -12.672 (2nd ... 10th) | -12.672 | -12.887 (all harmonics, aliased) | -12.887 | - | -13.024 |
| 16-bit quantizer, TPDF dither | -1 dBFS | - | -127.1 | -93.115 ($q^2/4$ in band) | -93.103 | - | -92.443 |
| white Gaussian noise added, -70 dBFS (AES17 rms) | -20 dBFS | - | -85.5 | -50.796 | -50.809 | - | -50.159 |

The clipper's THD+N re the total (-12.887 dB) is -12.657 dB re the fundamental: 0.015 dB above its THD, the share of the harmonics above the 10th and of
the aliases.

**THD against level** (6.3.3, polynomial): 0 dBFS -25.87, -10 dBFS -36.02, -20 dBFS -46.02, -30 dBFS -56.02, -40 dBFS -66.02 dB, each within 0.01 dB of
the closed form. Below -10 dBFS the 2nd harmonic dominates and THD falls by 1 dB per dB.

**Harmonics against frequency**, Hammerstein system (the polynomial, then a 4th-order Butterworth low-pass at 2 kHz), -6 dBFS; 2nd / 3rd harmonic in dB re
the fundamental. Expected: $H_n |LP(n f)| / (H_1 |LP(f)|)$.

| Input frequency | stepped sine (6.3.2) | synchronized sweep (A.4.7) | sweep, polynomial alone |
|---|---|---|---|
| 100 Hz | -32.11 / -49.12 | -32.11 / -49.12 | -32.11 / -49.12 |
| 300 Hz | -32.11 / -49.13 | -32.11 / -49.13 | -32.11 / -49.12 |
| 700 Hz | -32.35 / -53.08 | -32.35 / -53.07 | -32.11 / -49.12 |
| 1000 Hz | -35.10 / -63.59 | -35.10 / -63.60 | -32.11 / -49.12 |
| 1500 Hz | -46.20 / -77.72 | -46.20 / -77.72 | -32.11 / -49.12 |
| 3000 Hz | - | -57.44 / -91.06 | -32.11 / -49.14 |

(The stepped frequencies are the coherent ones: 100.34, 300.29, 700.20, 999.76, 1500 Hz.) Largest error of the sweep against the closed form: polynomial
0.017 dB, Hammerstein 0.001 dB (harmonics above -80 dB).

**Oracle** `polynomial_thd_48k` (the prototype's `measure_thdn`, $x + 0.1x^2 + 0.05x^3$, -6 dBFS, 999.76 Hz, 65536 samples): the C++ unit gives the same
harmonics to 1e-5 dB and THD -32.0344 dB (prototype -32.0344 dB).

Tolerances in the test: harmonics, THD and both THD+N values of the polynomial 0.01 dB; clipper harmonics and THD 0.05 dB, its THD+N 0.01 dB; quantizer
and noise THD+N 0.3 dB (one random realisation); THD against level and the stepped Hammerstein harmonics 0.01 dB; swept harmonics 0.05 dB; FFT against
a direct DFT 1e-10.

**What the results teach**
- For a memoryless curve the harmonics do not depend on the frequency; a filter after the curve makes them fall where it attenuates $n f$ but not $f$.
  The sweep shows this in one measurement as accurately as the stepped sine.
- THD and THD+N tell different stories: a dithered 16-bit quantizer has no harmonics (THD -127 dB) but THD+N -93 dB; a clipper's harmonics above the band
  edge return as aliases that only THD+N sees.
- A notch-based analyzer reads a little lower THD for low harmonics (the notch skirt) and a little higher THD+N for noise (no low-pass at 48 kHz) than the
  FFT; the method belongs in the report.

## Implementation
```cpp
pluginlab::measure::DistortionSettings settings;     // sampleRate, frequencyHz (997), levelDbfs (-1), settleSeconds, measureSeconds, maximumHarmonic (10), notchQ (2), channels
pluginlab::measure::DistortionResult result = pluginlab::measure::measureDistortion(device, settings);
// result.frequencyHz (coherent), result.windowSamples, result.channels[c]: fundamentalDbfs, harmonicOrders, harmonicDb, thdDb, thdPercent, thdnDb, thdnNotchDb
auto byLevel = pluginlab::measure::measureDistortionVsLevel(device, settings, {0.0, -10.0, -20.0});
auto byFrequency = pluginlab::measure::measureDistortionVsFrequency(device, settings, {100.0, 1000.0, 5000.0});
pluginlab::measure::HarmonicResponse swept = pluginlab::measure::measureSweptHarmonics(device, sweepSettings, 5, frequencies);
// swept.channels[c].harmonicDb[n - 2][i], swept.channels[c].thdDb[i]
```
