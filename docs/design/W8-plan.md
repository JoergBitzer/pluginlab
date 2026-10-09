# W8: system classification, EQ band model, parameter mapping and calibration (plan)

Status: **plan v2, not started** (v1 2026-10-09 on the author's request "start the planning document on W8"; v2 the same day after the author's answers
and comments, kept verbatim in the appendix). Planning §8, W8: "EQ band model + parameter mapping, calibration of knobs by measurement. Done when:
mapping data validated for the test EQs." Lessons of the prototype (planning §5 item 4, `docs/prototype/LESSONS_LEARNED.md`): the model of "one peaking
band" was too narrow; every plugin needed an hour of manual exploration for its mapping file; a tool that lists all parameters and proposes a mapping is
needed.

**What changed in v2** (the author: "it is too early for W8 in this form"): before a plugin is described as an EQ (an LTI system made of bands), W8 first
answers **what kind of system it is**: LTI, time-variant, non-linear without memory, Hammerstein, generalized Hammerstein, or other. A typical user does
not know that most tools (a frequency response, a band model, a matcher) assume LTI; the classification says which measurements and models are valid for
a plugin. The band model then applies to the LTI part only. Calibration only **reports** deviations; the matcher of W9 only needs a few examples, not the
full behaviour. Test objects (decided later the same day): only our own reference models and deliberately wrong test plugins in the repository; the author's two test objects stay local.

## 1. What W8 has to answer
For a plugin (an EQ first) and a setting of its knobs:
0. **What kind of system is it?** LTI (up to low-level noise and distortion), time-variant, non-linear (memoryless, Hammerstein, generalized
   Hammerstein, Wiener, other), and which measurements are therefore meaningful.
1. **What does the plugin do?** For the LTI part: bands with type, frequency, gain, Q (or bandwidth, slope, order) and phase behaviour, from a
   **measurement** (W7), not from the knobs.
2. **What do the knobs say?** Which parameter belongs to which band and which band quantity, and the knob law (normalised value -> Hz, dB, Q).
3. **Myth and reality:** where the two disagree (the knob says 1 kHz, the filter sits at 1087 Hz, as in the PeakEQ of W5; the knob says Q 1, the bandwidth
   is that of RBJ Q 0.7; "all knobs at 0" and the plugin distorts, as Pult EQ with its default drive: `docs/measurements/real-plugins-2026-10-09.md`).
