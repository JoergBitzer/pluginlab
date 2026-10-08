# Frequency response

Unit of W7.2. Code: `src/measure/FrequencyResponse.cpp` (`measureSteppedResponse`, `measureMultitoneResponse`, `measureSweptResponse`); tests
`tests/MeasureResponseTests.cpp`, oracle case `tests/oracle/prototype_sweep_rbj_peak_48k.json`. Conventions (dBFS, analyzer): [README](README.md);
the gain at one frequency: [level-and-gain.md](level-and-gain.md).

## Purpose
How the gain of a plugin changes with frequency: the curve of an EQ, the band limits of a converter model, the ripple of a "flat" plugin. Every
method gives the complex response $H(f)$ (magnitude and phase) at a list of frequencies, absolute and relative to the gain at 997 Hz. The phase here
contains the latency of the plugin; its separation and the group delay follow in W7.3/W7.4.

## Standard and sources
- **AES17-2015, 6.2.3 Frequency response**: "The test signal shall be a sine wave at -20 dB relative to the maximum input level. The frequency of the
  test signal shall be varied from 20 Hz to the upper band-edge frequency in steps of no more than one octave. One of the test frequencies shall be
  997 Hz. The analyzer shall include the standard low-pass filter. The output level of the EUT shall be measured at each frequency. The results shall be
  presented as a graph with frequency on the x-axis, shown logarithmically, and the level relative to the level at 997 Hz on the y-axis."
  This is the **stepped sine** (also the method of the Audio Precision analyzers).
- **AES17-2015, 5.4 / table 3**: the standard third-octave frequencies (IEC 61260-1 nominal values), used here as the steps.
- **AES17-2015, annex A.3 (synchronous multitone), A.3.4**: many tones at once, each on a bin of the analysis period, no window; the response is the
  ratio of output and stimulus in each tone bin. "Correlated with" the sine method (A.3.9).
- **AES17-2015, annex A.4 (exponential sine sweep), A.4.5**: the impulse response by deconvolution of a sweep, the response by its Fourier transform.
  pluginlab uses the **synchronized swept sine**: A. Novak, P. Lotton, L. Simon, "Synchronized Swept-Sine: Theory, Application, and Implementation",
  J. Audio Eng. Soc. 63(10), 2015, pp. 786-798 (sweep $x(t) = \sin(2\pi f_1 L e^{t/L})$ with $L = k/f_1$, $k$ an integer; analytic inverse filter, eq. 43).
  Its predecessor: A. Farina, "Simultaneous measurement of impulse response and distortion with a swept-sine technique", 108th AES Convention, Paris
  2000, preprint 5093. The division by the spectrum of the excitation (here: of the stimulus deconvolved the same way, a "reference channel"):
  S. Mueller, P. Massarani, "Transfer-Function Measurement with Sweeps", J. Audio Eng. Soc. 49(6), 2001, pp. 443-471.
- Schroeder phases of the multitone: M. R. Schroeder, "Synthesis of low-peak-factor signals and binary sequences with low autocorrelation", IEEE Trans.
  Information Theory 16(1), 1970, pp. 85-89 (in pluginlab the form for any tone spacing, see `src/signals`).

## The three methods

### A. Stepped sine (AES17 6.2.3)
**Stimulus:** one sine per frequency (default: the 31 standard third-octave frequencies 20 Hz ... 20 kHz, 997 Hz in place of 1 kHz), -20 dBFS, each step
`| Hann fade-in 10 ms | latency + settling 0.1 s | measurement window | Hann fade-out 10 ms |`; the window holds whole periods (rounded to samples), at least
0.1 s and at least 10 periods (`pluginlab_signals` `makeSteppedSine`). About 8 s at 48 kHz.

**Routine:** render; input and output through the standard low-pass filter; in each step's window the complex amplitude of the step frequency in input
and output (one DFT bin) and the true rms of both.

**Analysis:** $H(f_i) = Y_i / X_i$ (selective, with phase); the broadband level ratio $20\lg(y_\text{rms}/x_\text{rms})$ as AES17 reads (it contains harmonics and
noise; reported as `broadbandDb`); relative level $20\lg|H(f_i)| - 20\lg|H(997)|$.

**Validity:** 20 Hz ... 20 kHz (the steps); every point is measured with full signal level, so it has the largest dynamic range per point. The window is a
whole number of periods only up to rounding to samples (leakage below 1e-3 dB here). A latency longer than the settling time must be given
(`latencySamples`), otherwise the window starts before the plugin has settled.

