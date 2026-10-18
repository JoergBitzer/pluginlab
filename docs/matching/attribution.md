# The matcher: why not closer

Unit of W9.5. Code: `attribute` in `src/engine/PluginMatch.cpp`; the section
"Why not closer" of the match report; tests: the attribution part of
`tests/PluginMatchTests.cpp` (CTest `PluginLabMatchAttribution`). Plan:
`docs/design/W9-plan.md`, 4.4; `planning.md` W9: "master reproduced on each
test plugin or the residual quantified and attributed".

## The causes
When a target misses the tolerance, every cause that applies is named, in
this order; the first is the main one:

| Cause | Test |
|---|---|
| a knob at the end of its range | a released parameter within 1 % of its range of a bound |
| the bands interact | the target's mapping has two or more bands and they do not add in dB (W8.7, measured); before the band fits, because a fit describes interacting bands as another family |
| another design family | the band fit (W8.3) of the target at the found setting (careful sweep): its band nearest to a master band (within an octave) is of another family; families with the same magnitude count as one (RBJ, state variable, linear phase; RBJ, Butterworth and Linkwitz-Riley pass filters) |
| a band missing | the master has more bands (gain bands of at least 0.5 dB, all others) than the fitted target |
| not found | none of these; if the target has a mapping and did not start from the model, it is matched once more from the model start (W9.4); if that succeeds, the match counts and no cause is named |

The master's bands: the curve itself, or the band fit of a plugin master.

## Results
From the attribution tests (2026-10-13, 0.60.0), Debug, tolerance strict; the
cause the code has built in is the first one named:

| Master | Target | Residual | Main cause, evidence |
|---|---|---|---|
| Reference EQ, RBJ peak 8 kHz +6 dB Q 2 | Test EQ Limited (frequency knob ends at 5 kHz; new test build) | 4.41 dB | a knob at the end of its range: "Frequency at the end of its range (5000)" |
| Reference EQ, Orfanidis peak 10 kHz +6 dB Q 1 | Test EQ (RBJ) | 0.81 dB | another design family: "the master's band is Orfanidis peak 10000 Hz +6.0 dB, the target's RBJ cookbook peak 10436 Hz +5.9 dB; the designs differ most at 19331 Hz" |
| curve: peaks 300 Hz +5 dB, 3 kHz -4 dB | Test EQ (one band) | 3.98 dB | a band missing: "the master has 2 bands, the target makes 1" |
| curve: two overlapping RBJ peaks (1 kHz and 1.41 kHz, +9 dB, in series) | Test EQ Parallel (six parameters, model start) | 0.60 dB | the bands interact: "up to 3.49 dB against the sum of single bands" (second: another design family, the fit's description of the parallel response) |

## Limits
- The knob-range cause does not check that relaxing the bound would help
  (the plan's proposal); a parameter at its bound while the residual sits
  elsewhere is named all the same.
- The family comparison rests on the band fit; a plugin whose response the
  fit describes poorly gets the family cause less reliably.
- Not found is a statement about the optimizer, not about the plugin.
- A knob with a large linear range keeps its useful part near one end (the
  Reference EQ's Q, 0.1 ... 20: Q 0.5 ... 2 is 7 % of the knob); from a start an
  octave away the optimizer slid against that end and was reported "at the
  end of its range" although the cause is the start; the model start avoids
  it (W9.7, the teaching case).
- A target matched against a bound costs many renders (the Limited case:
  230 renders, 15.7 s): the optimizer keeps trying steps that the bound cuts
  away.
