# Phase response and group delay

Unit of W7.4. Code: `src/measure/Phase.cpp` (`measurePhaseResponse`, `getSegmentGroupDelay`), the impulse response `src/measure/SweptImpulse.cpp`;
tests `tests/MeasurePhaseTests.cpp`. Conventions: [README](README.md); the sweep: [frequency-response.md](frequency-response.md); the delay that is removed
first: [delay-and-polarity.md](delay-and-polarity.md).

## Purpose
What a plugin does to the timing of the frequencies: the phase response without its constant delay (is the plugin minimum-phase, linear-phase, an
all-pass?), the group delay (how late each frequency band arrives, e.g. the low end of a steep high-pass or a linear-phase EQ's latency), and the phase
between the channels (a stereo plugin that shifts one channel against the other changes the image).

## Standard and sources
- **AES17-2015, 6.8.3 Input-to-output phase response** (optional): "This test measures the deviation of EUT phase from linear with frequency." (a) Transfer
  function method: "The input-to-output phase response of the EUT shall be the difference between the phase of the transfer function and a linear fit to
  this phase." (b) Sine wave method: the measured phase minus "phase (degrees) = EUT delay (s) x frequency (Hz) x 360". The note: "the phase curve may need
  to be unwrapped". Result: a graph over log frequency, or "a specification indicating the worst-case variation in phase deviation from linear over the
  measured frequency range, for example, '+1,0/-2,5 degrees from 20 Hz to 20 kHz'".
- **AES17-2015, 6.8.4 Group delay vs frequency**: "g = -d phi / df ... The change in phase d phi can be approximated by the difference of neighboring
  phase measurements."
- **AES17-2015, 6.2.7 Inter-channel phase response**: the phase of each channel relative to a reference channel, "also possible to derive ... from the
  phase response (see 6.8.3) by subtraction of one channel response from another"; annex A.4.6 and A.4.8 (the same from the sweep).
- Group delay from the impulse response without differentiation: $\tau_g(\omega) = \mathrm{Re}\left\{\frac{\sum_n n\,h[n]\,e^{-j\omega n}}{\sum_n h[n]\,e^{-j\omega n}}\right\}$
  (the derivative of $\ln H$ with respect to $\omega$; J. O. Smith III, *Introduction to Digital Filters with Audio Applications*, W3K Publishing 2007,
  section "Numerical Computation of Group Delay"). Phase and group delay in general: A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal Processing*,
  3rd ed., section 5.1.

## Stimulus
The synchronized sweep of the frequency response (shared `SweptImpulse`): 5 Hz ... 0.95 Nyquist, -20 dBFS, with the window around the linear peak and the
reference channel.

## Routine
1. The impulse response of each channel and its peak $d$ (the delay of AES17 6.8.2 a).
2. On a grid of 1/48 octave from 20 Hz to 20 kHz: $H(f)$ (window ratio, as in the frequency response) and the **exact group delay** of the window
   ($\tau_g$ of the device window minus $\tau_g$ of the reference window, by the formula above).
3. **Phase without the delay** (6.8.3 b): $\arg\big(H(f)\,e^{j2\pi f d/f_s}\big)$, unwrapped along the grid. Each step is predicted from the group delay (mean of
   both points) and the multiple of $2\pi$ nearest to the prediction is taken, so a steep phase (a high-Q all-pass) between two grid points is followed.
4. **Deviation from linear** (6.8.3 a): a straight line $a + b f$ fitted (least squares) to that phase in the **passband**: the band of validity where the gain is
   within 40 dB of its largest value (in a deep stop band the phase is noise). The deviation $\varphi - (a + bf)$ and its extremes ("+max/-min degrees") over the
   passband; the delay of the line, $d - b f_s / 360$ samples.
5. **Group delay by differences** (6.8.4): $-(\varphi_{k+1} - \varphi_k)/(2\pi\,\Delta f/f_s) + d$ at the geometric midpoints, for comparison.
6. **Inter-channel phase** (6.2.7): $\arg(H_c / H_1)$, wrapped to +-180 degrees.

## Band of validity and limits
- 20 Hz ... 20 kHz (the sweep's band). Below about 50 Hz the window (0.14 s before, 0.5 s after the peak) limits the group delay: errors up to 0.18
  samples (4 microseconds at 48 kHz) at 20 Hz against 0.03 samples above 50 Hz.
- The group delay and the phase deviation mean something only in the passband; below about -40 dB of the largest gain the float precision of the
  output limits the phase derivative (Butterworth 4th order at 5.3 kHz, -58 dB: 0.26 samples off), and in a deep stop band the phase is noise.
- The "linear fit" of AES17 depends on the frequency grid and band: on a log grid it is weighted towards the high frequencies, so a fractional-delay
  all-pass, whose group delay rises towards Nyquist, gets a fitted delay of 37.18 instead of 37.25 samples and a deviation of +14/-4 degrees. For a
  converter (a delay plus a little filtering, AES17's case) this does not matter; for filters the group delay curve is the clearer result.
- The phase includes a polarity inversion (+-180 degrees); W7.3 reports the polarity separately.

## Results for known test signals
From `MeasurePhaseTests` (2026-10-09, 0.29.0), 48 kHz, one channel; errors against the exact response: phase where the exact gain is above -60 dB, group
delay within 40 dB of the largest gain (the passband).

| Device | Delay removed (peak) | Fitted delay | Deviation from linear (degrees) | Phase error (degrees) | Group delay error below 50 Hz (samples) | above 50 Hz (samples, where) | Difference method error (samples) |
|---|---|---|---|---|---|---|---|
| RBJ all-pass 1 kHz Q 4 | 0 | 3.408 | +231.4/-209.2 | 2.8e-03 | 0.18 | 0.020 (50 Hz) | 0.18 |
| RBJ peak 1 kHz +6 dB Q 2 | 0 | 0.047 | +18.5/-20.1 | 2.0e-03 | 0.12 | 0.014 (51 Hz) | 0.12 |
| Butterworth low-pass 4th order 1 kHz | 22 | 17.292 | +99.7/-51.8 | 2.9e-03 | 0.095 | 0.033 (2958 Hz) | 0.095 |
| linear-phase FIR 255 taps (latency 127) | 127 | 127.000 | +0.000/-0.000 | 8.5e-05 | 0.011 | 0.002 (12.9 kHz) | 0.011 |
| Thiran 37.25 samples (order 3) | 37 | 37.178 | +14.4/-3.9 | 1.8e-04 | 0.011 | 0.0025 (18.0 kHz) | 0.011 |
| inverted, delay 100 | 100 | 100.000 | +0.000/-0.000 | 9.7e-06 | 0.0006 | 0.0013 (17.2 kHz) | 0.0005 |

Inter-channel phase (stereo device, the left channel unchanged): right channel one sample later: within 4.6e-06 degrees of $-360° f/f_s$; right channel
through an RBJ all-pass at 1 kHz: within 0.016 degrees of $\arg H$.

Tolerances in the test: phase 0.05 degrees; group delay below 50 Hz 0.48 samples (10 microseconds), above 50 Hz 0.048 samples (1 microsecond);
linear-phase FIR: fitted delay 127 within 0.001, deviation from linear below 0.01 degrees; inter-channel phase 0.05 degrees.

**What the results teach**
- A linear-phase filter has a deviation from linear of 0 degrees and a constant group delay equal to its latency; every minimum-phase EQ has a phase
  deviation of the order of its gain (+-20 degrees for a 6 dB peak), and an all-pass turns the phase by 360 degrees with no change in the magnitude.
- The group delay of a filter is not its latency: the Butterworth low-pass has its peak at 22 samples, a fitted delay of 17.3 and a group delay that
  peaks near its corner.
- AES17's difference approximation and the exact formula agree where the phase is smooth; the exact formula needs no unwrapping and no grid.

## Implementation
```cpp
pluginlab::measure::PhaseSettings settings;       // sampleRate, levelDbfs (-20), frequencies (default 1/48 octave 20 Hz ... 20 kHz), channels
pluginlab::measure::PhaseResponse result = pluginlab::measure::measurePhaseResponse(device, settings);
// result.frequencyHz, result.differenceFrequencyHz, result.validFromHz/validToHz,
// result.channels[c]: delaySamples, phaseDegrees, fitInterceptDegrees, fitDelaySamples, deviationFromLinearDegrees, deviationMaxDegrees/MinDegrees,
//                     groupDelaySamples, groupDelayDifferenceSamples, interChannelDegrees
```