4. **The base of the matcher (W9):** a few well-chosen examples (not the full behaviour, the author's answer 4).

## 2. System classification (W8.1)
The author's proposal (appendix), reviewed. All tests use the synchronized sweep of W7.2/W7.5 (Novak et al. 2015): its deconvolution gives the linear
response $H_1(f)$ and the harmonic responses $H_n(f)$ (the n-th harmonic for the input frequency f) in one measurement.

| Class | Signature in the measurements | Test (proposal) |
|---|---|---|
| **LTI** | $H_n$ below a limit (e.g. -80 dB re $H_1$) **and** $H_1$ the same at every level **and** time-invariant | sweeps at three levels (e.g. -40, -20, -6 dBFS); time invariance as below |
| **Time-variant** (LFO, tremolo, chorus, auto-gain, random modulation) | two identical inputs at different times give different outputs; energy outside the harmonic windows of the deconvolution; sidebands around a steady sine | the fingerprint's determinism and time-invariance tests (W5); the same sweep at two start times; the spectrum of a long sine (sidebands at $f \pm f_\text{mod}$) |
| **Memoryless non-linear** | $H_1$ and every $H_n$ flat in magnitude with constant phase (only the common latency); the harmonics change with level as the orders of a polynomial predict | flatness of all $H_n$ over the band; a polynomial (or a table) fitted at one level predicts another level |
| **Hammerstein** (curve, then one filter $H$) | $H_n(f) = c_n\,H(n f)$: every harmonic response is the **same** filter, read at the output frequency $n f$; $H_1 = c_1 H$ | $H_n(f) / H_1(n f)$ constant over f for every n (verified on a Hammerstein test device in W7.5: within 0.001 dB) |
| **Wiener** (one filter $G$, then a curve) | $H_n(f) \propto G(f)^n$ (read at the input frequency) for the leading order of each harmonic | $H_n(f) / H_1(f)^n$ constant over f at a level where the leading order dominates |
| **Generalized Hammerstein** (parallel branches $x^n \to G_n$) | harmonic responses that differ, and a model fitted at one level **predicts the other levels** | the branch filters $G_n$ from the $H_n$ (a triangular linear transform, Novak et al. 2010); the prediction of a second and third level |
| **Other** (Wiener-Hammerstein, Volterra, dynamics such as compressors, hysteresis) | none of the above: e.g. $H_1$ changes with level (a compressor), the generalized Hammerstein fit does not predict other levels | reported as "other", with the evidence (level dependence of $H_1$, residual of the best model) |

**Remarks on the author's decision tree** (the author: "I am not sure, if this is always correct"):
1. *"A linear response ⇒ LTI"* needs more than one level: a compressor below its threshold, an expander above it, or a soft clipper at a low level all
   look LTI at -20 dBFS. Hence sweeps at several levels (and the maximum input level of W7.9 as the upper end).
2. *"Non-linear without a transfer function ⇒ memoryless"*: yes; "no transfer function" means flat $H_1$ and flat $H_n$ (constant phase), allowing a
   common latency (a curve followed by a pure delay is memoryless in this sense).
3. *"The transfer function equal for all overtones ⇒ Hammerstein"*: yes, if "equal" is read at the **output frequency**: harmonic n of the input frequency
   f passes the filter at $n f$. Harmonic n also contains contributions of the orders $n+2, n+4, \dots$; they pass the same filter, so the test holds.
4. *"Different for all overtones ⇒ generalized Hammerstein"*: not necessarily. A Wiener system, a Volterra system or a dynamics processor also has
   different harmonic responses. A generalized Hammerstein model can always be fitted at one level; whether the system **is** one shows only when the
   fitted model predicts another level. The Wiener system is worth its own class: it is common (a pre-emphasis before a saturation, a tone stack
   before a tube model) and has a clear signature.
5. *Time variance*: the fingerprint (W5: determinism, time invariance, real-time pace, a parameter change) answers most of it; two cheap measurements are
   added: the same sweep at two start times (an LFO changes $H_1$ between them) and sidebands around a long sine (modulation, AES17 6.4.4 is the same idea
   for converters). A time-variant system makes the harmonic responses of the sweep meaningless (the sweep assumes time invariance).
6. *Noise and floors*: "LTI" needs limits (harmonics below e.g. -80 dB, $H_1$ equal within 0.05 dB across levels); a float plugin is at -150 dB, a
   dithered one at its noise; the limits go into a settings file like the fingerprint's.

The classification is the first line of every plugin's report: "**LTI** (frequency response, phase and the band model are valid)", "**Hammerstein**
(the frequency response describes the filter; the distortion comes before it)", "**time-variant**: the frequency response is an average", and so on.

## 3. Sources
- A. Novak, L. Simon, F. Kadlec, P. Lotton, "Nonlinear system identification using exponential swept-sine signal", IEEE Transactions on Instrumentation
  and Measurement 59(8), 2010, pp. 2220-2229 (the generalized Hammerstein model from the harmonic responses of the sweep).
- A. Novak, P. Lotton, L. Simon, "Synchronized swept-sine: theory, application, and implementation", J. Audio Eng. Soc. 63(10), 2015, pp. 786-798.
- A. Farina, "Simultaneous measurement of impulse response and distortion with a swept-sine technique", AES 108th Convention, 2000, preprint 5093.
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (the RBJ designs and their Q, bandwidth and shelf-slope definitions).
- S. J. Orfanidis, "Digital parametric equalizer design with prescribed Nyquist-frequency gain", J. Audio Eng. Soc. 45(6), 1997, pp. 444-455.
- T. Schmidt, J. Bitzer, "Digital equalization filter: New solution to the frequency response near Nyquist and evaluation by listening tests", AES 128th
  Convention, London, 2010 (the side study `docs/design/orfanidis-q-correction-study.md`).
- U. Zölzer (ed.), *DAFX: Digital Audio Effects*, 2nd ed., Wiley 2011 (shelving and peak filters).
- V. Välimäki, J. D. Reiss, "All about audio equalization: solutions and frontiers", Applied Sciences 6(5), 2016, 129.
- D. A. Bohn, "Constant-Q graphic equalizers", J. Audio Eng. Soc. 34(9), 1986 (constant against proportional Q).
- The reference designs of W6 (`pluginlab_reference`) and the side studies (Orfanidis Q correction, Nyquist-matched low-pass).
- Fitting: nonlinear least squares (Levenberg-Marquardt), J. Nocedal, S. J. Wright, *Numerical Optimization*, 2nd ed., Springer 2006, chapter 10.

## 4. The work packages
| Step | Content | Done when |
|---|---|---|
| **W8.1** | **System classification** (section 2): sweeps at three levels, the harmonic responses, the tests for each class, the time-variance tests (fingerprint, sweep at two start times, sidebands of a long sine); the class with its evidence and the list of measurements that are valid for it, first line of the report. Test devices with known class: gain and filters (LTI), tremolo (time-variant), polynomial and clippers (memoryless), polynomial then a filter (Hammerstein), a filter then a polynomial (Wiener), parallel branches with different filters (generalized Hammerstein), a compressor-like gain that follows the level (other). | Every test device is classified correctly at 44.1/48/96 kHz; the limits are in a settings file; the twelve EQs of `real-plugins-2026-10-09.md` classified (at their defaults) as a first real check. |
| **W8.2** | **Band model** (`pluginlab_eqmodel`, no hosting): `Band` = type (peak, low/high shelf, low/high pass, band pass, notch, all-pass, tilt), frequency, gain, shape (Q, bandwidth in octaves, shelf slope S, order or dB/octave), **design family** (RBJ, Orfanidis, Zölzer, analogue-matched, Butterworth/LR, **linear phase**) and on/off; `EqModel` = bands + output gain + latency. Conversions between the Q and bandwidth definitions. **The Reference EQ gets a linear-phase band** (the author's answer 3: common in mastering EQs): the magnitude of a chosen design with zero phase, as a symmetric FIR with its latency. | The model reproduces every reference processor (response to 1e-6 dB), including the new linear-phase band; the conversions agree with the cookbook; `docs/eqmodel/band-model.md`. |
| **W8.3** | **Band identification** for an LTI (or the filter of a Hammerstein) system: fit an `EqModel` to the measured response (magnitude in dB; the phase decides between minimum phase and linear phase); initial guesses from the response, Levenberg-Marquardt; bands added while the residual drops clearly; the family chosen by the smallest residual. | Reference processors with 1 ... 4 known bands (including linear phase) are recovered (frequency 0.1 %, gain 0.01 dB, Q 0.5 %); "no good description" for a non-EQ instead of invented bands. |
| **W8.4** | **Parameter survey**: every parameter alone over its range, measured and classified (moves a frequency, changes a gain or a width, switches a type or a band, changes the **system class**, e.g. a drive knob that makes an EQ non-linear, does nothing); the texts the plugin shows. | The classification is right for every parameter of the reference plugins and the test objects; coupled parameters are reported, not guessed. |
| **W8.5** | **Mapping proposal and file**: from the survey, "parameter 3 = band 2 frequency, law: log, 20 Hz ... 20 kHz", the knob law fitted for the displayed text and for the measured value. JSON per plugin (identifier, version, sample rates, date, who confirmed it). **Both places** (the author's answer 2): confirmed mappings of our own plugins (reference and test plugins) in the repository (`mappings/`, test data), all others (including the author's two test objects) in the user's settings folder. | Proposals for the reference and test plugins and for the author's two test objects need at most small edits; the files drive the reference plugins exactly. |
| **W8.6** | **Calibration report, deviations only** (the author's answer 4): for each mapped band parameter, the displayed value against the measured one over the range and at 44.1/48/96 kHz, as a "myth and reality" report. No corrected knob law is handed to the matcher. | The PeakEQ case of W5 reproduced from the calibration alone; the report of each test object lists its deviations. |
| **W8.7** | **Interactions**: bands that do not add in dB (analogue-modelled EQs, proportional Q, an output stage), measured by pairs of bands against the sum of single bands. | Reported for each test object; the model flags when the sum of single bands is not good enough. |
| **W8.8** | **Host and report**: classification, survey, mapping and calibration on the Developer page and as Markdown, like the fingerprint. | The author can classify, survey, confirm and read the calibration of a plugin from the host. |

Order: W8.1 first (it decides what the rest may assume), then W8.2 and W8.3 (no plugin needed; reference processors and the Reference EQ), then W8.4
... W8.8. Each step: design note in this file, code, tests, document, version, commit (as in W7).

## 5. Test objects (decided 2026-10-09)
The author changed the decision of the beginning (planning §6, "Test objects of the project"): the training material and the tests use **only our own
reference models and deliberately wrong test plugins**; the manual suggests websites for open-source or free plugins; the author uses **two test objects
downloaded with his own credentials**, which are **not part of the repository** (and not of CI).
- **In the repository and in CI** (every step is developed and tested with these): the **Reference EQ** with its designs (RBJ, Orfanidis, Zölzer, SVF,
  Butterworth, ...) and, new in W8.2, a linear-phase band; more special designs from the side studies (Orfanidis Q correction, Nyquist-matched low-pass)
  added when needed; the Reference Nonlinear and Utility plugins; the **deliberately wrong test plugins** (W5: EQ designed for 44.1 kHz, EQ that resets in
  prepare, latency liar, ...; W5d.3: editors that mishandle the scale factor), extended by test plugins with a known system class for W8.1 (Hammerstein,
  Wiener, generalized Hammerstein, time-variant) and with known knob errors for W8.6 (a frequency knob off by a factor, a Q knob that is a bandwidth, a
  gain knob that is not in dB).
- **The author's two test objects** (local, not committed): the real-world check of W8.1 ... W8.6; their results are reported in the author's words,
  their mappings are **not** committed (W8.5: "in the repository" now means the mappings of our own plugins only).
- The real EQs measured so far (`docs/measurements/real-plugins-2026-10-09.md`) remain local examples.

## 6. Design questions to settle in W8.2/W8.3 (proposals)
1. **One model, several families**: RBJ, Orfanidis, analogue-matched and linear-phase designs differ for the same numbers; the model keeps the family
   and the fit tries the families; the report says which describes the plugin best.
2. **Q definitions**: one internal Q (RBJ's, on the analogue prototype), converted for display; the mapping records the plugin's definition.
3. **Fitting in dB with weights** by third-octave density; the phase decides minimum against linear phase (W7.4).
4. **Sample rate**: every survey and calibration at 44.1, 48 and 96 kHz.
5. **Delivery**: all renders through the plugin device of W7.11.

## 7. Answers of the author (2026-10-09)
1. Test set: only our own reference models and deliberately wrong test plugins in the repository and the training material; the author's two test
   objects (downloaded with his credentials) stay local; the manual suggests websites for open-source or free plugins (decided later the same day). →
   section 5.
2. Mapping files: both (repository for the test objects, user folder for the rest). → W8.5.
3. Scope: a linear-phase band belongs in (common in mastering EQs; the Reference EQ gets one); dynamic EQ bands and M/S modes later. → W8.2.
4. Calibration: only report the deviations; the matcher needs only very few examples, not the full behaviour. → W8.6.
5. Comments: classify the system first (LTI, time-variant, non-linear with or without memory; Hammerstein and generalized Hammerstein as models; Volterra,
   hysteresis and the like as "other"). → section 2, W8.1.

## Progress
(none yet)

## Appendix: the author's comments (2026-10-09, verbatim)
Answers in the question list:
1. "We have your reference implementation of several EQ types and with our side projects we can even implement more special designs"
2. "both is good"
3. "linear phase is very common for mastering EQs, so it would be good to have at least a linear phase band in the reference EQ. Dynamic EQs and M/S
   modes are more complex and can be left for later."
4. "Only reporting the deviation. The matcher do not need to match the full behaviour, just very feq examples"

Comments:
> I think, it is to early for W8 in this form.
> Of course we can do it, but in my opinion, we should first answer the question, if the system is mainly LTI. Also it would be great to have an idea, if
> the system is just time-variant or non-linear. And if non-linear, if it is memoryless or has memory. This would help to understand the system and to
> find a good model. Hammerstein models and generalized Hammerstein models are a good starting point for non-linear systems. Volterra and other
> non-linearitise like hysteresis are subsumable as "others". Thought process: A typical user is not aware, that many tools need assume LTI.
> I am not sure, if this is always correct, but I think it is: Iw we measure an exponential sweep and we get a linear response the system is LTI (of
> course some low-level noise and some low-level distortion artefacts should be allowed). If we get a non-linear beahviour without any transfer functin
> the system is memoryless non-linear. If we get a nonlinear behaviour and the transfer function is equal for all overtones the system is a simple
> Hammerstein system. If we get a nonlinear behaviour and the transfer function is different for all overtones the system is a generalized Hammerstein
> system. For time-variant system we need other meaningfull measurements. Ideas? Perhaps our fingerprint measurement is enough.

And later: "I still have not decided for the final test objects". Then the decision: "Test objects for training session e.g. in the manual are only our
own reference models and deliberately wrong test plugins. We will suggest some web-sites to download other test-objects that are open-source or free.
With this constraint, I can download 2 test objects that are downloaded with my credentials. These are not part of the repository."
