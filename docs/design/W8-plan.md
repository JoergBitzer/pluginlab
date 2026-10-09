# W8: EQ band model, parameter mapping and calibration (plan)

Status: **plan, not started** (author's request 2026-10-09: "start the planning document on W8"). Planning §8, W8: "EQ band model + parameter mapping,
calibration of knobs by measurement. Done when: mapping data validated for the test EQs." Lessons of the prototype (planning §5 item 4,
`docs/prototype/LESSONS_LEARNED.md`): the model of "one peaking band" was too narrow (shelves, high/low-pass, several bands, bands without Q); every
plugin needed an hour of manual exploration for its mapping file; a tool that lists all parameters and proposes a mapping is needed.

## 1. What W8 has to answer
For an EQ plugin and a setting of its knobs:
1. **What does the plugin do?** A description in the terms of an EQ: bands with type, frequency, gain, Q (or bandwidth, slope, order) and phase behaviour.
   This description comes from a **measurement** (W7: frequency response, phase), not from the knobs.
2. **What do the knobs say?** Which parameter belongs to which band and which band quantity, and the knob law (normalised value -> Hz, dB, Q).
3. **Myth and reality:** where the two disagree (the knob says 1 kHz, the filter sits at 1087 Hz, as in the PeakEQ of W5; the knob says Q 1, the bandwidth
   is that of RBJ Q 0.7; the gain says +6 dB and the peak is +5.6 dB; the frequency moves with the sample rate).
4. **The base of the matcher (W9):** to set plugin B so that it does what plugin A does, the matcher needs B's knobs in band terms and a model to optimise.

## 2. Sources
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (the RBJ designs and their Q, bandwidth and shelf-slope definitions).
- S. J. Orfanidis, "Digital parametric equalizer design with prescribed Nyquist-frequency gain", J. Audio Eng. Soc. 45(6), 1997, pp. 444-455.
- T. Schmidt, J. Bitzer, "Digital equalization filter: New solution to the frequency response near Nyquist and evaluation by listening tests", AES 128th
  Convention, London, 2010 (the side study `docs/design/orfanidis-q-correction-study.md`).
- U. Zölzer (ed.), *DAFX: Digital Audio Effects*, 2nd ed., Wiley 2011 (shelving and peak filters).
- V. Välimäki, J. D. Reiss, "All about audio equalization: solutions and frontiers", Applied Sciences 6(5), 2016, 129 (a review of parametric and
  graphic EQ designs, Q and bandwidth definitions, proportional Q).
- D. A. Bohn, "Constant-Q graphic equalizers", J. Audio Eng. Soc. 34(9), 1986 (constant against proportional Q: the bandwidth of many analogue EQs
  depends on the gain).
- The reference designs of W6 (`pluginlab_reference`): RBJ, Orfanidis, Zölzer, SVF (TPT), analogue prototypes, Butterworth, Linkwitz-Riley,
  linear-phase FIR, and the Nyquist-matched low-pass of the side study (`docs/design/nyquist-matched-lowpass-study.md`).
- Fitting: nonlinear least squares (Levenberg-Marquardt), J. Nocedal, S. J. Wright, *Numerical Optimization*, 2nd ed., Springer 2006, chapter 10.

