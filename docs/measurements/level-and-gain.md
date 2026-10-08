# Level and gain

Unit of W7.1. Code: `src/measure/Gain.cpp` (`measureGain`), analyzer `src/measure/Analyzer.cpp`; tests `tests/MeasureGainTests.cpp`, oracle case
`tests/oracle/gain_rbj_peak_997_48k.json`. Conventions (dBFS, analyzer): [README](README.md).

## Purpose
How much louder or quieter a plugin makes a signal at one frequency, and how equal its channels are. The most basic measurement, and the reference of the
frequency response (which is given relative to the gain at 997 Hz).

## Standard and sources
- **AES17-2015, 6.2.2 Gain**: "The test signal shall be a 997 Hz sine wave at -20 dB relative to the maximum input level. The analyzer shall include the
  standard low-pass filter. The gain shall be the ratio of the output level of the EUT to the level of the test signal. It shall be reported in dB."
  For a very noisy EUT the standard band-pass filter may be used (5.2.9).
- **AES17-2015, 6.2.4 Gain matching between channels**: the same test signal on all channels at once; "the gain matching shall be the greatest
  difference in levels, expressed in dB".
- **AES17-2015, 5.2.3 Level meter** (true rms, at least 25 ms, whole periods), **5.2.5 standard low-pass filter**, **5.2.10 frequency-domain band-pass
  filters** and **annex A.2** (rms measurements in the frequency domain by Parseval's theorem; no window for a whole number of periods).
- The selective (lock-in) gain is the same quantity as the prototype's `measure_gain` (Python, `measure/level.py`).

## Stimulus
A sine of 997 Hz (`frequencyHz`) at -20 dBFS (`levelDbfs`, i.e. -20 dB re the maximum input level 0 dBFS), the same on all channels (`channels`, default 2),
starting at phase 0, `settleSeconds` (0.5 s) plus the measurement window long.

## Routine
1. Render the stimulus through the device (fresh state).
2. Discard the first `settleSeconds` (switching-on transient, latency, parameter smoothing of the plugin).
3. Measurement window: a whole number of periods of the test frequency, at least `measureSeconds` (1 s) and at least 25 ms: 997 periods = exactly 1 s at
   every integer sample rate for 997 Hz.
4. Filter input and output with the standard low-pass filter (identity at 44.1/48 kHz).
5. For each channel: the true rms of the filtered input and output in the window, and the complex amplitude of the test frequency in both.
6. Report per channel and the matching over the channels.

## Analysis
With $x$ the filtered input, $y$ the filtered output, $N$ samples in the window from $n_0$:

$$L_\text{in} = 20\lg\!\left(\sqrt2\,\sqrt{\tfrac1N\textstyle\sum x^2}\right)\ \text{dBFS (as generated)},\qquad
G = 20\lg\frac{y_\text{rms}}{x_\text{rms}}\ \text{dB}\quad\text{(AES17 6.2.2, broadband)}$$

$$X = \frac2N\sum_{n=0}^{N-1} x[n_0 + n]\,e^{-j2\pi f n/f_s},\quad Y \text{ likewise},\qquad
G_\text{sel} = 20\lg\frac{|Y|}{|X|},\quad \varphi = \arg\frac{Y}{X}$$

$$\text{matching} = \max_c G_c - \min_c G_c\quad\text{(AES17 6.2.4)}$$

- The **broadband gain** $G$ is the standard's gain: everything at the output counts (the tone, its harmonics, noise, DC).
- The **selective gain** $G_\text{sel}$ counts only the tone (a one-bin frequency-domain band-pass); it is what a gain at one frequency of a frequency
  response means, and AES17's alternative for noisy equipment.
- The **phase** $\varphi$ includes the latency of the device (a delay of $d$ samples adds $-360°\,f d/f_s$); W7.3/W7.4 separate the two.
- Input and output pass the same standard low-pass filter, so its passband ripple (0.0022 dB at most) cancels in the gain; the reported input level is the
  level as generated.

## Band of validity and limits
- One frequency per measurement (20 Hz ... 20 kHz possible; below 25 Hz the 25 ms rule is met by the whole-period rule anyway). For other frequencies than
  997 Hz the window length is rounded to whole samples, so it holds a whole number of periods only up to that rounding (leakage below 1e-4 dB for 1 s).
- A device that is not settled after `settleSeconds` (slow smoothing, an envelope) gives a wrong gain; the fingerprint's "settles within" test tells.
- A time-varying device (tremolo, noise) gives the average over the window.
- Levels below about -120 dBFS lose precision in float samples (the device works in float).
- For a distorting device the broadband gain contains the harmonics (by design, see results); the unit does not judge whether the device is "linear enough"
  (that is W7.5 THD+N and W7.9).

## Results for known test signals
From `MeasureGainTests` (run 2026-10-08, 0.26.1), at 48 kHz unless stated; expected values from the exact answers of the reference processors.

| Device | Expected | Measured (broadband gain, phase / selective gain) |
|---|---|---|
| gain -6 dB, 44.1 kHz | -6 dB, 0 degrees | -6.00000 dB, 0.0000 degrees |
| gain -6 dB, 48 kHz | -6 dB, 0 degrees | -6.00000 dB, 0.0000 degrees |
| gain -6 dB, 96 kHz (standard low-pass active) | -6 dB, 0 degrees | -6.00000 dB, 0.0000 degrees |
| gain -6 dB, polarity inverted | -6 dB, 180 degrees | -6.00000 dB, 180.0000 degrees |
| RBJ peak 1 kHz +6 dB Q 2 | $|H(997\,\text{Hz})|$ = 5.99906 dB, 0.4865 degrees | 5.99906 dB, 0.4865 degrees |
| RBJ low-pass 1 kHz Q 0.71 | $H(997\,\text{Hz})$: -2.94874 dB, -89.7549 degrees | -2.94874 dB, -89.7549 degrees |
| Thiran delay 10.5 samples (order 3) | 0 dB, -78.5138 degrees ($-360° \cdot 997 \cdot 10.5 / 48000$, wrapped) | 0.00000 dB, -78.5138 degrees |
| right channel 0.5 dB lower (gain matching) | 0.5 dB | 0.50000 dB |
| $x + 0.1x^2 + 0.05x^3$ at -20 dBFS | broadband 0.00358 dB (tone, DC and harmonics), selective 0.00326 dB ($c_1 + \frac34 c_3 A^2$) | 0.00358 dB / 0.00326 dB |
| 8-bit quantizer with TPDF dither | broadband +0.0132 dB (adds $q^2/4$), selective 0 dB | 0.01258 dB / -0.00065 dB |
| 8-bit quantizer without dither | no closed form | 0.03627 dB / 0.03180 dB |

Tolerances in the test: 1e-4 dB for the exact devices, 0.002 dB for the dithered quantizer (a random signal; one 1 s window).

**Oracle**, the prototype's lock-in measurement (`measure_gain`, 997 Hz, -20 dBFS) against the selective gain and phase of the C++ unit:
- `gain_simple_997_48k`, a plain gain of -6 dB (the simplest case; it must never fail): -6.000000 dB, 0 degrees in both (tolerance 1e-4 dB, 1e-4 degrees);
- `gain_rbj_peak_997_48k`, the prototype's RBJ peak (1 kHz, +6 dB, Q 2): 5.999058 dB, 0.48647 degrees in both (tolerance 0.001 dB, 0.01 degrees). The
  peak is used because it is the only reference device of the prototype; at 997 Hz it is nearly a pure gain, so the phase is checked by the low-pass and
  the delay above.

**What the results teach**
- For a linear device broadband and selective gain are the same; the difference of the two is a first sign of distortion or noise.
- Undithered quantization is not noise: with a sine that repeats with the sample grid the error repeats too, contains a component at the fundamental and
  moves even the selective gain (0.03 dB at 8 bits, -20 dBFS). With TPDF dither the error is independent of the signal and the tone is untouched.
- A plain gain or a polarity inversion cannot show an error of the phase (0 or 180 degrees whatever the convention); the low-pass (-89.75 degrees) and the
  delay (-78.51 degrees) can. The phase contains the latency: a delay of $d$ samples gives $-360° \cdot 997 \cdot d / f_s$ (wrapped to $\pm180°$), which is
  why the unit of W7.3 measures the delay separately.

## Implementation
```cpp
pluginlab::measure::GainSettings settings;            // sampleRate, frequencyHz (997), levelDbfs (-20), settleSeconds, measureSeconds, channels
pluginlab::measure::GainResult result = pluginlab::measure::measureGain(device, settings);
// result.channels[c]: inputLevelDbfs, outputLevelDbfs, gainDb, selectiveGainDb, phaseDegrees; result.matchingDb
```