### B. Synchronous multitone (AES17 A.3)
**Stimulus:** 31 log-spaced tones from 20 Hz to 20 kHz, each on a bin of the period (65536 samples at 44.1/48 kHz, 131072 at 96 kHz: the power of two
at or above 1.3 s), Schroeder phases, -20 dBFS rms in total; two periods long.

**Routine:** render; the first period lets the device reach its periodic steady state, the second is analysed; for each tone the complex amplitude in
input and output (one bin, no window: synchronous).

**Analysis:** $H(f_k) = Y_k / X_k$ at the tone frequencies; relative to the tone nearest 997 Hz (996.8 Hz at 48 kHz).

**Validity:** exact for a linear time-invariant device whose impulse response is shorter than one period (1.3 s); only at the tone frequencies. Each tone
has about 15 dB less level than the whole signal (31 tones), and the rounding of the float samples of all tones lands on every tone: the floor is about
**-132 dB** relative to the input, so a deep stop band (an 8th-order low-pass far above its corner) is not measurable below that. Fast (2.7 s at 48 kHz).

### C. Synchronized swept sine (AES17 A.4, Novak et al.)
**Stimulus:** sweep from 5 Hz (two octaves below the passband) to 0.95 of Nyquist (at most 40 kHz), about 4 s (the integer $k$ fixes the real length:
3.37 s at 48 kHz), -20 dBFS peak, 0.1 s of silence before and at least 1 s after.

**Routine:**
1. Render; deconvolve the response with the analytic inverse filter: $h = \mathrm{IFFT}(Y \tilde X)/(f_s A)$ (`pluginlab_signals` `deconvolveSweptSine`). The
   linear impulse response lies at time 0 (plus the latency), the responses of the harmonics at $-L\ln n$ (the end of the circular buffer).
2. The peak of the linear response (searched in the first half of the buffer).
3. A window around the peak: before it at most half the distance to the 2nd-harmonic response ($L\ln 2/2$, 0.14 s at 48 kHz; the band-limited impulse
   rings before its peak), after it 0.5 s; raised-cosine tapers on the outer halves.
4. **Reference channel:** the stimulus itself is deconvolved and windowed the same way (around time 0).
5. $H(f) = \mathrm{DTFT}_\text{window}\{h\}(f) \,/\, \mathrm{DTFT}_\text{window}\{h_\text{stimulus}\}(f)$ at any list of frequencies (default 1/24 octave from 20 Hz to
   20 kHz, 240 points), the phase with the time origin of the deconvolution (latency included).

**Why the reference channel:** without it the response showed errors of up to 0.37 dB and 2.8 degrees at the band edges (the ripple of the abruptly
starting and stopping sweep, and the ringing of the band-limited impulse cut by the window): the same for every device. The stimulus has the same ripple
and the same window, so the ratio removes them (as a dual-channel analyser divides by its reference channel; Mueller and Massarani 2001). Step by step
in the tests: window before the peak 0.05 -> 0.25 s (at most $L\ln2/2$): 0.37 -> 0.13 dB; sweep start 10 -> 5 Hz: 0.12 dB (now at the upper edge);
reference channel: 0.002 dB.

**Validity:** 20 Hz ... 20 kHz (from twice the start frequency to the stop frequency / 1.05); devices whose impulse response (latency + tail) fits into
the window (0.5 s after the peak) and whose latency is less than half the deconvolution buffer; floor about **-120 dB**. Mildly nonlinear devices: the
harmonic responses lie outside the window (that is what the synchronization is for; their transfer functions are the subject of W7.5).

## Results for known test signals
From `MeasureResponseTests` (2026-10-09, 0.27.0): the largest deviation from the exact response $H(e^{j\omega})$ of the reference processor, in the band of
validity of the method; magnitude and level re 997 Hz only where the exact response is above -80 dB, phase above -60 dB. "Floor": the highest level
the method shows where the exact response is below -120 dB.