## 3. The work packages
| Step | Content | Done when |
|---|---|---|
| **W8.1** | **Band model** (`pluginlab_eqmodel`, no hosting): `Band` = type (peak, low/high shelf, low/high pass, band pass, notch, all-pass, tilt), frequency, gain, shape (Q, bandwidth in octaves, shelf slope S, order or dB/octave), **design family** (RBJ, Orfanidis, Zölzer, analogue-matched, Butterworth/LR, linear phase) and on/off; `EqModel` = bands + output gain + latency. The response of the model (magnitude and phase) from the reference designs. Conversions between the Q and bandwidth definitions (RBJ Q, bandwidth at -3 dB of the gain, "half-gain" bandwidth, proportional Q). | The model reproduces every reference processor of W6 exactly (its response equals the processor's to 1e-6 dB); the conversions agree with the cookbook formulas; a document `docs/eqmodel/band-model.md` with the definitions side by side. |
| **W8.2** | **Band identification by measurement**: fit an `EqModel` to a measured response (W7.2 sweep, magnitude in dB and, for minimum-phase EQs, the phase of W7.4): initial guesses from the response (peaks, slopes, plateaus), then Levenberg-Marquardt on the band parameters; the number of bands chosen by the residual (add a band while the residual drops clearly); the family chosen by the smallest residual. The residual over 20 Hz ... 20 kHz says how well the description fits. | Reference processors with 1 ... 4 known bands are recovered (frequency within 0.1 %, gain within 0.01 dB, Q within 0.5 %) from the measured response at 44.1/48/96 kHz; the fit says "no good description" for a non-EQ (e.g. a comb filter) instead of inventing bands. |
| **W8.3** | **Parameter survey** of a plugin (from the fingerprint's scan, W5): every parameter alone over its range (e.g. 9 positions), the response measured each time, the change classified: moves a frequency, changes a gain, changes a width, switches a type or a band on/off, does nothing; the texts the plugin shows at each position (`getText`) recorded. Choice parameters with all their entries. | For the three reference plugins and the test EQs the classification is right for every parameter (checked by hand once and kept as expected results); unknown or coupled parameters are reported, not guessed. |
| **W8.4** | **Mapping proposal and file**: from the survey, a proposal "parameter 3 = band 2 frequency, law: log, 20 Hz ... 20 kHz" for each band parameter, with the knob law fitted (linear, logarithmic, table of points) both for the displayed text and for the measured band value. Stored as JSON per plugin (identifier, version, sample rates, date, who confirmed it); the author or user confirms or edits the proposal. | Proposals for the test EQs need at most small edits; the files load and drive the reference plugins exactly. |
| **W8.5** | **Calibration: knob against measurement** ("myth and reality"): for each mapped band parameter, the displayed value against the measured one over the range and at 44.1/48/96 kHz; the deviations in a report (e.g. "frequency: measured = 1.087 x knob at 48 kHz, 1.000 x at 44.1 kHz: the design assumes 44.1 kHz"; "Q: the knob is the bandwidth in octaves, not RBJ Q"; "gain: proportional Q, the bandwidth narrows with gain"). | The PeakEQ case of W5 is reproduced from the calibration alone; the report of each test EQ lists its laws and deviations. |
| **W8.6** | **Interactions**: bands that do not add in dB (an analogue-modelled EQ whose bands interact, a band whose Q depends on its gain, an output stage), checked by measuring pairs of bands against the sum of the single bands. | Interaction measured and reported for each test EQ; the model flags when the sum of single bands is not good enough for the matcher. |
| **W8.7** | **Host and report**: the survey, the proposal, the confirmed mapping and the calibration report on the Developer page (a table; editing of the mapping) and as Markdown, like the fingerprint. | The author can survey, confirm and read the calibration of a test EQ from the host without the command line. |

Order: W8.1 and W8.2 first (they need no plugin; tested with the reference processors and the Reference EQ plugin), then W8.3 ... W8.5 on the
reference plugins and the test EQs, W8.6, W8.7. Each step: design note in this file, code, tests, document, version, commit (as in W7).

## 4. Test objects
- The **Reference EQ plugin** (W6.4: RBJ, Orfanidis, Zölzer, SVF, Butterworth, ...): every answer known; the first and decisive test of W8.1 ... W8.5.
- The **test EQs** of planning §6 (at most 5: own, 2 open source, 2 free closed source; decided so far: Dusk Audio 4K EQ 2 and Soundly Shape it) and
  the candidates of open question 1 (Venn Free EQ). The own **PeakEQ** with its known 44.1 kHz design for W8.5.
- Shape it: 1001 log-spaced frequency texts and text lists for the type: the survey must handle text-list parameters.
- 4K EQ 2: an analogue-modelled EQ (proportional Q likely): W8.6.

## 5. Design questions to settle in W8.1/W8.2 (proposals)
1. **One model, several families**: a band is not just "a peak of Q 2"; RBJ, Orfanidis and the analogue-matched designs give different responses near
   Nyquist for the same numbers. The model keeps the family as a property and the fit tries the families; the report says which family describes the
   plugin best (itself a finding: "this EQ cramps near Nyquist like RBJ").
2. **Q definitions**: the model stores one internal Q (RBJ's, defined on the analogue prototype) and converts for display; the mapping records which
   definition the plugin's knob uses.
3. **Fitting in dB with weights**: the magnitude in dB, weighted by third-octave density (not by bin count, which would favour the high frequencies);
   phase only as a check (an EQ that is not minimum phase: linear-phase or mixed-phase modes, W7.4 shows it).
4. **Sample rate**: every survey and calibration at 44.1, 48 and 96 kHz (the PeakEQ lesson); mapping laws may depend on the rate.
5. **Delivery**: all renders through the plugin device of W7.11 (fresh instance, careful delivery); the parameter smoothing and delivery quirks of W5 are
   therefore already handled.

## 6. Open questions for the author
1. **Which EQs form the test set** (planning §9, question 1 is still open): W8 needs at least the Reference EQ plus two real EQs to be meaningful.
2. **Are mapping files part of the repository** (e.g. `mappings/<plugin>.json`, committed after confirmation) or user data only? Proposal: both:
   confirmed mappings of the test EQs in the repository (they are test data), others in the user's settings folder.
3. **Scope of band types**: dynamic EQ bands, M/S modes and linear-phase modes are out of W8 (they need W11 or the stereo units); is that right?
4. **How far "calibration" goes**: only report the deviations (myth and reality), or also offer a corrected knob law to the matcher (W9)? Proposal: both;
   the matcher uses the measured law, the report shows the difference.

## Progress
(none yet)
