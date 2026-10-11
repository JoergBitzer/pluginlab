# EQ band model

Unit of W8.2. Code: `src/eqmodel/EqModel.cpp` (library `pluginlab_eqmodel`);
tests `tests/EqModelTests.cpp`, the plugin test in
`tests/PluginMeasurementTests.cpp`. Plan: `docs/design/W8-plan.md`.

## Purpose
A common language for what an EQ does: a band is a type, a frequency, a gain,
a width and an order, realised by a design family. The model computes the
exact response of every family; the Reference EQ plugin uses the same code,
so model and plugin cannot drift apart. The band identification (W8.3) fits
this model to measured responses.

## The model
| Element | Values |
|---|---|
| type | low-/high-pass, band-pass (two kinds), notch, all-pass, peak, low/high shelf |
| frequency | Hz, limited to 0.45 fs |
| gain | dB (peak, shelves) |
| width | Q (RBJ's definition on the analog prototype) |
| order | Butterworth 1 ... 8, Linkwitz-Riley 2/4/8, Zoelzer shelves 1/2 |
| family | see the next table |
| model | bands in series, an output gain; latency = sum of the bands' latencies |

| Family | Types | Phase |
|---|---|---|
| RBJ cookbook | all | minimum |
| Orfanidis | peak | minimum |
| Zoelzer (DAFX) | peak, shelves | minimum |
| State variable (TPT) | all (RBJ transfer function) | minimum |
| Butterworth, Linkwitz-Riley | low-/high-pass | minimum |
| Linear phase (RBJ magnitude) | all but the all-pass | linear |

The linear-phase family keeps the magnitude of the RBJ design and makes the
phase linear: a symmetric FIR by frequency sampling with a Blackman window
(`reference::designLinearPhaseFir`), 2047 taps at 48 kHz (scaled with the
sample rate), latency (taps - 1) / 2 = 1023 samples (21 ms). An all-pass has
no magnitude to keep: with linear phase it would be a pure delay, so the family
does not offer it. Added on the author's wish: linear-phase EQs are common in
mastering.

## Widths
| Relation | Formula (analog prototype) |
|---|---|
| bandwidth in octaves from Q | BW = (2 / ln 2) asinh(1 / (2 Q)) |
| Q from bandwidth | Q = 1 / (2 sinh((ln 2 / 2) BW)) |
| shelf slope S from Q | 1/Q^2 = (A + 1/A)(1/S - 1) + 2, A = 10^(gain/40) |

The bandwidth is measured between the -3 dB points of a band-pass, between
the half-gain points (in dB) of a peak. `reference::getQFromBandwidth`
also compensates the warping of the digital design.

## Sources
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter
  coefficients" (RBJ designs, Q, bandwidth, shelf slope).
- S. J. Orfanidis, "Digital parametric equalizer design with prescribed
  Nyquist-frequency gain", J. Audio Eng. Soc. 45(6), 1997, pp. 444-455.
- U. Zölzer (ed.), *DAFX*, 2nd ed., Wiley 2011.
- The reference designs of W6 (`pluginlab_reference`).

## Results
From `EqModelTests` and `PluginMeasurementTests` (2026-10-11, 0.44.0):

| Check | Result | Tolerance |
|---|---|---|
| model against the realised processor, 78 bands (all families and types, 44.1/48/96 kHz) | 3.6e-7 dB | 1e-6 dB |
| BW from Q against the half-gain points of the analog peak | 0 octaves | 1e-6 |
| Q from shelf slope against the reference | exact | 1e-12 |
| linear-phase peak 1 kHz +6 dB Q 2: phase | exactly linear | 1e-9 |
| the same: magnitude against RBJ, 50 Hz ... 20 kHz | 0.046 dB | 0.05 dB |
| a model of four bands against the product of its bands | exact | 1e-12 |
| Reference EQ plugin, linear phase, through the host: response | 0.00000 dB | 0.01 dB |
| the same: impulse peak / reported latency | 1023 / 1023 | exact |
| the same: deviation from linear phase | 0.0000 degrees | 0.1 |

"Realised processor": the band's own filter (biquad cascade, state-variable
structure, FIR) run sample by sample in double precision for one second; its
spectrum compared with the model's formula.

## Implementation
```cpp
pluginlab::eqmodel::Band band;          // family, type, frequencyHz, gainDb,
                                        // q, order, enabled
pluginlab::eqmodel::BandDesign design = pluginlab::eqmodel::designBand(band, fs);
std::complex<double> h = pluginlab::eqmodel::getBandResponse(design, band, f, fs);
pluginlab::eqmodel::EqModel model;      // bands, outputGainDb
std::complex<double> total = pluginlab::eqmodel::getModelResponse(model, f, fs);
```
