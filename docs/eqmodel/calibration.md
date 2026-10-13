# Calibration: myth and reality

Unit of W8.6. Code: `src/engine/Calibration.cpp` (`calibrate`,
`createCalibrationReport`); the host's `--calibrate`; tests
`tests/CalibrationTests.cpp` (one CTest per plugin,
`PluginLabCalibration_*`). Plan: `docs/design/W8-plan.md`, W8.6.

## Purpose
A knob that says 1 kHz does not always give a band at 1 kHz. The own PeakEQ of
W5 designs its filter for 44.1 kHz whatever rate the host runs at: at 48 kHz
the band sits 8.8 % high, at 96 kHz 2.18 times. Other plugins call a bandwidth
"Q", or show dB that are not the band's dB. The calibration finds these
deviations from the measurement alone, at 44.1, 48 and 96 kHz, and reports
only them (the author's answer 4). No corrected knob law is handed to the
matcher (W9): it needs a few examples, not the full behaviour.

## What it measures
For each band parameter of the mapping (W8.5: frequency, gain, Q, order):

| Step | What |
|---|---|
| 1 | the setting: the defaults, and what the mapping says the parameter needs to act (a gain for a frequency knob, a switch for a band) |
| 2 | the knob positions: 7 of a continuous knob (0.05 ... 0.95), every step of a discrete one (up to 16) |
| 3 | at each position and each rate: the text the plugin shows, its first number ("1.2 kHz" = 1200) and the text's resolution (half a unit of its last digit) |
| 4 | a synchronized sweep (-12 dBFS, 1 s, left channel), the band fit of W8.3 (up to 3 bands) on 20 Hz ... min(20 kHz, 0.45 fs) |
| 5 | the dominant band (largest gain; without a gain band a pass filter): its frequency, gain, Q or order against the number shown |

A position is not measured when the shown frequency lies outside the measured
band, the fit finds no band description, or the band sits at the edge of the
measured band (within 5 % of 20 Hz or of the top). A gain knob at 0 dB gives
no band: measured 0 dB.

## Tolerances and patterns
| Quantity | Tolerance (at least half a unit of the text's last digit) | Patterns |
|---|---|---|
| frequency | 1 % | "x 1.088" (one factor at every position), "varies: x a ... x b" |
| gain | 0.1 dB | "offset +0.50 dB", "x 0.500" (a multiple of the dB shown), "a x shown + b dB", "varies" |
| Q | 3 % | "x k", "a bandwidth in octaves, not a Q" (the measured Q is that of a bandwidth of the shown number of octaves: Q = sqrt(2^N) / (2^N - 1)), "varies" |
| order | 0 | "+1 orders", "varies" |

Over the rates (the conclusion):
- **follows the sample rate**: the frequency factor is host rate / design rate,
  the same design rate at every host rate (within 1 %), as shown at one of
  them. "The band is designed for 44.1 kHz whatever rate the host runs at":
  the PeakEQ fault.
- **the same at every rate**: the knob shows something else than it does.
- **depends on the sample rate**: deviates at some rates, as shown at others.

## The report
On the Developer page (0.55.0, W8.8): "Calibrate" runs it in a process of its
own with the progress bar, "View calibration" shows the report, the column
"Calibration" the state; `PluginLabHost --developer <plugin file> --calibration
[--view]` does the same from the command line. The report ends with the
interactions between bands (W8.7, [interactions.md](interactions.md)).
`PluginLabHost --calibrate <plugin file> <report> [<plugin id>]` uses the
mapping in the user's mapping folder (`~/.config/pluginlab/mappings`); without
one the survey proposes it first and writes it there (as `--map` does). The
report:

| Section | Content |
|---|---|
| Deviations | one row per parameter that deviates: the pattern at each rate (bold: outside the tolerance) and the conclusion; then "As shown at every rate: ..." |
| <parameter>: shown and measured | the positions of a parameter that deviates: the text and the measured value at each rate, with the factor or the difference |
| About the calibration | why, what it does, the tolerances, the patterns |

A plugin without deviations gets one line: "Everything as shown".

## Results
From `CalibrationTests` (2026-10-11, 0.50.0), Debug, 63 renders and about
65 s per plugin.

| Plugin | Result |
|---|---|
| Test EQ (correct) | everything as shown at 44.1, 48 and 96 kHz |
| Test EQ Fs Fault (the PeakEQ fault: designed for 44.1 kHz) | frequency: as shown at 44.1 kHz, x 1.089 at 48 kHz, x 2.177 at 96 kHz: "follows the sample rate: the band is designed for 44.1 kHz"; gain and Q as shown |
| Test EQ Knob Faults | frequency x 1.050, gain x 0.500 (the dB shown are not the band's dB), "Q" a bandwidth in octaves: all three "the same at every rate" |
| Reference EQ | everything as shown (frequency, gain, Q, order 1 ... 8) |

The PeakEQ case of W5 is reproduced from the calibration alone (the plan's
criterion): 48000 / 44100 = 1.0884, 96000 / 44100 = 2.1769. At 96 kHz the
two highest knob positions put the band above the measured band (9.5 kHz x
2.18 = 21 kHz); the pattern rests on the five positions below.

The mappings of the three test EQs are in `mappings/` (proposed by the survey,
checked against the plugin code by these tests).

## Limits
- One band at a time, at the mapping's context; a band whose deviation
  depends on another band's setting is the interaction question (W8.7).
- The band model decides what "measured" means: a plugin whose Q follows
  another definition (e.g. the bandwidth of the digital filter instead of
  RBJ's analogue Q) shows that as a Q deviation, which is the truth in the
  band model's terms.
- Frequencies near the top of the band (a filter designed for 44.1 kHz
  running at 96 kHz puts a 17 kHz knob at 37 kHz) cannot be measured; the
  pattern rests on the positions that can.