| Device | Method | Points | Magnitude (dB) | Re 997 Hz (dB) | Phase (degrees) | Floor |
|---|---|---|---|---|---|---|
| RBJ peak 1 kHz +6 dB Q 2 | stepped sine | 31 | 7.5e-05 | 6.2e-05 | 2.0e-04 | - |
| | multitone | 30 | 1.3e-08 | 1.5e-08 | 8.4e-08 | - |
| | swept sine | 240 | 2.0e-04 | 2.0e-04 | 2.0e-03 | - |
| RBJ low shelf 200 Hz -9 dB Q 0.71 | stepped sine | 31 | 2.5e-04 | 2.3e-04 | 1.8e-03 | - |
| | multitone | 30 | 1.6e-08 | 1.4e-08 | 9.6e-08 | - |
| | swept sine | 240 | 2.1e-03 | 2.1e-03 | 2.1e-02 | - |
| Butterworth low-pass 8th order 1 kHz | stepped sine | 22 | 1.4e-04 | 1.8e-04 | 4.4e-03 | -132 dB |
| | multitone | 21 | 3.6e-06 | 3.6e-06 | 5.7e-06 | -132 dB |
| | swept sine | 175 | 2.8e-03 | 2.8e-03 | 4.3e-03 | -120 dB |
| linear-phase FIR 2047 taps (latency 1023) | stepped sine | 31 | 6.0e-04 | 7.0e-04 | 9.8e-03 | - |
| | multitone | 30 | 8.5e-09 | 8.0e-09 | 8.8e-08 | - |
| | swept sine | 240 | 1.5e-05 | 1.5e-05 | 7.3e-05 | - |
| integer delay 100 samples | stepped sine | 31 | 7.4e-04 | 8.2e-04 | 2.5e-03 | - |
| | multitone | 30 | 7.6e-13 | 7.7e-13 | 8.7e-12 | - |
| | swept sine | 240 | 1.2e-06 | 1.5e-06 | 8.3e-06 | - |
| RBJ peak, 44.1 kHz | stepped sine | 31 | 2.7e-05 | 1.9e-05 | 4.4e-04 | - |
| | multitone | 31 | 1.4e-08 | 1.6e-08 | 6.2e-08 | - |
| | swept sine | 240 | 2.0e-04 | 2.0e-04 | 2.0e-03 | - |
| RBJ peak, 96 kHz | stepped sine | 31 | 1.4e-05 | 1.2e-05 | 5.2e-04 | - |
| | multitone | 30 | 7.9e-09 | 7.9e-09 | 3.9e-08 | - |
| | swept sine | 240 | 2.0e-04 | 2.0e-04 | 2.0e-03 | - |

Tolerances in the test: stepped sine and swept sine 0.01 dB and 0.05 degrees, multitone 1e-4 dB and 1e-3 degrees.

**Oracle** (`prototype_sweep_rbj_peak_48k`): the Python prototype measures its RBJ peak (1 kHz, +6 dB, Q 2) with its own implementation of the synchronized
sweep (it replaced the Farina sweep on 2026-10-08): 0.0002 dB from the exact response and 0.0002 dB from the C++ sweep measurement, 30 frequencies
50 Hz ... 15 kHz (tolerance 0.01 dB).

**What the results teach**
- The three methods agree with the exact answer far better than any plugin difference that matters (0.01 dB); they differ in what else they offer:
  the stepped sine has the full level in every point and the broadband reading of AES17; the multitone is exact and fast but only at its tones and with
  less dynamic range per tone; the sweep gives any frequency resolution, the phase with the latency, and the harmonics for free.
- A sweep measurement is only as good as its window and its normalisation: the raw deconvolution was off by 0.37 dB at 20 Hz; the reference channel
  brought it to 0.002 dB.
- A linear-phase FIR with 1023 samples of latency and an integer delay are measured as exactly as a minimum-phase filter: the latency only turns the
  phase (it is part of $H$ here).

## Implementation
```cpp
pluginlab::measure::SteppedResponseSettings stepped;      // frequencies (AES17 third octaves), levelDbfs, latencySamples, settleSeconds, ...
pluginlab::measure::MultitoneResponseSettings multitone;  // lowestHz, highestHz, numberOfTones, levelDbfs, periodSamples
pluginlab::measure::SweepResponseSettings sweep;          // startHz (5), stopHz, approximateSeconds, windowBeforeSeconds, windowAfterSeconds, frequencies
pluginlab::measure::FrequencyResponse response = pluginlab::measure::measureSweptResponse(device, sweep);
// response.frequencyHz, response.referenceHz, response.validFromHz/validToHz,
// response.channels[c]: response (complex), magnitudeDb, relativeDb, phaseDegrees, broadbandDb (stepped sine), referenceGainDb
```
