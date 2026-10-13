# The matcher: matching plugins

Unit of W9.4. Code: `src/engine/PluginMatch.cpp` (`matchPlugins`,
`makeModelStart`, `createPluginMatchReport`), the host's `--match`; tests
`tests/PluginMatchTests.cpp` (CTest `PluginLabPluginMatching`) and
`PluginLabHostMatch`. Plan: `docs/design/W9-plan.md`, 4.1 ... 4.5.

## The main case
The author: a user owns one EQ and wants to hear how several new ones
compare. The owned EQ at its setting is the master; each new EQ is a target.
The matcher sets every target so that its magnitude response equals the
master's, within the parameters the user released; the user then listens.

## How
| Step | What |
|---|---|
| master | a plugin at a setting (measured through a live instance; it must be LTI too) or a band-model curve (exact, no render) |
| targets | one after another, each with its own live instance (W9.3): the start (the user's values, switches as the user set them), the check (LTI, two renders agree), the released parameters (indices, each over its whole range), the match within the budget (default 15 s), the found setting in the plugin's texts |
| model start (optional) | the master's bands (a curve, or the band fit of a plugin master, W8.3) set through the target's mapping (`makeSettingForBand`, W8.5) on the target's bands, sorted by frequency: a start near the answer even from far away |
| tolerance | a level (exact 0.01, strict 0.1 dB default, medium 0.25, loose 0.5 dB) |
| report | per target: matched or not within the tolerance (with the reason the optimizer stopped), largest and rms deviation, the found setting, renders and seconds; a refused target with its reason |

From the command line: `PluginLabHost --match <job file> <report>`; the job:

```json
{
  "master": {"curve": [{"family": "RBJ cookbook", "type": "Peak", "frequency": 1000, "gain": 6, "q": 2}]},
  "targets": [
    {"plugin": "/path/My EQ.vst3", "release": ["Gain", "Frequency", "Q"], "start": {"Band 1 On": 1.0}, "modelStart": true}
  ],
  "tolerance": "strict",
  "seconds": 15
}
```

A plugin master: `"master": {"plugin": "/path/Owned EQ.vst3", "setting": {"Gain": 0.75, ...}}`
(normalised values by parameter name). The model start uses the target's
mapping in the user's mapping folder.

## Results
From `PluginMatchTests` (2026-10-13, 0.59.0), Debug, tolerance strict:

| Master | Target (released: gain, frequency, Q) | Result | Renders, time |
|---|---|---|---|
| Reference EQ, RBJ peak 1 kHz +6 dB Q 2 | Test EQ, from about +3 dB, 700 Hz, Q 1.5 | matched, 0.029 dB: Gain 5.97, Frequency 1000, Q 1.99 | 23, 5.7 s |
| same | Test EQ Knob Faults | matched, 0.026 dB: the knobs show Gain 11.95, Frequency 954, Q 0.72 (they lie; the matcher measures) | 28, 6.0 s |
| same | Test EQ Fs Fault (48 kHz) | matched, 0.094 dB at Frequency 922 (a 44.1 kHz design: 919 in theory) | 19, 5.6 s |
| Reference EQ, Orfanidis peak 10 kHz +6 dB Q 1 | Test EQ (RBJ) | not within the tolerance: 0.81 dB (near Nyquist) | 31, 6.0 s |
| curve: RBJ peaks 300 Hz +5 dB Q 1, 3 kHz -4 dB Q 2 | Test EQ Two Bands (six parameters) | matched, 0.009 dB | 15, 5.5 s |
| Reference EQ, RBJ peak | Test EQ Switch (band switched on by the user) | matched | |
| Reference EQ, RBJ peak | Test EQ Drive, drive on | refused: not LTI | |
| Reference EQ, RBJ peak | Test EQ from 38 Hz, model start | matched at the start (0.000 dB), one render | 1, 5.6 s |

Of the 5.5 ... 6 s per target, about 4.5 s are the checks (the system class
with the careful delivery); the match itself takes about one second.

## Limits
- The attribution of a residual (why not closer) is W9.5.
- The released parameters move over their whole range; narrowing a range is
  in the parameter space but not yet in the job file.
- Bands of the model start are assigned by frequency order; a target with
  fewer bands than the master gets the lowest ones.
