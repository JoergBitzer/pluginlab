# Noise: idle channel noise, dynamic range, mains products

Unit of W7.7. Code: `src/measure/Noise.cpp` (`measureIdleNoise`, `measureDynamicRange`, `measureMainsProducts`, `getWeightedRms`), the weightings
`src/measure/Weighting.cpp`; tests `tests/MeasureNoiseTests.cpp`. Conventions: [README](README.md); THD+N: [thd-and-thdn.md](thd-and-thdn.md).

## Purpose
What a plugin adds when there is nothing or almost nothing to process: the noise floor at idle (dither, "analogue" noise, denormal tricks), the
noise and distortion around a quiet signal (the dynamic range, the plugin's real resolution), and hum (many analogue-modelling plugins add mains
hum on purpose). A plugin that truncates without dither is silent at idle but not around a quiet signal, so both measurements are needed.

## Standard and sources
- **AES17-2015, 6.4.2 Idle channel noise level**: for a digital input, digital zero as the test signal; the output through the standard low-pass filter
  (5.2.5) and the standard weighting filter (5.2.7); the rms level, reported as **dBFS CCIR-RMS**.
- **AES17-2015, 6.4.1 Dynamic range** (also known as signal-to-noise ratio): a 997 Hz sine at -60 dB relative to the maximum input level. The output
  goes through the standard low-pass, the standard notch at 997 Hz (5.2.8) and the standard weighting filter. The dynamic range is the maximum output
  level (6.2.6) minus that rms level, reported as **dB CCIR-RMS**. It includes all harmonic, inharmonic and noise components.
- **AES17-2015, 5.2.7 Standard weighting filter**: ITU-R BS.468-4 with an additional gain of -5.63 dB, which puts unity gain at 2 kHz ("CCIR-RMS"),
  with the response and tolerances of table 1.
- **ITU-R BS.468-4**, Measurement of audio-frequency noise voltage level in sound broadcasting (normative reference of AES17): the weighting curve,
  0 dB at 1 kHz and +12.2 dB at 6.3 kHz. The closed-form magnitude used here is the network's
  $|H(f)| \propto f / |h_1(f) + j h_2(f)|$ with $h_1 = -4.737338981378384\cdot10^{-24} f^6 + 2.043828333606125\cdot10^{-15} f^4 - 1.363894795463638\cdot10^{-7} f^2 + 1$,
  $h_2 = 1.306612257412824\cdot10^{-19} f^5 - 2.118150887518656\cdot10^{-11} f^3 + 5.559488023498642\cdot10^{-4} f$, as it is commonly published (e.g.
  in the Wikipedia article "ITU-R 468 noise weighting"). It is normalised here to 0 dB at 1 kHz and **verified against AES17 table 1** (results below).
- **IEC 61672-1:2013**, Electroacoustics - Sound level meters - Part 1, annex E: the A-weighting
  $A(f) = \frac{f_4^2 f^4}{(f^2 + f_1^2)\sqrt{(f^2+f_2^2)(f^2+f_3^2)}\,(f^2+f_4^2)}$ with $f_1$ = 20.598997 Hz, $f_2$ = 107.65265 Hz, $f_3$ = 737.86223 Hz,
  $f_4$ = 12194.217 Hz, normalised to 0 dB at 1 kHz. AES17 does not use A-weighting; it is added because "dB(A)" is the figure in most data sheets.
- **AES17-2015, 6.5.1 Power line (mains) related products**: digital zero in; frequency-domain band-pass filters (5.2.10) no wider than half the mains
  frequency at M times the mains frequency, M = 1 ... 5; the power line level is the rms sum of the five levels.
- Quantization noise: a uniform quantizer with step $q$ has error power $q^2/12$; with TPDF dither (triangular, +-1 LSB) the total noise is $q^2/4$
  and independent of the signal. A full-scale sine against $q^2/12$ gives the textbook $6.02N + 1.76$ dB. Sources: B. Widrow, I. Kollár, *Quantization
  Noise*, Cambridge University Press 2008; S. P. Lipshitz, R. A. Wannamaker, J. Vanderkooy, "Quantization and dither: a theoretical survey",
  J. Audio Eng. Soc. 40(5), 1992, pp. 355-375.

## Stimuli
- Idle channel noise and mains products: digital zero (a buffer of zeros) after a settling time of 0.5 s.
- Dynamic range: a sine at 997 Hz moved to the nearest bin of the window (996.83 Hz at 48 kHz, 65536 samples) at -60 dBFS.

