# Inter-channel crosstalk

Unit of W7.8. Code: `src/measure/Crosstalk.cpp` (`measureCrosstalk`, `getCrosstalkFrequencies`); tests `tests/MeasureCrosstalkTests.cpp`.
Conventions: [README](README.md). Gain matching between channels (AES17 6.2.4), the other half of the planned W7.8, is part of the gain unit:
[level-and-gain.md](level-and-gain.md).

## Purpose
How much of one channel appears in the other. A plugin has no capacitances, so any crosstalk is a decision: an M/S stereo-width control, a
"console emulation" that leaks a little on purpose, a mono-summed sidechain, or a bug (a channel loop that reads the wrong buffer). The measurement shows
which, and whether it depends on frequency.

## Standard and sources
- **AES17-2015, 6.5.2 Inter-channel crosstalk ratio** ("also known as separation"): one channel is driven, the others get digital zero; the driven
  channel gets a sine at -20 dB re the maximum input level, varied from 20 Hz to the upper band edge in steps of no more than one octave; the analyzer
  uses the standard low-pass filter; the ratio of the level on each undriven channel to the level on the driven channel, in dB, plotted over log
  frequency.
- **AES17-2015, A.3.8** (multitone): the crosstalk as the level of the tone in the undriven channel relative to the same tone in the driven channel,
  which is the selective form used here as the main result.
- Mid/side width: M = (L + R)/2, S = w (L - R)/2, L' = M + S, R' = M - S (the usual definition, as in `reference::ChannelMatrix::makeWidth`). With only L
  driven: L' = (1 + w)/2 L, R' = (1 - w)/2 L, so the crosstalk is $20\log_{10}|(1-w)/(1+w)|$.

## Stimulus
For each frequency of the list (20, 40, 80, ..., 10240 Hz and 20 kHz: octave steps over the passband) and each channel in turn: a sine at -20 dBFS on that
channel, digital zero on the others, 0.2 s settling, then a window of whole periods of at least 0.5 s.

## Routine and analysis
1. Render; every output channel through the standard low-pass filter.
2. **Selective level** of each channel: the amplitude of the test tone (one-bin DFT over whole periods: a frequency-domain band-pass of one bin, AES17
   5.2.10). **Broadband level**: the true rms of the window (AES17's level: tone, noise, everything).
3. Crosstalk $c_{d \to r}(f) = 20\log_{10}(L_r / L_d)$ for the driven channel d and every receiving channel r, both selective and broadband. Also the worst
   (largest) selective crosstalk over all pairs and frequencies.

## Band of validity and limits
- The selective form measures only what is coherent with the tone; the broadband form adds the noise of the undriven channel. For a noisy plugin the
  broadband value is a noise measurement (AES17's form gives "-77 dB crosstalk" for independent channels with noise at -100 dB below).
- The phase of the leak is not reported. A width above 1 leaks with inverted polarity (R' = (1 - w)/2 L < 0); $w$ and $1/w$ give the same magnitude
  (1.5 and 0.667: -13.98 dB) with opposite sign. The sign matters when the channels are summed to mono (W7.10 null test, W8).
- The floor is the float precision of the samples (below -200 dB for exact zeros). The test signal is -20 dBFS, so a non-linear leak (crosstalk that
  depends on level) would need other levels (`levelDbfs`).
- Time-varying crosstalk (an auto-panner) is averaged over the window.

## Results for known test signals
From `MeasureCrosstalkTests` (2026-10-09, 0.33.0), 48 kHz, stereo, selective crosstalk at all 11 frequencies from 20 Hz to 20 kHz (range of the values):

| Device | Expected L -> R | Measured L -> R | Expected R -> L | Measured R -> L |
|---|---|---|---|---|
| crosstalk -40 dB (`makeCrosstalk`) | -40.000 | -40.000 ... -40.000 | -40.000 | -40.000 ... -40.000 |
| crosstalk -90 dB | -90.000 | -90.000 ... -90.000 | -90.000 | -90.000 ... -90.000 |
| width 1.5 (wider) | -13.979 | -13.979 ... -13.979 | -13.979 | -13.979 ... -13.979 |
| width 0.5 (narrower) | -9.542 | -9.542 ... -9.542 | -9.542 | -9.542 ... -9.542 |
| width 0 (mono) | 0.000 | 0.000 ... 0.000 | 0.000 | 0.000 ... 0.000 |
| asymmetric: L' = L, R' = R + 0.01 L | -40.000 | -40.000 ... -40.000 | none | none (below -200) |
| independent (identity) | none | none (below -200) | none | none (below -200) |

**Crosstalk rising with frequency** (-40 dB through a first-order Butterworth high-pass at 10 kHz into the other channel; expected $k|H(f)|$, dB):
20 Hz -95.361 / -95.361, 160 Hz -77.300 / -77.300, 1280 Hz -59.269 / -59.269, 5120 Hz -47.675 / -47.675, 10240 Hz -42.872 / -42.872,
20 kHz -40.180 / -40.180 (all 11 frequencies within 0.001 dB). The 6 dB per octave of a capacitive leak is visible below the corner.

**Noise instead of crosstalk**: independent channels with white Gaussian noise at -100 dB (sample rms) on each: the broadband crosstalk reads -76.98 dB
(expected: the noise relative to the -20 dBFS tone's rms, -76.99 dB) at every frequency; the selective crosstalk stays below -116 dB.

Tolerances in the test: matrices and the filtered leak 0.001 dB (exact zeros: below -200 dB); the noise case 0.3 dB, selective at least 15 dB below the
broadband value.

**What the results teach**
- A stereo-width control is crosstalk by design: width 0.5 already puts the other channel at -9.5 dB.
- Asymmetric crosstalk (one direction only) is visible only because each channel is driven in turn.
- AES17's broadband ratio mixes crosstalk and noise; the selective ratio separates them.

## Implementation
```cpp
pluginlab::measure::CrosstalkSettings settings;  // sampleRate, levelDbfs (-20), frequencies (empty: octaves 20 Hz ... 20 kHz), settleSeconds, measureSeconds, channels
pluginlab::measure::CrosstalkResult result = pluginlab::measure::measureCrosstalk(device, settings);
// result.frequencyHz, result.selectiveDb[driven][receiving][i], result.broadbandDb[driven][receiving][i], result.worstSelectiveDb
```
