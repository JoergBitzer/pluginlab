# Band identification

Unit of W8.3. Code: `src/eqmodel/BandFit.cpp` (`fitEqModel`); tests
`tests/BandFitTests.cpp`. The model: [band-model.md](band-model.md).

## Purpose
From a measured frequency response to an EQ description: which bands, of
which type, at which frequency, gain and Q. This is the "reality" side of
"myth and reality" (the knobs are the myth, W8.6), and the base for mapping
knobs to bands (W8.4, W8.5). It is meaningful for an LTI system or the filter
of a Hammerstein system (W8.1 says which).

## Routine
| Step | What |
|---|---|
| 1 | magnitude in dB on a log grid (1/24 octave, 20 Hz ... 20 kHz) |
| 2 | delay from the phase slope at the lowest frequencies (no wrapping there) |
| 3 | linear phase if the phase without the delay stays within 2 degrees |
| 4 | flat model: the output gain alone |
| 5 | candidates for the largest remaining feature: pass filters at a steep end (RBJ, Butterworth 1 ... 4), a peak or dip (RBJ, Orfanidis; start: the half-gain width), shelves at either end |
| 6 | one start per candidate for the first band; then greedily the best candidate for the next feature, all bands refitted |
| 7 | pruning: a band is removed if the model stays as good without it |
| 8 | of the starts: the good one with the fewest bands, else the smallest residual |
| 9 | linear phase: a few final iterations with the real FIR |
| 10 | "no good description" if the rms residual is above 0.5 dB |

The fit: Levenberg-Marquardt on the dB residual, numerical Jacobian;
parameters log10 frequency, gain and log10 Q per band, the output gain.
Limits: peak Q at least 0.2 (a very broad peak imitates a shelf and an
offset), shelf and pass Q at least 0.3.

## Equivalences
Some designs are the same digital filter; the fit reports the first name.

| Reported | Equivalent |
|---|---|
| RBJ peak, Q | Zoelzer peak with a mapped Q (same bilinear shape) |
| RBJ high-/low-pass, Q 0.707 | Butterworth order 2 |
| RBJ shelf | Zoelzer 2nd-order shelf (Q 0.707) |
| state-variable (TPT) | the RBJ design of the same type |

Orfanidis differs from RBJ near Nyquist and is found (15 kHz case below).

## Band of validity and limits
- An LTI response (or the filter of a Hammerstein system): run W8.1 first.
- Up to 6 bands; bands that overlap strongly may be described by fewer or
  other bands with the same response: the residual is the honest measure.
- Two shelves can imitate a 3rd-order high-pass within 0.004 dB in 20 Hz ...
  20 kHz: the search prefers a single band when one suffices.
- The search tries every candidate as the first band: slow in a debug build
  (about 100 s for the test set), a few seconds in a release build.

## Results
From `BandFitTests` (2026-10-11, 0.45.0). Models from the band model's exact
responses; errors are the largest over the bands.

| Model | fs (kHz) | Recovered | Errors f / gain / Q |
|---|---|---|---|
| RBJ peak 1 kHz +6 dB Q 2 | 48 | the same | 0 / 0 / 0 |
| RBJ peak 3 kHz -8 dB Q 4 | 44.1 | the same | 0 / 0 / 0 |
| RBJ low shelf 150 Hz +5 dB | 96 | the same | 0 / 0 / 0 |
| RBJ high shelf 8 kHz -4 dB | 48 | the same | 0 / 0 / 0 |
| Butterworth high-pass order 3, 60 Hz | 48 | the same | 0 / - / - |
| low shelf 100 Hz +4, peaks 800 Hz -6 Q 2 and 4 kHz +3 Q 1, high shelf 12 kHz -3, output -1 dB | 48 | the same four bands | 0 / 0 / 0 |
| Orfanidis peak 15 kHz +6 dB Q 1 | 48 | the same (family found) | 0 / 0 / 0 |
| linear-phase peak 1 kHz +6 dB Q 2 | 48 | the same (linear phase found) | 0 / 0 / 0 |

"0": below 0.00005 % / dB / % (the residual is below 0.00001 dB rms).

| Other check | Result |
|---|---|
| measured: sweep through RBJ peak 2.5 kHz -5 dB Q 3 + low shelf 200 Hz +3 dB | both bands, 2500.0 Hz, -5.00 dB, Q 3.000; 200 Hz, +3.00 dB |
| comb filter y = x + 0.5 x(n - 48) | residual 1.83 dB rms: no good description |

Tolerances in the test: frequency 0.1 %, gain 0.01 dB, Q 0.5 %, the family
and the number of bands exact; measured: frequency 2.5 Hz, gains 0.02 dB.

## Implementation
```cpp
pluginlab::eqmodel::FitSettings settings;    // sampleRate, maximumBands,
                                             // goodResidualDb, ...
pluginlab::eqmodel::FitResult fit =
    pluginlab::eqmodel::fitEqModel(frequencies, response, settings);
// fit.model (bands, output gain), fit.linearPhase, fit.latencySamples,
// fit.rmsResidualDb, fit.goodDescription, fit.summary
```