## Routine and analysis
1. Render; the output through the standard low-pass filter (identity at 44.1 and 48 kHz).
2. **Weighted level from the spectrum**: $\mathrm{rms}^2 = \frac{1}{N^2}\sum_{k=0}^{N/2} c_k\,|W(f_k)|^2\,|G(f_k)|^2\,|X_k|^2$ with $c_k = 2$ (1 for DC and
   Nyquist), $W$ the weighting and $G$ an extra filter (the notch). The weighting curves are applied as the magnitude of the analogue network on each
   bin, so there is no bilinear warping near Nyquist. For a stationary signal this is the same rms that the time-domain filter chain would give.
   Four weightings: **unweighted** (the standard low-pass only; at 44.1/48 kHz everything up to Nyquist, DC included), **20 Hz ... 20 kHz** (the AES17
   passband), **CCIR-RMS**, **A**. Levels in dBFS after AES17 3.12 (rms relative to a full-scale sine: $20\log_{10}(\mathrm{rms}\sqrt2)$).
3. **Idle channel noise** (6.4.2): the four levels of the output.
4. **Dynamic range** (6.4.1): $G$ = the standard notch (RBJ notch at the coherent test frequency, Q 2). It is zero on the test frequency's bin, so
   the tone is removed completely; the notch's skirt also removes some noise around 997 Hz, as AES17's analogue notch would. The result is
   `maximumOutputDbfs` (0 dBFS) minus the residual level, for every weighting.
5. **Mains products** (6.5.1): the window is a whole number of seconds (1 s: bins every 1 Hz, so every multiple of 50 or 60 Hz lies on a bin with a
   rectangular window). The band at $M f_\text{mains}$ is the rms of the bins strictly inside $\pm f_\text{mains}/4$ (half the mains frequency wide:
   25 bins at 50 Hz), computed by single-bin DFTs. The total is the rms sum over M = 1 ... 5.

## Band of validity and limits
- The maximum output level of a plugin is taken as 0 dBFS (a full-scale sine). A plugin that clips earlier or has gain will get its own reference from
  W7.9 (maximum input level); the dynamic range then shifts by that difference.
- **Idle noise alone can mislead**: a plugin without dither (or with a noise gate, or one that switches its processing off at silence) is silent at
  idle (below -300 dBFS here) and still has the full quantization error around a quiet signal. Its dynamic range shows it. The opposite also happens:
  a plugin with dither has idle noise but no signal-dependent distortion. (AES17 6.4.5 low-level noise modulation measures the transition; not part
  of this unit.)
- The unweighted level includes DC (the standard low-pass passes it); the weighted levels and the 20 Hz ... 20 kHz band do not. A DC offset of 0.001
  shows as -56.99 dBFS unweighted and nothing weighted.
- White noise is weighted by a fixed amount: 20 Hz ... 20 kHz -0.80 dB, CCIR-RMS +0.88 dB, A -2.72 dB at 48 kHz (the power share of the curve over
  0 ... 24 kHz). At other sample rates these shares change: the CCIR-RMS curve still has -27.8 dB at 20 kHz, and the unweighted value grows with the band.
- Mains bands as wide as AES17 allows (half the mains frequency) catch hum at a mains frequency that is not the one assumed: 50 Hz hum lies inside the
  60 Hz band (45 ... 75 Hz). Narrower bands (`bandwidthHz`) separate them.
- Results for noise are estimates from one realisation of the noise: 65536 samples give a spread of a few hundredths of a dB (more for the weighted
  values, which rest on fewer effective bins).

