# Maximum level, overload and gain non-linearity

Unit of W7.9. Code: `src/measure/Linearity.cpp` (`measureMaximumLevel`, `measureGainLinearity`), using the THD+N analysis of
[thd-and-thdn.md](thd-and-thdn.md) and the idle channel noise of [noise.md](noise.md); tests `tests/MeasureLinearityTests.cpp`.
Conventions: [README](README.md).

## Purpose
Where a plugin stops being linear, and how it fails beyond that point. A float plugin has no fixed full scale: some clip at 0 dBFS, some at an
internal "headroom" level, some saturate softly from -20 dBFS on, some stay linear far above 0 dBFS. The maximum input level is also the reference of
other AES17 tests (gain at -20 dB, dynamic range at -60 dB re this level). Gain non-linearity shows the other end: does the gain stay the same down to
the noise, or do small signals vanish (truncation without dither, a gate, denormal flushing)?

## Standard and sources
- **AES17-2015, 6.2.1 Maximum input level**: a 997 Hz sine, the analyzer with the standard low-pass filter; (a) **THD+N ratio method**: the level is
  increased until THD+N (6.3.1) reaches -40 dB; (b) **compression method**: increased "until the corresponding increase in EUT output level is 0,3 dB
  lower than the increase in the test level". The note recommends (a) for EUTs that hard clip and (b) for EUTs that soft clip.
- **AES17-2015, 6.2.6 Maximum output level**: the same two methods; the result is the output level at that point.
- **AES17-2015, 6.6.8 Overload behavior**: a 997 Hz sine at +3 dB above the maximum input level; THD+N above 20 % (-14 dB) "is a strong indication that
  rollover has occurred" (an overflow that wraps around instead of clipping).
- **AES17-2015, 6.3.7 Gain non-linearity** ("also known as linearity"): the idle channel noise level (6.4.2) is measured first; a 997 Hz sine at -5 dB
  re the maximum input level is the reference; its output through a band-pass at 997 Hz (the standard band-pass, or a frequency-domain band-pass no wider
  than 500 Hz) is the reference output level; the level falls in steps of at most 5 dB until the band-passed output is within 5 dB of the idle channel
  noise level; at each step the deviation = output level - (reference output level + test level - reference test level).
- The closed forms of the test devices: the Fourier series of the clipped sine (thd-and-thdn.md), the numerical harmonics of tanh
  (`reference::getShaperHarmonics`), quantization and TPDF dither (noise.md).

## Routine and analysis
**Maximum level** (`measureMaximumLevel`):
1. One evaluation at level L: the THD+N analysis of W7.5 (coherent sine, window the next power of two above 0.25 s: 16384 samples, 996.09 Hz at 48 kHz),
   giving THD+N (bins 20 Hz ... 20 kHz, re the total) and the output level (true rms through the standard low-pass).
2. Criterion: THD+N >= -40 dB (method a), or $G(L_\text{ref}) - G(L) \ge 0.3$ dB with $G = $ output level - input level and $L_\text{ref}$ = -40 dBFS
   (method b; our reading of "0.3 dB lower than the increase": the cumulative compression relative to the small-signal gain).
3. Search: from -20 dBFS upwards in 1 dB steps to the first level that meets the criterion (at most +24 dBFS: a float plugin may be linear above full
   scale); if -20 dBFS already meets it, downwards to the first that does not (at least -60 dBFS). Then bisection to 0.01 dB. Reported: the midpoint of
   the last interval, the output level there (6.2.6), THD+N and gain there.
4. "Not found" has two forms: the criterion is never met up to +24 dBFS (a linear device), or it is met at every level down to -60 dBFS
   (`exceededEverywhere`: the THD+N of the noise alone is above -40 dB; the THD+N method does not apply).
5. Overload (6.6.8): THD+N at the maximum + 3 dB; rollover if above -14 dB.

**Gain non-linearity** (`measureGainLinearity`):
1. Idle channel noise (6.4.2, dBFS CCIR-RMS).
2. From `maximumInputDbfs` - 5 dB downwards in 5 dB steps: the output in a frequency-domain band-pass of 500 Hz around the coherent 997 Hz (the rms of
   the bins within +-250 Hz; window 65536 samples), and the deviation from the ideal.
3. Stop when the band-passed output is within 5 dB of the idle channel noise level, at the latest at -140 dBFS (a device without idle noise would never
   stop).

## Band of validity and limits
- A plugin's maximum level depends on its settings (gain, drive, ceiling); AES17 asks for gain controls at their minimum. Each setting is its own
  measurement (W8, W9).
- The THD+N method needs a device whose THD+N is below -40 dB at moderate levels. A noisy device (noise at -40 dB) never passes; the unit says so instead
  of reporting a level.
- The criterion assumes one crossing. A device whose THD+N falls again above the first crossing (e.g. a level-dependent process) is reported at the first
  crossing above -20 dBFS.
- Clipping at exactly full scale: a coherent sine at 0 dBFS has samples exactly at +-1 (at 996.09 Hz with 16384 samples, sample 1024 is the peak). A
  device that maps +1 to -1 (wraparound) fails there; the result is 0 dBFS within the 0.01 dB resolution (-0.004 dB).
