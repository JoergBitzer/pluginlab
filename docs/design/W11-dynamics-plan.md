# W11.3: dynamics (plan)

Status: **plan v1** (2026-10-18, for review; questions for the author in section 9). Part of W11, after the delay (`W11-delay-plan.md`) and the
reverb (`W11-reverb-plan.md`). Dynamics processors are non-linear and remember the past: no transfer function describes them, so the
measurements are a static curve plus responses to level changes.

## 1. What W11.3 has to answer
- **Musician**: does this compressor squash like mine (how much, how fast, how it lets go); how do I set it the same?
- **Engineer / teacher**: threshold, ratio, knee, attack, release, the detector (peak or RMS), the side-chain filter, the stereo link, the
  lookahead, the distortion that fast settings cause; are the knobs labelled honestly?
- **Developer**: does the static curve match the design at every sample rate; are the time constants independent of the block size; does the
  limiter hold its ceiling with inter-sample peaks; does the reported latency include the lookahead?

## 2. The class
| Kind | Typical knobs |
|---|---|
| compressor (feed-forward / feedback, peak / RMS, hard / soft knee) | threshold, ratio, knee, attack, release, makeup, mix, detector, stereo link, side-chain HPF |
| limiter | ceiling, release, lookahead, true peak |
| expander, gate | threshold, ratio or range, attack, hold, release, hysteresis |
| upward compressor, transient shaper | amount, attack, sustain |
| de-esser, multiband, dynamic EQ | band, threshold per band |
| ducker | external side chain |

System class (W8.1): non-linear, usually "other" (the harmonic models do not fit dynamics).

## 3. What exists to build on
The stepped and steady tones of the AES17 units (W7), THD and THD+N (W7.5), maximum level and linearity (W7.9), the system class (W8.1), the
fingerprint's side-chain detection, the real-time tests (W5c), the matcher's general layer (W9.1, with a deterministic check instead of the LTI
check), the level trims of the Easy page (W10.4), the reports.

## 4. Measurements
### 4.1 Static curve
- A 1 kHz tone (and 100 Hz, 10 kHz: see 4.4) stepped from -80 to 0 dBFS in 1 dB steps, each step held until the output settles (at least five
  times the longest release, detected from the output envelope); the steady output level per input level: the **input/output curve** and the
  **gain reduction curve**.
- Rising and falling: a difference is **hysteresis** (gates, some compressors).
- **Fit** of the gain computer of Giannoulis, Massberg, Reiss (2012): threshold, ratio, knee width (hard knee as width 0), makeup; by the
  bounded Levenberg-Marquardt of W9.1; the residual tells whether the plugin follows that model at all.

### 4.2 Detector
- The same RMS level with different crest factors (sine, square, a two-tone, noise): a peak detector reacts to the peaks, an RMS detector to the
  power; the threshold shift between the signals tells which (and the RMS window from the ripple on a low tone).
- Feed-forward against feedback: the slope above the threshold at high ratios (a feedback design cannot reach infinity : 1), the shape of the
  attack transient.

### 4.3 Time behaviour
- **Level step**: a tone switched between two levels below and above the threshold (e.g. 20 dB apart), up and down. The **gain envelope**
  g(t) = |analytic(output)| / |analytic(input)| at sample resolution (the Hilbert envelope of a steady carrier; no meter ballistics in the way).