## Results for known test signals
From `MeasureNoiseTests` (2026-10-09, 0.32.0), 48 kHz, one channel. Expected values for white noise: the known noise power times the power share of
the weighting (and the notch), integrated over 0 ... 24 kHz on a 0.25 Hz grid (independent of the unit's bin sum). Dithered 16-bit: rms $q/2$ with
$q = 2^{-15}$.

**Weighting curves** against the standards:

| Curve | Against | Largest difference | Note |
|---|---|---|---|
| CCIR-RMS | AES17 table 1, 21 frequencies 31.5 Hz ... 31.5 kHz | 0.080 dB (100 Hz, tolerance 1.0 dB) | 6.3 kHz: 6.587 dB against 6.57 dB (BS.468's 12.2 dB minus 5.63; the table prints 6.6 +- 0.01, which is the rounded value) |
| A | IEC 61672-1 nominal values, 31.6 Hz ... 15.8 kHz at the exact base-10 frequencies | 0.040 dB (31.6 Hz) | the table is rounded to 0.1 dB |

**Idle channel noise** (dBFS, expected / measured):

| Device | unweighted | 20 Hz ... 20 kHz | CCIR-RMS | A |
|---|---|---|---|---|
| 16-bit quantizer, TPDF dither | -93.32 / -93.32 | -94.12 / -94.12 | -92.44 / -92.48 | -96.04 / -96.06 |
| 24-bit quantizer, TPDF dither | -141.48 / -141.49 | -142.28 / -142.29 | -140.61 / -140.65 | -144.21 / -144.22 |
| white Gaussian noise, rms -80 dB (sample rms) | -76.99 / -77.01 | -77.79 / -77.81 | -76.11 / -76.14 | -79.71 / -79.74 |
| 16-bit quantizer, no dither | silent (below -300) | | | |
| DC offset 0.001 | -56.99 / -56.99 | - | - | - |

**Dynamic range** (dB re 0 dBFS, -60 dBFS at 996.83 Hz, expected / measured):

| Device | unweighted | 20 Hz ... 20 kHz | dB CCIR-RMS | dB(A) |
|---|---|---|---|---|
| 16-bit quantizer, TPDF dither | 93.46 / 93.41 | 94.28 / 94.25 | 92.52 / 92.53 | 96.32 / 96.32 |
| 24-bit quantizer, TPDF dither | 141.62 / 141.65 | 142.45 / 142.48 | 140.68 / 140.75 | 144.48 / 144.52 |
| 16-bit quantizer, no dither | 98.09 (6.02 N + 1.76) / 98.07 | 98.73 | 96.65 | 100.71 |

**Mains products** (dBFS, window 1 s, bands 25 Hz / 30 Hz wide): a hum adder at 50 Hz (and one at 60 Hz) with the fundamental at -90 dBFS and
harmonics at -6, -12, -20, -30 dB: every line within 0.001 dB (-90.000, -96.000, -102.000, -110.000, -120.000), power line level -88.777 dBFS
(expected -88.777). 50 Hz hum measured as 60 Hz mains: M = 1 reads -90.000 dBFS (inside the band 45 ... 75 Hz), M = 2 nothing.

Tolerances in the test: weighting curves within the AES17 table tolerances (6.3 kHz: 0.02 dB against 6.57 dB) and 0.05 dB against IEC 61672-1; noise
levels and dynamic ranges 0.15 dB; mains lines and total 0.001 dB; undithered dynamic range within 2 dB of 6.02 N + 1.76.

**What the results teach**
- Dither costs 4.77 dB of unweighted dynamic range ($q^2/4$ against $q^2/12$: 93.4 against 98.1 dB at 16 bits) and buys a noise floor that does not
  depend on the signal.
- The same noise reads 3.6 dB apart between CCIR-RMS and A (white noise: +0.88 against -2.72 dB): a data sheet's "dynamic range" without the
  weighting is not comparable.
- The notch removes a little noise with the tone (16 bits: 93.46 dB dynamic range against 93.32 dB idle noise, unweighted).

## Implementation
```cpp
pluginlab::measure::NoiseSettings settings;      // sampleRate, settleSeconds, measureSeconds, testFrequencyHz (997), testLevelDbfs (-60), maximumOutputDbfs (0), notchQ (2), channels
pluginlab::measure::IdleNoiseResult idle = pluginlab::measure::measureIdleNoise(device, settings);
// idle.channels[c]: unweightedDbfs, bandDbfs, ccirRmsDbfs, aWeightedDbfs (or .get(Weighting))
pluginlab::measure::DynamicRangeResult range = pluginlab::measure::measureDynamicRange(device, settings);
// range.channels[c].residual (dBFS), range.channels[c].dynamicRange (dB, the same four weightings)
pluginlab::measure::MainsSettings mains;         // mainsHz (50), harmonics (5), bandwidthHz (0: half the mains frequency), measureSeconds (1, whole seconds), channels
pluginlab::measure::MainsResult hum = pluginlab::measure::measureMainsProducts(device, mains);
// hum.channels[c]: lineDbfs[M - 1], totalDbfs
// weightings alone: pluginlab::measure::getWeightingGain(Weighting::CcirRms, f), getBs468Gain(f), getAWeightingGain(f)
```
