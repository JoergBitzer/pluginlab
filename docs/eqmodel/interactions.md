# Interactions between bands

Unit of W8.7. Code: `src/engine/Interactions.cpp` (`measureInteractions`,
`createInteractionReport`); part of the host's `--calibrate` report; tests
`tests/InteractionTests.cpp` (one CTest per plugin, `PluginLabInteractions_*`).
Test plugins: `tests/plugins/TestTwoBandEqPlugin.cpp`. Plan:
`docs/design/W8-plan.md`, W8.7.

## Purpose
The band model (W8.2) describes an EQ as bands in series: their responses
multiply, their levels add in dB. The calibration (W8.6) and a match band by
band (W9) rely on that. Many analogue designs do not work this way: bands in
parallel (each adds its share to the dry signal), bands that share a stage, an
output stage that reacts to the level. Then two bands together are not the sum
of the two alone. W8.7 measures whether a plugin's bands add, and by how much
they do not.

## What it measures
For each pair of bands of the mapping that have a frequency and a gain:

| Step | What |
|---|---|
| 1 | band A an RBJ peak at 1 kHz, band B one at 0, 1/3, 1 and 3 octaves above (set through the mapping, `makeSettingForBand`), Q 1, gains +6 / +6 dB and +6 / -6 dB |
| 2 | four responses: neither band (the defaults), A alone, B alone, both (synchronized sweep, -12 dBFS, 1 s) |
| 3 | the deviation from additivity, dB(A+B) - dB(A) - dB(B) + dB(neither), 20 Hz ... 20 kHz; its largest value and where |
| 4 | a case interacts if the largest deviation exceeds 0.05 dB; the plugin's bands add if no case interacts |

The settings come from the mapping, but the comparison does not depend on
how exact its laws are: the single bands and the pair use the same settings.

## The test plugins
| Plugin | Topology | Expected |
|---|---|---|
| Test EQ Two Bands | two RBJ peaks in series, H = H1 H2 | the bands add (deviation 0 up to the measurement) |
| Test EQ Parallel | the same peaks in parallel, H = 1 + (H1 - 1) + (H2 - 1) | at one frequency: +6 / +6 dB give 1 + 2 (A - 1) = 9.51 dB instead of 12 dB (-2.49 dB); +6 / -6 dB give 1 + (A - 1) + (1/A - 1) = 3.50 dB instead of 0 dB; less with distance |
| Reference EQ | one band | nothing to combine |

The two test EQs are also the first multi-band test of the survey and the
mapping (W8.4, W8.5): bands at 300 Hz and 3 kHz by default.

## Results
From `InteractionTests` (2026-10-12, 0.54.0), 48 kHz, Debug, 25 renders and
7 s per plugin.

| Case (band A 1 kHz, Q 1) | Test EQ Two Bands (series) | Test EQ Parallel |
|---|---|---|
| same frequency, +6 / +6 dB | 0.000 dB | -2.480 dB at 1016 Hz (theory -2.486 dB at 1 kHz) |
| same frequency, +6 / -6 dB | 0.000 dB | +3.493 dB (theory +3.502 dB) |
| 1/3 octave, +6 / +6 / +6 / -6 dB | 0.000 dB | -2.405 / +3.361 dB |
| 1 octave | 0.000 dB | -1.919 / +2.576 dB |
| 3 octaves | 0.000 dB | -0.497 / +0.588 dB |
| conclusion | the bands add | the bands interact: up to 3.49 dB |

Parallel bands influence each other far beyond their overlap: three octaves
apart still half a dB, because each band's skirt adds to the dry signal
instead of multiplying it. The Reference EQ has one band: nothing to combine.

The survey and the mapping found both bands of the two test EQs (gain,
frequency, Q of bands 1 and 2, laws as in the code). One inaccuracy: the
plugin-wide Bypass (JUCE's VST3 wrapper) was proposed as the on/off switch
of band 1, because it acted only with band 1's gain in its context; the
mappings in `mappings/` carry a note.

## Limits
- A switch that acts on all bands (a bypass) is proposed by the mapping as
  the switch of the band whose gain made it act.
- Pairs only; three bands that interact only together are not looked for.
- Peaks only; shelves and pass filters of a plugin are combined in the
  interaction question only through their peaks' behaviour.
- One level (-12 dBFS): an output stage that reacts to the level is an
  interaction only if the plugin is LTI otherwise (the class of W8.1 decides
  first).