- The compression method uses the broadband output level (AES17's level: harmonics included). For tanh the harmonics add to the rms, so the fundamental alone
  would compress earlier.
- Gain non-linearity uses AES17's stop rule: the CCIR-RMS idle noise (wideband, weighted) against a 500 Hz band. With dither the band noise is about 18 dB
  below the CCIR-RMS level, so the steps stop while the deviation from noise is still small (0.04 dB at -90 dBFS, 16 bits).

## Results for known test signals
From `MeasureLinearityTests` (2026-10-09, 0.34.0), 48 kHz, one channel. Expected maximum input: the level where the closed-form THD+N (clipper: Fourier series
with every harmonic up to the 20001st folded below Nyquist; tanh: numerical harmonics up to the 20th) reaches -40 dB, or where the closed-form compression
(the rms of the sampled tanh output) reaches 0.3 dB, by bisection. Expected maximum output: the rms of the sampled clipped sine at the measured input level.

| Device | Method | Expected max. input (dBFS) | Measured | Expected max. output (dBFS) | Measured | Overload THD+N (+3 dB) | Rollover |
|---|---|---|---|---|---|---|---|
| hard clip at 0.5 (-6.02 dBFS) | THD+N -40 dB | -5.769 | -5.770 | -5.819 | -5.819 | -16.96 dB | no |
| gain +6 dB, then hard clip at 1 | THD+N -40 dB | -5.748 | -5.746 | 0.203 | 0.203 | -16.95 dB | no |
| 16-bit quantizer, no dither (clips at 1 - q) | THD+N -40 dB | 0.252 | 0.254 | 0.203 | 0.203 | -16.95 dB | no |
| tanh(x) | compression 0.3 dB | -8.459 | -8.457 | | -8.757 | -33.09 dB | no |
| tanh(x) | THD+N -40 dB | -9.076 | -9.074 | | -9.336 | -34.25 dB | no |
| wraparound beyond +-1 (integer overflow) | THD+N -40 dB | 0 (+1 wraps to -1) | -0.004 | | -0.004 | -0.80 dB | **yes** |
| gain -6 dB | THD+N -40 dB | none up to +24 dBFS | none | | | | |
| white noise -40 dB rms added | THD+N -40 dB | none (noise alone exceeds) | none | | | | |

A hard clipper reaches THD+N -40 dB 0.25 dB above its clipping level (clipping at -6.02 dBFS, maximum input -5.77 dBFS); the overload THD+N of a clipper
at +3 dB is -17 dB (no rollover), of a wraparound -0.8 dB (rollover).

**Gain non-linearity** (996.83 Hz, steps of 5 dB from -5 dBFS, band 500 Hz):
- gain -6 dB: no idle noise, 28 steps down to -140 dBFS, largest deviation below 1e-6 dB.
- 16-bit quantizer with TPDF dither: idle noise -92.46 dBFS CCIR-RMS; the steps stop at -90 dBFS. Deviation (expected: the band's share of the dither
  noise, $10\log_{10}(1 + N_\text{band}/P_\text{tone})$ / measured): -50 dBFS 0.0000/0.0003, -65 dBFS 0.0001/0.0016, -80 dBFS 0.0042/0.0101,
  -90 dBFS 0.0419/0.0418.
- 16-bit quantizer without dither: no idle noise, so the steps go down to -140 dBFS. Deviation: below 0.01 dB down to -60 dBFS, then -0.03 (-65), -0.05
  (-70), +0.10 (-75), -0.17 (-80), +0.56 (-85), +0.64 (-90), +1.01 dB (-95 dBFS); from -100 dBFS on the output is silent (every sample rounds to zero below
  half a step, -96.33 dBFS).

Tolerances in the test: maximum input 0.02 dB (resolution 0.01 dB), maximum output 0.01 dB; gain non-linearity: the gain below 1e-4 dB, dithered 0.02 dB
against the expected noise share; undithered below 0.01 dB down to -60 dBFS and silent below half a step.

**What the results teach**
- "0 dBFS" is not the maximum input level of a float plugin: a +6 dB gain stage before a clipper moves it to -5.7 dBFS, a gain stage alone has none.
- Hard and soft clipping are different stories: the clipper is clean until 0.25 dB past its threshold, tanh has THD+N -40 dB at -9 dBFS and compresses
  by 0.3 dB at -8.5 dBFS.
- Without dither small signals are not just noisier: the gain itself goes wrong by up to 1 dB, then the signal vanishes. With dither the gain is right to
  the noise floor.

## Implementation
```cpp
pluginlab::measure::MaximumLevelSettings settings;   // method (ThdN, Compression), thdnLimitDb (-40), compressionLimitDb (0.3), referenceLevelDbfs (-40),
                                                     // startDbfs (-20), lowestDbfs (-60), highestDbfs (+24), resolutionDb (0.01), frequencyHz, ...
pluginlab::measure::MaximumLevelResult level = pluginlab::measure::measureMaximumLevel(device, settings);
// level.found, level.exceededEverywhere, level.maximumInputDbfs, level.maximumOutputDbfs, level.thdnAtMaximumDb, level.gainAtMaximumDb,
// level.overloadThdnDb, level.rollover, level.frequencyHz, level.windowSamples, level.evaluations
pluginlab::measure::LinearitySettings linearity;      // maximumInputDbfs (0), stepDb (5), lowestDbfs (-140), bandwidthHz (500), ...
pluginlab::measure::LinearityResult curve = pluginlab::measure::measureGainLinearity(device, linearity);
// curve.idleNoiseDbfs, curve.inputDbfs, curve.outputDbfs, curve.deviationDb
```
