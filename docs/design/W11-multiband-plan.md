# W11.5: multiband dynamics (plan)

Status: **plan v1** (2026-10-19, for review; questions for the author in section 8). The author: "Multi-band is one class and dynamic EQ
another one" (answer to `W11-dynamics-plan.md`, question 5). Builds on W11.3 (single-band dynamics) and on the EQ work (W8, W9).

## 1. What W11.5 has to answer
- **Musician / engineer**: how many bands, where are the crossovers, how does each band compress (threshold, ratio, attack, release), do the
  bands add up to a flat response when nothing is compressed, what does it do to a mix?
- **Developer**: is the crossover sum flat (magnitude and phase: Linkwitz-Riley, linear-phase FIR), do the bands stay aligned in time, does the
  reported latency include a linear-phase crossover, are the bands independent of the block size?

## 2. What W11.3 already gives
The classification (W11.3 4.4: the two-tone test tells single band / multiband / dynamic EQ), the static curve and the gain curves of the three
phases per test tone, the model fit (Simmer, Schmidt, Bitzer 2006), the gain curves on music, the detector tests, the reports.

## 3. Measurements
### 3.1 The crossover at rest
- Below every threshold the plugin is an LTI filter bank whose sum should be flat: the frequency response and the phase (W7.2, W7.4) of the
  whole, the group delay (linear-phase crossovers: a pure delay; minimum-phase Linkwitz-Riley: an all-pass in the sum).
- **The bands one by one**: with a band soloed or muted (if the plugin offers it, from the mapping), or by the two-tone test: the probe's gain
  over frequency while a loud tone drives one band (the "dynamic transfer function" of W11.3) gives the band edges and the crossover slopes
  (12, 24, 48 dB/octave).
- Crossover frequencies against the knobs (the calibration idea of W8.6).

### 3.2 Each band's dynamics
The W11.3 measurements with test tones inside one band at a time (centred between the crossovers): static curve, threshold, ratio, knee,
attack, release, hold, the model variant; the band's detector (does a tone in one band reduce another band: band linking).

### 3.3 Interactions
- A tone at a crossover frequency: two bands share it; the gain it gets when both detect it (the sum of two partly compressed bands; dips or
  bumps at the crossover under compression).
- Two loud tones in two bands: independence of the bands.
- Phase and time alignment under compression (a band's gain change must not move it in time).

### 3.4 With music
The gain curves per band on a music sample (one row per band, the band's input level under it): what each band does over time; the spectrum
before and after; loudness (BS.1770-5) for the level match.

## 4. Comparing two multiband processors
The band edges and slopes, each band's static curve and gain curves overlaid, the sum response at rest, the gain curves per band on music.
No matching at first.

## 5. Visualising
- **Crossover plot**: the bands' responses and their sum over frequency (dB), the crossover frequencies marked.
- **Static curves per band** (one colour per band), and the threshold over frequency.
- **Gain curves per band on music**: a stack of rows, one per band, on one time axis.
- **Gain reduction over frequency and time** (a heat map: frequency bands over time, the reduction as colour) for music.

## 6. Reference plugin
**Reference Multiband**: 2 ... 4 bands, Linkwitz-Riley crossovers (24 dB/octave) or linear-phase FIR (switchable), the Reference Compressor of
W11.3 per band, solo and mute per band, band linking. Every value of 3 has its expected value.

## 7. Work packages
| Step | Content | Done when |
|---|---|---|
| W11.5a | crossover at rest: sum response, band edges, slopes, latency | the reference's crossovers within 1 % in frequency, the sum flat within 0.05 dB |
| W11.5b | the dynamics per band (W11.3 per band), band linking | the reference's per-band thresholds and time constants recovered |
| W11.5c | interactions at the crossovers, two-band independence | the reference's crossover behaviour under compression described |
| W11.5d | gain curves per band on music, report section, views of 5 | readable reports for the reference and two real multiband compressors |

## 8. Questions for the author
1. Test set: which free multiband compressors (and an own implementation to start the reference from)?
2. Linear-phase crossovers: important enough for the first version (they add a large latency)?
3. Band solo / mute: may the measurement use them when the mapping knows them, or only the two-tone test (works for every plugin)?

## 9. References
- U. Simmer, D. Schmidt, J. Bitzer, "Parameter estimation of dynamic range compressors: models, procedures and test signals," AES 120th
  Convention, Paris, 2006 (single band; the procedure applied per band here).
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital dynamic range compressor design - a tutorial and analysis," JAES 60(6), 2012.
- S. H. Linkwitz, "Active crossover networks for noncoincident drivers," JAES 24(1), 1976 (the Linkwitz-Riley crossover).
- ITU-R BS.1770-5 (2023) for the loudness.