- **Attack and release** in two conventions, both reported: the time constant of an exponential fitted in the dB domain (the 63 % definition,
  as the smoothing filters of the tutorial are defined) and the time until the output is within a tolerance of its final value (the convention
  of the AGC standards IEC 60268-8 and, for hearing aids, IEC 60118-2 and ANSI S3.22; their step levels and tolerances to be taken from the
  standards' text).
- Dependence on the step size (some designs attack faster on larger steps), on the burst length before the release (**program-dependent
  release**, auto release: a short burst against a long one), and on the block size and sample rate (the fingerprint's pattern).
- **Lookahead**: the gain falls before the step arrives (against the dry path at the reported latency): its duration, and whether the reported
  latency includes it.
- **Hold** (gates): the time between the end of the burst and the start of the release.

### 4.4 Frequency dependence of the side chain
The static curve at 100 Hz, 1 kHz and 10 kHz (side-chain HPF, de-esser, tilted detectors), or the threshold over frequency directly: the input
level at which the gain reduction reaches 1 dB, per third octave.

### 4.5 Distortion caused by the dynamics
A tone above the threshold at 50, 100, 1000 Hz: fast attack and release follow the waveform and modulate it, the THD rises at low frequencies
(W7.5 units, measured in the steady state above the threshold); THD against the attack/release settings is a known trade-off.

### 4.6 Stereo and side chain
- **Stereo link**: one channel above the threshold, the other below: the gain reduction of the quiet one (0 % to 100 % link).
- **External side chain** (ducker): the side-chain bus driven, the main input steady: the gain reduction from the key (the fingerprint already
  finds side-chain buses).

### 4.7 Limiters
The ceiling with sample peaks and with **inter-sample peaks** (the true peak of ITU-R BS.1770, 4x oversampling); the overshoot on a fast step;
the release.

### 4.8 With music
For listening and the Easy page: the gain reduction trace and the loudness over time on a music file; integrated loudness (ITU-R BS.1770-5,
K-weighting, gated) and the loudness range before and after; crest factor. The level match for listening uses the integrated loudness (the
trims of W10.4).

## 5. Comparing two dynamics processors (and matching)
- The static curves overlaid (the difference in dB at each input level), threshold/ratio/knee from the fits, attack and release times (ms and
  ratio), the gain envelopes of the same step overlaid, the detector type, the side-chain frequency response, the distortion.
- **Matching**: a "dynamics domain" for the general matcher: the features are points of the static curve and samples of the gain envelope for
  an up and a down step at log-spaced times; released parameters threshold, ratio, knee, attack, release, makeup (from a mapping). The fast path
  needs a determinism check instead of the LTI check (two renders of the same setting equal). Grey-box models with the structure of a digital
  compressor fit analog units well (Wright and Välimäki 2022); differentiable compressors are used for style transfer (Steinmetz, Bryan, Reiss
  2022): both support matching by the parameters of a known structure.
- With music: the difference of the gain reduction traces and of the short-term loudness, after a loudness match.

## 6. Visualising
- **Static curve** (input against output level, 1 : 1 line, the fitted model, threshold and knee marked) and the **gain reduction curve**.
- **Gain envelope** over time for the step up and down (linear or log time axis), attack and release marked, two plugins overlaid.
- **Family of curves**: the static curve or the gain envelope for several settings (ratio 2, 4, 8; attack 1, 10, 100 ms): what a knob does.
- **Threshold over frequency** (side-chain response).
- **Hysteresis loop** for gates (rising and falling curves).
- **Gain reduction heat map**: level over time with the gain reduction as colour, for a level staircase or for music.
- **Level histograms** before and after on music (the dynamic range distribution).

## 7. Reference plugins (ground truth)
**Reference Compressor**: the feed-forward design recommended by Giannoulis et al. (2012): gain computer with hard or soft knee, smoothing in the
log domain after the gain computer with attack and release time constants, peak or RMS detector, lookahead, side-chain HPF, stereo link, makeup.
**Reference Gate**: threshold, range, attack, hold, release, hysteresis. Every value of 4 has an analytic expected value.

## 8. Work packages
| Step | Content | Done when |
|---|---|---|
| W11.3a | gain envelope extraction, level staircase with settling detection, side-chain driving in the rig | the reference's static curve within 0.1 dB, its envelope sample-exact |
| W11.3b | static curve, fit (threshold, ratio, knee), hysteresis | the reference's parameters recovered within 0.2 dB / 1 % |
| W11.3c | attack and release (both conventions), step size, program dependence, lookahead, hold | the reference's time constants within 2 % |
| W11.3d | detector type, side-chain frequency response, stereo link, external side chain | peak and RMS of the reference told apart; link 0 / 50 / 100 % measured |
| W11.3e | distortion of the dynamics, limiter true peak | the reference's LF distortion grows with faster release |
| W11.3f | report section "Dynamics", Developer page, views of 6, loudness with music | readable reports for the references and two real compressors |
| W11.3g | comparison and the dynamics domain of the matcher | the reference matched to itself with another start; two real compressors compared |

## 9. Questions for the author
1. **Test set**: which free compressors, limiters and gates (a clean digital compressor, an 1176- or LA-2A-style model, a limiter, a gate)?
2. **Attack/release convention**: report both (63 % time constant and the AGC-standard tolerance time), or one as the main number?
3. **The tone frequency** for the static curve: 1 kHz (with 100 Hz and 10 kHz for the side chain) right, or 997 Hz as in AES17?
4. **Music-based comparison**: which material (the MusicRadar samples of `planning.md`)? Loudness after BS.1770 for the level match?
5. **Multiband and dynamic EQ**: in W11.3 or later (proposal: later; they combine the EQ work and this package)?

## 10. References
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital dynamic range compressor design - a tutorial and analysis," JAES 60(6), 2012, pp. 399-408
  (designs, detectors, the recommendation of feed-forward with log-domain smoothing, quality metrics).
- A. Wright, V. Välimäki, "Grey-box modelling of dynamic range compression," DAFx-20in22, Vienna, 2022, pp. 304-311.
- C. J. Steinmetz, N. J. Bryan, J. D. Reiss, "Style transfer of audio effects with differentiable signal processing," JAES 70, 2022
  (arXiv 2207.08759).
- M. A. Martínez Ramírez, E. Benetos, J. D. Reiss, "Deep learning for black-box modeling of audio effects," Applied Sciences 10(2), 638, 2020.
- IEC 60268-8, "Sound system equipment - Part 8: Automatic gain control devices" (attack and recovery times of AGC devices).
- IEC 60118-2 (hearing aids with automatic gain control) and ANSI S3.22 (hearing aid characteristics): attack and release by level steps.
- ITU-R BS.1770-5 (2023), "Algorithms to measure audio programme loudness and true-peak audio level"; EBU Tech 3342 (loudness range).
- AES17-2015 for the tones, the levels and the THD+N units already in use (W7).

Sources found 2026-10-18:
[AES E-Library: Giannoulis et al. 2012](https://aes2.org/publications/elibrary-page/?id=16354),
[Aalto: grey-box compression (CC BY)](https://acris.aalto.fi/ws/portalfiles/portal/89253381/Wright_et_alii_GREY_BOX_MODELLING_OF_DYNAMIC_RANGE_COMPRESSION.pdf),
[Style transfer of audio effects](https://export.arxiv.org/abs/2207.08759),
[Applied Sciences: black-box modelling](https://www.mdpi.com/2076-3417/10/2/638),
[Audio Precision: AGC attack and release (IEC 60268-8, IEC 60118-2, ANSI S3.22)](https://www.ap.com/download/apx-attack-release-plugin/),
[IEC 60268-8 preview](https://www.technickenormy.cz/publicdoc/iec_previews/62327.pdf).
