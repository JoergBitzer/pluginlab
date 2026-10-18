# Zipper noise per parameter (fingerprint, 0.65.0)

The author: "do we have a test for zipper noise per parameter? That should be added to the developer page, especially to the standard report page."

## What it is
A host hands a plugin new parameter values in steps: automation once per block (in a DAW without sample-accurate automation), a knob as fast as the
GUI sends. A plugin that applies a new value at once changes its output in a step at every new value; every step is a click, many in a row sound like a
zipper. A plugin that smooths its parameters (a ramp within the block, a one-pole smoother per sample) turns the steps into a continuous change.
Smoothing once per block (half the way to the target at every block start, the test plugin Block Smoothing) does not help: during a ramp it still jumps at
every block.

## The measurement
Per continuous parameter that changes the audio (the switches and choices jump by nature, a bypass is left out), the others at the setting B:
1. Five tones (100, 315, 1000, 3150, 10000 Hz, moved onto bins of the 65536-sample analysis, -20 dBFS each, fixed random phases) at 48 kHz, block 512.
2. A fresh instance, the careful delivery (poke, setting, settle), 0.5 s of tones as run-in, then 65536 samples (1.37 s) of tones while the parameter goes from
   `lowSetting` to `highSetting` (0.25 -> 0.75), **one new value per block** (128 steps).
3. The same with the parameter held at the low and at the high position.
4. The residual of each output: the power of the spectrum (4-term Blackman-Harris window, side lobes -92 dB) between 20 Hz and 20 kHz, more than 45 Hz away
   from every tone and its harmonics 2 ... 5, relative to the power within 45 Hz of the tones.

Why 45 Hz: a smooth ramp of the level spreads a tone by some tens of hertz (a 24 dB ramp in 1.37 s: -67 dB at 30 Hz, -83 dB at 60 Hz off); steps once per block
put their energy at the multiples of the block rate (93.75 Hz) around every tone and broadband. The guard is just under half the block rate.

Judgement: zipper noise when the residual while moving is more than `zipperExcessDb` (10 dB) above the larger still one **and** above `zipperAboveDb`
(-60 dB re the tones). The first condition keeps a plugin's own noise or distortion from counting; the second is the line between the two kinds of plugin:

| Test case | residual while moving | still | judged |
|---|---|---|---|
| synthetic: a gain in steps once per block, -12 -> +12 dB | -46.8 dB | -146.9 dB | zipper |
| synthetic: the same gain changed every sample | -100.5 dB | | none |
| Test Gain (applies the gain at once) | -46.5 dB | -146.0 dB | zipper |
| Test Block Smoothing (half the way per block) | -46.6 dB | -146.0 dB | zipper |
| Test Gain Ramp (linear ramp within each block, `applyGainRamp`) | -70.0 dB | -146.0 dB | none |
| Reference EQ (no smoothing): Frequency / Gain / Q | -30.4 / -64.0 / -82.1 dB | about -146 dB | zipper / none / none |

The Reference EQ's frequency knob is the classic case: the biquad's coefficients jump at every block and the filter's state does not fit the new
coefficients, a transient each time.

## In the report and on the Developer page
- Summary: "zipper noise while a parameter moves (one new value per block)": "none", "N of M" (bold) or "not judged"; the Developer page has the column
  "Zipper noise".
- Findings: the parameters with zipper noise, with the excess and the level.
- Section "Zipper noise": a table per parameter: change between the test positions, residual moving and still, excess, the verdict; a parameter that does
  not change the tones between the test positions (with the others as in B) is "not judged".

## Limits
- One ramp per parameter (0.25 -> 0.75 in 1.37 s); a plugin that smooths only large jumps, or only some ranges, is judged by this one ramp.
- The steps come at a fixed block rate; a knob from a GUI arrives less regularly (the same steps, the energy less concentrated).
- The tones are on channel 1 (L = R); a parameter that acts on L - R only (a width) changes nothing here and is "not judged".
- The thresholds are in the settings file (`zipperExcessDb`, `zipperAboveDb`); -60 dB re the tones is a measurement line between "steps" and "smooth", not a
  listening test.
