# W11.3: dynamics (plan)

Status: **plan v2** (v1 2026-10-18; v2 2026-10-19 with the author's answers and comments and the author's paper Simmer, Schmidt, Bitzer
2006, section 9). Part of W11, after the delay (`W11-delay-plan.md`) and the
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
### 4.0 The model of the measurement (Simmer, Schmidt, Bitzer 2006)
The author's paper "Parameter estimation of dynamic range compressors: models, procedures and test signals" (AES 120th Convention, Paris,
2006) gives the procedure this package follows: a single-band feed-forward model with **two smoothing stages**, each with its own attack and
release: the smoothing of the detector (the input power, RMS) and the smoothing of the gain after the static curve; a look-ahead delay; the
static curve G = (X - T)/R - (X - T) + M above the threshold. Artificial test signals measure the static and the dynamic behaviour; the model's
parameters are fitted to the output of the device (a simplex there; the bounded Levenberg-Marquardt of W9.1 here), with three model variants
(detector smoothing only, gain smoothing only, both) and the one with the smallest error chosen; the release phase first (longer, more
precise), then the attack. What the paper found in five real compressors is what the measurements must be able to show: a static curve that is
linear in amplitude instead of in dB (log and antilog omitted), level-dependent release, switching between smoothing rules (a kink in the gain
curve), oversampling before the RMS detector (a ripple at the signal frequency), a fixed look-ahead. All were fitted within 1 dB on music.

### 4.1 Static curve
- The paper's signal: a **500 Hz rectangular (square) wave** with rising levels in 1 dB steps, each step 2 s long (a square wave has the same
  RMS and peak value, so the curve does not depend on the detector type; rising levels settle with the attack time, which is shorter than the
  release). In addition sines at 100 Hz, 1 kHz and 10 kHz (the author: start with this) for the frequency dependence of 4.4; the steady output
  level per input level: the **input/output curve** and the **gain reduction curve**, in dB and in amplitude (a linear-amplitude curve shows
  in the amplitude plot as a straight line, as in the paper's compressors B to D).
- Rising and falling: a difference is **hysteresis** (gates, some compressors).
- **Fit** of the gain computer of Giannoulis, Massberg, Reiss (2012): threshold, ratio, knee width (hard knee as width 0), makeup; by the
  bounded Levenberg-Marquardt of W9.1; the residual tells whether the plugin follows that model at all.

### 4.2 Detector
- The paper's signal: a full-scale square wave with **inserted zeros**: the peak stays, the RMS falls: a peak detector keeps the gain, an RMS
  detector lets it rise; and the converse (constant RMS, varying peak) as the cross-check the paper proposes. Also sine, two-tone and noise at
  the same RMS (the threshold shift between them), the RMS window from the ripple on a low tone, and the ripple at the signal frequency that
  oversampling before the detector leaves.
- Feed-forward against feedback: the slope above the threshold at high ratios (a feedback design cannot reach infinity : 1), the shape of the
  attack transient.

### 4.3 Time behaviour
- **Level step**: the paper's three phases: below the threshold (static), full scale (attack), below the threshold again (release), each 0.5 s
  and longer until the gain has settled. The **gain curve** g(t) = output / delayed input (the look-ahead from 4.3b removed): for the square wave
  sample by sample (no zero crossings), for sines by the analytic envelopes (|analytic(output)| / |analytic(input)|; the paper's glitches at
  zero crossings avoided).
- **The model fit** (4.0): attack and release of the detector and of the gain, for the three variants; the variant with the smallest error, the
  residual as "how well a classic compressor model describes it". The standards' numbers alone are not enough (the author: double log
  smoothing, linear attack and hold are not covered by them): the fit adds the model variants below, and the gain curves are shown, not only
  summarised.
- **Model variants beyond the paper** (the author's comments): smoothing in the log domain (of the level, of the gain, or both: "double log
  smoothing"); **linear attack** (a ramp of the gain in dB or in amplitude over a fixed time, used in limiters); **hold** (the gain is frozen for
  a hold time after the level falls, before the release starts: "to reduce the movement of the gain"; hold is an internal parameter of many
  compressors and limiters, not only of gates); **level-dependent release** (the paper's compressor B); program-dependent release (the LA-2A's
  optical cell).
- **Attack and release** in two conventions, both reported: the time constant of an exponential fitted in the dB domain (the 63 % definition,
  as the smoothing filters of the tutorial are defined) and the time until the output is within a tolerance of its final value (the convention
  of the AGC standards IEC 60268-8 and, for hearing aids, IEC 60118-2 and ANSI S3.22; their step levels and tolerances to be taken from the
  standards' text).
- Dependence on the step size (some designs attack faster on larger steps), on the burst length before the release (**program-dependent
  release**, auto release: a short burst against a long one), and on the block size and sample rate (the fingerprint's pattern).
- **Lookahead** (the paper's method): the lag of the first maximum of the cross-correlation between input and output for white noise; the gain
  then falls before the step arrives (against the delayed input): its duration, and whether the reported latency includes it.
- **Hold** (gates): the time between the end of the burst and the start of the release.

### 4.4 Frequency dependence: side-chain filter, multiband, dynamic EQ
The static curve at 100 Hz, 1 kHz and 10 kHz (side-chain HPF, de-esser, tilted detectors), or the threshold over frequency directly: the input
level at which the gain reduction reaches 1 dB, per third octave.

**Is it multiband or a dynamic EQ?** (the author: "it is important that we know"). The **two-tone test**: a loud tone at f1 (above the threshold)
and a quiet probe tone at f2 (well below it), f2 swept over the band while f1 stays. The gain applied to the probe, over f2, is a "dynamic
transfer function":
- **single band** (with or without a side-chain filter): the probe gets the same gain reduction at every f2 (one gain for the whole signal);
- **multiband**: only the probes in f1's band are reduced; the reduction ends at the crossover frequencies (their slopes: the crossover filters);
- **dynamic EQ**: the probe is reduced in a bell or shelf around the band's frequency, with the band's shape; it grows with f1's level.
The same test with f1 swept tells the bands' detection ranges. Classification first; multiband and dynamic EQ as classes are later packages
(the author), this test only says which class a plugin belongs to.

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

### 4.8 With music: gain curves of several compressors
The author: "I would like to see and compare gain curves of different compressors. This is a very important feature for musicians and
engineers. This should be possible for musical input." For each compressor the **gain curve on a music file** (the MusicRadar samples of
`planning.md`): g(t) = output / delayed input, estimated robustly (least squares over short windows, or the ratio of smoothed envelopes; the
paper shows the glitches of a sample-by-sample ratio at zero crossings), all compressors on one time axis with the input's level under them.
Integrated loudness and loudness range before and after (ITU-R BS.1770-5, the current standard: decided by the author), crest factor. The level
match for listening uses BS.1770-5 (the trims of W10.4).

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
- **Gain curves of the attack and release phases** (the author: attack and release must be visualised): g(t) for the step up and down (linear
or log time axis), the fitted model dashed over the measurement (as in the paper's figures 6, 7, 12), attack, hold and release marked, several
plugins overlaid.
- **Gain curves on music**: several compressors' g(t) on one time axis over the input's waveform or level (4.8).
- **Dynamic transfer function** of the two-tone test (4.4): the probe's gain over frequency: flat (single band), steps (multiband), a bell
  (dynamic EQ).
- **Family of curves**: the static curve or the gain envelope for several settings (ratio 2, 4, 8; attack 1, 10, 100 ms): what a knob does.
- **Threshold over frequency** (side-chain response).
- **Hysteresis loop** for gates (rising and falling curves).
- **Gain reduction heat map**: level over time with the gain reduction as colour, for a level staircase or for music.
- **Level histograms** before and after on music (the dynamic range distribution).

## 7. Reference plugins (ground truth)
The author's set: a **feed-forward compressor** (the reference below), a **feedback compressor** (1176 style), a **tube / optical compressor**
(LA-2A style: program-dependent release), a **limiter** (linear attack, true peak, hold) and a **gate**, in our own reference plugins; other
designs if source code is found. The author's AAT_Compressor in ~/AudioDev is a starting point.
**Reference Compressor**: the feed-forward design recommended by Giannoulis et al. (2012): gain computer with hard or soft knee, smoothing in the
log domain after the gain computer with attack and release time constants, peak or RMS detector, lookahead, side-chain HPF, stereo link, makeup;
switchable to the paper's two-stage model (detector and gain smoothing in the linear or log domain), a linear attack and a hold time.
**Reference Gate**: threshold, range, attack, hold, release, hysteresis. Every value of 4 has an analytic expected value.

## 8. Work packages
| Step | Content | Done when |
|---|---|---|
| W11.3a | gain envelope extraction, level staircase with settling detection, side-chain driving in the rig | the reference's static curve within 0.1 dB, its envelope sample-exact |
| W11.3b | static curve, fit (threshold, ratio, knee), hysteresis | the reference's parameters recovered within 0.2 dB / 1 % |
| W11.3c | the gain curves of the three phases, the model fit with its variants (4.0, 4.3), both standard conventions, step size, program dependence, lookahead, hold, linear attack | the reference's time constants within 2 % in each model variant; the variant recognised |
| W11.3d | detector type, side-chain frequency response, the two-tone test (single band / multiband / dynamic EQ), stereo link, external side chain | peak and RMS of the reference told apart; a multiband and a dynamic-EQ test plugin classified; link 0 / 50 / 100 % measured |
| W11.3e | distortion of the dynamics, limiter true peak | the reference's LF distortion grows with faster release |
| W11.3f | report section "Dynamics", Developer page, views of 6, gain curves of several compressors on music, loudness (BS.1770-5) | readable reports for the references and two real compressors; three compressors' gain curves on a MusicRadar sample in one plot |
| W11.3g | comparison and the dynamics domain of the matcher | the reference matched to itself with another start; two real compressors compared |

## Progress
### W11.3a, W11.3b gain envelope, static curve, fit (0.89.0, done)
- `reference::Compressor`: the feed-forward design of Giannoulis et al. (2012): detector (peak, or RMS as a moving mean square), stereo link,
  gain computer with hard or soft knee, the gain reduction smoothed in the log domain with smooth branching (attack, release), look-ahead,
  makeup; its exact static curve; the gain per sample can be recorded.
- `measure::makeStaircase` (the square wave of Simmer, Schmidt, Bitzer 2006 at 500 Hz, an even half period: no sample is zero; or a sine),
  `getGainEnvelopeDb` (output / delayed input, sample by sample), `measureStaticCurve` (the rising and the falling staircase, the mean gain of the
  last quarter of every step, whether it settled, the hysteresis), `fitStaticCurve` (with threshold and knee fixed the gain computer is linear
  in 1/R - 1 and the makeup: least squares; threshold and knee on a grid of 0.25 / 0.5 dB, then refined).
- Tests: the envelope equals the reference's gain sample by sample (1e-4 dB, float output); the static curve of -60 ... 0 dBFS within
  0.0000 dB, no hysteresis, every step settled; the fit for knees of 0, 6, 12 dB: threshold -20.000 dB, ratio 4.0000, knee and makeup exact.
- Not yet: driving a side chain in the rig (W11.3d), the gate and its hysteresis (with the Reference Gate).

### W11.3c the time behaviour (0.90.0, done)
- `reference::Compressor` with the model variants: GainLog (the gain reduction smoothed in dB), LevelLog (the detector level smoothed in dB,
  the gain computer after it), TwoStage (the detector power smoothed with its own attack and release, then the gain in dB), and a hold.
- `measure::measureStepResponse`: the paper's step (below the threshold, 0 dBFS, below again; the square wave) and **a second, lower step**;
  the gain curve; the three variants simulated with the measured static curve and fitted (time constants in decades: a grid, coordinate
  descent, then the simplex of Nelder and Mead twice; the hold free between 0 and the measured one); the variant with the smallest error; the
  hold (after the fall, until the gain moved by 0.01 dB: an upper bound); the convention of ANSI S3.22 (attack within 3 dB, release within 4 dB
  of the final value; IEC 60268-8 and IEC 60118-2 still to be checked against their text). `measureLookahead`: noise below the threshold,
  the lag of the largest cross-correlation.
- Tests: every variant recognised (its error 0.0000 dB, the others 0.2 ... 0.6 dB), attack, release (and the detector's) within 2 % (in fact
  exact to 0.1 %), a hold of 30 ms found (29.99 ms), the look-ahead of 2 ms = 96 samples.
- Found: **two smoothers in series cannot be told apart by one step**: their order is (for linear smoothers exactly) interchangeable, only the
  static curve between them breaks the symmetry; with one step the fit found 3.7 / 5.8 ms for 5 / 2 ms at an error of 0.01 dB. A second step
  of another height separates them. A measured hold is an upper bound: a smoothed detector delays the start of the release too (2.5 ms for
  the two-stage reference without hold), so the hold is fitted.
- Not yet: the linear attack and the level-dependent and program-dependent release (the step size and the burst length), the block size and
  sample rate dependence.

### W11.3d detector, side chain, bands, stereo link (0.91.0, done)
- References: `reference::Compressor` with a side-chain high-pass (second-order Butterworth before the detector); `TwoBandCompressor` (a
  Linkwitz-Riley crossover of order 4, a compressor per band); `DynamicEq` (a band-pass feeds the detector, the reduction sets the gain of a
  peak filter at the same frequency and Q).
- `measure::measureDetector`: the paper's first test with a 4 kHz square, every second period zero (the RMS -3 dB, the peak stays). **A smoothed
  peak detector (Giannoulis' gain smoothing) is not an ideal one**: in the gaps its release runs, its reduction settles at c / (1 + tau_a / tau_r)
  whatever the gap length (the reference: 1.36 dB of the RMS detector's 2.26 dB). The decision compares the measured change with both
  predictions, the RMS one (the static curve at -3 dB) and the time model of W11.3c simulated as a peak detector (1.26 dB); the nearer wins.
  The paper's cross-check (the same RMS, a peak 12 dB higher: a pulse every 16 samples) is reported but does not decide: an attack slower than
  the pulses averages them (here by chance exactly the square's reduction).
- `measureThresholdAt`: the sine level for 1 dB of reduction (a 1 dB staircase); the side-chain high-pass at 1 kHz: -18.05 dBFS at 10 kHz,
  -15.08 at 1 kHz (its -3 dB), none at 100 Hz.
- `measureDynamicTransfer` (the two-tone test): a loud tone, quiet probes on a sixth-octave grid (whole hertz, those within a sixth of an
  octave of the tone left out: the two tones beat, the gain follows the beating and its sidebands fall on the probe); single band if every
  probe is reduced by more than half the largest reduction, else the width of the region within 3 dB of the largest: at least an octave a band
  of a multiband, narrower a dynamic EQ (a dynamic shelf would look like a band). Classified: the compressor (13.8 dB everywhere), the two-band
  compressor (4.5 octaves), the dynamic EQ (0.5 octaves).
- `measureStereoLink`: left at 0 dBFS, right at -30 dBFS: the right channel's reduction re the left's; 0, 0.5 and 1 within 0.02 of the link law.
- Not yet: the external side chain (a key input in the rig), program-dependent release.

### W11.3e distortion of the dynamics, limiters (0.92.0, done)
- `measure::measureDynamicsDistortion`: THD (the unit of W7.5) of a sine above the threshold per frequency. The reference (attack 1 ms):
  at 50 Hz -49.8 / -31.7 / -17.8 dB for a release of 500 / 50 / 5 ms (at 1 kHz -75.8 / -57.4 / -41.8 dB): the gain follows the waveform.
- `getTruePeakDbfs` (ITU-R BS.1770 annex 2: 4x oversampling; here a windowed sinc with 48 zero crossings per side, Blackman, every phase at
  unity DC gain, interpolated only where all taps lie inside the signal: zeros outside would be an edge of their own, ringing by 1 %; with 16
  zero crossings the ripple at fs/4 was 0.1 dB), `getSamplePeakDbfs`. A sine at fs/4 with 45 degrees: samples -3.01 dBFS, true peak 0.00 dBFS.
- `measureLimiter`: the fs/4 sine at +6 dBFS (sample peak, true peak), a step from -40 to +6 dBFS (the overshoot in the first 50 ms). The
  reference as a limiter (ratio 100, threshold -1 dBFS, attack 0.5 ms): sample peak -0.96 dBFS, true peak +2.05 dBFS (a sample-peak limiter
  lets the inter-sample peak through), overshoot 6.65 dB without look-ahead, 0.00 dB with 5 ms.
- Not yet: a true-peak limiter as a reference, the linear attack.

### W11.3f reports, views, music, loudness (0.93.0, done)
- **PluginLab Reference Compressor** (plugin): `reference::Compressor` with all its controls (threshold, ratio, knee, makeup, attack, release,
  hold, detector and RMS window, the three smoothing variants, look-ahead, stereo link, side-chain high-pass). The look-ahead is the reported
  latency (set again when the parameter changes, on the thread that sets it); bypassed, the input is delayed by it. pluginval 2.0.0, strictness
  10: passed (it found the two faults first: a log range with whole-hertz snapping for a time in ms, and a bypass without the latency).
- `measure::measureDynamicsResponse`: everything of W11.3a ... e on one device with the levels chosen from the fitted threshold (the quiet ones
  10 dB below the knee). The unit **"dynamics"** of the reports (`engine::measureDevice`): the static curve with the fit, hysteresis, the time
  model, the standards' times, the look-ahead, the detector, the bands, the link, the THD and the limiter test; "none" when the steady gain varies
  by less than 0.5 dB from -60 to 0 dBFS. `summarizeDynamics` for the summary item `dynamics` and the Developer page's column "Dynamics".
- **The look-ahead** is now measured from the step: the gain moves before the level reaches the output (seen through the latency); the model is
  fitted with the gain shifted by it. Without that, a plugin with 2 ms look-ahead fitted an attack of 0.1 ms with 2 dB error; with it 5.00 ms,
  0.00 dB. (The cross-correlation of noise gives the audio's delay, look-ahead and latency together.)
- **The reported latency at the setting**: `getReportedLatency` read the latency of a fresh instance at its defaults; now the setting is
  delivered and a block processed first (a look-ahead parameter changes it). This also concerns the echo and reverb units.
- `measure::measureLoudness` (ITU-R BS.1770-5): the K-weighting from its analog prototypes (at 48 kHz the standard's coefficients, impulse
  response identical to 1e-12), 400 ms blocks with 75 % overlap, the gates of -70 LUFS and -10 LU; the loudness range of EBU Tech 3342 (3 s every
  100 ms, -70 LUFS and -20 LU, 10th to 95th percentile); true peak, sample peak, crest factor. Tests with the cases of EBU Tech 3341 (1 kHz at
  -23 dBFS stereo: -22.99 / -22.99 / -23.01 LUFS at 44.1 / 48 / 96 kHz; the gates -72/-36/-23/-36/-72 dBFS: -23.01 LUFS) and 3342 (10 and 5 LU
  exact). The cases need their full lengths: shortened, more blocks lie across the steps and pass the gates (-23.19 LUFS with 1/10 of the length).
- `measure::getGainCurve`: the gain per 10 ms window as the least-squares gain sum x y / sum x^2 (no glitches at zero crossings), windows more
  than 50 dB below the loudest left out.
- Views (`ui::writeDynamicsViews`, next to the report): the static curve (rising and falling staircase, the model, 1 : 1, the threshold), the gain
  over the input level, the attack and the release phase with the fitted model over the measurement (the standards' times, look-ahead and hold
  marked), the two-tone test. `ui::renderMusicGainCurves` and `PluginLabHost --gain-curves <audio> <report> <plugin>...`: every plugin's gain
  curve on one time axis over the input's level, with loudness, loudness range, peaks, crest factor and the median gain of input and outputs.
  `tools/fetch_music_samples.sh` downloads the MusicRadar real-world drum pack (not in the repository).
- Real compressors at their defaults (48 kHz):

| Plugin | static curve | time model | look-ahead | detector | bands | link |
|---|---|---|---|---|---|---|
| Reference Compressor | -20.0 dBFS, 4.00 : 1, knee 6.0 dB | gain in dB: 5.00 / 50.0 ms | 0 | peak | single | 100 % |
| CTAGDRC 1.1.0 | -10.0 dBFS, 2.00 : 1, knee 6.0 dB | gain in dB: 1.99 / 139.9 ms (0.00 dB) | 0 | peak | single | 100 % |
| SocaLabs Compressor 1.1.11 | -30.0 dBFS, 5.00 : 1, knee 5.0 dB | linear level (atypical): 0.50 / 2.5 ms (0.00 dB; 0.93.0 without it: two stages, 0.03 dB) | 0 | peak | single | 80 % |
| ZL Compressor 0.6.1 | -32.0 dBFS, 3.00 : 1, knee 16.0 dB | gain in dB: 15.9 / 79.6 ms (0.00 dB) | 0 | peak | single | 126 % |
| ZeroComp 1.1.7 | none: the steady gain varies by 0.01 dB (no compression at its defaults) | | | | | |

  SocaLabs smooths the linear level (0.93.0 had no such variant; the two-stage model with its gain smoothing at the floor of 0.1 ms came
  nearest); its fast release makes 50 Hz distort (THD -15.8 dB at -6 dBFS). CTAGDRC leaves 11 of the 61 rising steps unsettled after
  0.5 s (0.06 dB of hysteresis). ZL Compressor turns the quiet channel down by 126 % of the loud one's reduction, which no link law of
  (1 - link) x + link max(x) gives (a mid/side or sum detector would; not examined yet).
- Gain curves on music (`W11Plugins/dynamics_reports/gain_curves_drums.md`, MusicRadar real-world drums, loop 01, 3.7 s, 44.1 kHz): the four
  compressors at their defaults on one axis; integrated loudness -13.8 LUFS in, out -19.4 (Reference), -15.5 (CTAGDRC), -27.2 (SocaLabs),
  -22.3 LUFS (ZL); the crest factor rises from 16.4 dB to 20 ... 22.5 dB for the three with attacks above 1 ms or a fast release (the transients
  pass, the body is turned down), CTAGDRC keeps it (16.4 dB).
- Not yet: the family of curves, threshold over frequency, the hysteresis loop (gates), the gain-reduction heat map and the level histograms
  of 6.

### W11.3f addendum: the linear level as a fourth time model (0.94.0, done)
- `TimeModel::LevelLinear` (and `reference::Smoothing::LevelLinear`, the Reference Compressor's choice "Level (linear)"): the level as an
  amplitude smoothed with attack and release, the gain computer unsmoothed after it. The author asked to flag it: the report calls it
  **atypical** ("atypical: the linear level is smoothed, so attack and release in dB depend on how far the level steps"; "(atypical)" in the
  summary and the views). Why: an exponential in the amplitude is not one in dB, so a large reduction recovers its first decibels quickly and
  its last ones slowly, a small one evenly; Giannoulis et al. (2012) smooth in the log domain for that reason. ("Non-musical" would judge the
  sound; "atypical" only says that the time behaviour does not follow the usual design.)
- Tests: the reference with the linear level recognised (error 0.0000 dB, the other three 0.04 ... 0.60 dB), attack and release exact; only
  this variant is flagged. SocaLabs Compressor now fits exactly (0.50 / 2.5 ms, 0.00 dB).

### W11.3g comparison and the dynamics domain of the matcher (0.95.0, done)
- `measure::compareDynamics`: the static curves at the same input levels (B - A), the fitted parameters, attack and release as ratios, the RMS
  difference of the step's gain curves, whether time model, detector and bands agree. Verdicts with **provisional limits** (question 6 below):
  levels the same within 0.5 dB, close within 1 dB; times the same within 10 %, close within 25 %. For a fair overlay B is measured with A's
  quiet level (`DynamicsMeasurementSettings::quietDbfs`), so both see the same step.
- `engine::measurePluginDynamics`, `engine::createDynamicsComparisonReport`, `ui::writeDynamicsComparisonViews` (static curves, attack and
  release overlaid) and `PluginLabHost --compare-dynamics <A> <B> <report>`.
- `match::DynamicsProbe`: a short stimulus (a 3 dB staircase of 0.25 s steps and one step -50 / 0 dBFS, 7 s of audio instead of the report's
  60 s), features: the steady gain per level and the gain at 0.1 ... 400 ms after the rise and the fall, in units of 0.5 dB.
  `makeDynamicsObjective`: within the tolerance when every feature is within one unit.
- **A start without a model**: Levenberg-Marquardt from the defaults (-20 dBFS, 4 : 1, 5 / 50 ms) stopped in a valley where threshold and
  ratio trade off (-27.1 dBFS, 14.7 : 1, 2 dB off at middle levels). `MatchProblem::scanStart(points)` (domain-independent): one pass over the
  released parameters on a grid of 9 values each, the best kept, then Levenberg-Marquardt. The Reference Compressor plugin matched to itself
  (-30 dBFS, 6 : 1, 10 / 100 ms): -30.04 dBFS, 5.92 : 1, 9.88 / 99.3 ms, largest feature 0.09 dB, 37 renders for the scan and 10 for the fit.
  A mapping of compressor parameters (as the EQ's of W8.5) would give a model start instead.
- Two real compressors compared (`W11Plugins/dynamics_reports/compare_CTAGDRC_ZL.md`): at their defaults CTAGDRC (-10 dBFS, 2 : 1) and ZL
  Compressor (-32 dBFS, 3 : 1) differ by up to 16.3 dB at 0 dBFS; attack x 8.0, release x 0.57, the same time model (gain in dB), the same
  detector and bands.
- Not yet: the comparison on music (the difference of the gain traces and of the short-term loudness after a loudness match), the fast path's
  determinism check, a Matcher page for dynamics.

## 9. Questions for the author
1. **Test set**: which free compressors, limiters and gates (a clean digital compressor, an 1176- or LA-2A-style model, a limiter, a gate)?
Answer: I will collect a list of free dynamics processors. We should have at least a feed-forward compressor (the reference), a feedback compressor (1176 style), a tube compressor (LA-2A style), a limiter and a gate in our own reference plugins. Perhaps we will find source code for other designs as well.
2. **Attack/release convention**: report both (63 % time constant and the AGC-standard tolerance time), or one as the main number?
Answer: I am not sure if this is enough. Especially the double log smoothing is not covered by the standards. And linear attack is also not covered. Hold beahviour to reduce movement of the gain is not covered. It would be great if the attack and release behaviour is also visualized. 
3. **The tone frequency** for the static curve: 1 kHz (with 100 Hz and 10 kHz for the side chain) right, or 997 Hz as in AES17?
Answer lets start with your suggestion. 
4. **Music-based comparison**: which material (the MusicRadar samples of `planning.md`)? Loudness after BS.1770 for the level match?
Answer: Yes, the MusicRadar samples are a good start. The loudness match should be done with BS.1770-5, since this is the current standard.
5. **Multiband and dynamic EQ**: in W11.3 or later (proposal: later; they combine the EQ work and this package)?
Answer: Later: And again, you could start the planning in your spare time. Multi-band is one class and dynamic EQ another one. 
6. **Limits of "the same" for compressors** (W11.3g): no standard gives just noticeable differences for a compressor. Provisional: levels
the same within 0.5 dB and audibly close within 1 dB, attack and release the same within 10 % and close within 25 %; the matcher's unit is
0.5 dB. Are these right, or do you know of listening tests (e.g. on attack and release discrimination)?

## 10. References
- U. Simmer, D. Schmidt, J. Bitzer, "Parameter estimation of dynamic range compressors: models, procedures and test signals," AES 120th
  Convention, Paris, 2006 (the two-stage model, the test signals, the fit; the author is a co-author).
- G. W. McNally, "Dynamic range control of digital audio signals," JAES 32(5), 1984, pp. 316-327.
- F. Floru, "Attack and release time constants in RMS-based feedback compressors," JAES 47(10), 1999, pp. 788-803.
- U. Zölzer (ed.), DAFX - Digital Audio Effects, Wiley, 2002 (2nd ed. 2011), chapter on dynamics processing.
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

## Comments of the author (v1)
* you should also read: Simmer, Schmidt, Bitzer, AES 120th Convention 2006 (v2: read; section 4.0 follows it)
* dont forget hold as an internal parameter
* especially for limiter a linear attack is sometimes used.
* I would like to see and compare gain curves of different compressors. This is a very important feature for musicians and engineers. This should be possible for musical input.
* It is important that we know, if the current plugin is multiband or an dynamic EQ. Can we test that?


## possible plugins:

https://loudmax.blogspot.com/

https://github.com/mzuther/Squeezer
(open Software and claims feedforward and feedback and Opto)

https://www.kvraudio.com/product/fircomp-1-by-jon-v-audio/downloads

https://github.com/ZL-Audio/ZLCompressor/releases/tag/0.6.1

https://www.auburnsounds.com/products/Lens.html
(Muliband)

https://www.kvraudio.com/product/tl-easycomp-by-tonelib/downloads

https://socalabs.com/effects/compressor/

https://www.kvraudio.com/product/zerolimit-by-jun-murakami/downloads
(open source?)

https://www.kvraudio.com/product/ctag-dynamic-range-compressor-by-billy-messenger/downloads

https://socalabs.com/effects/expander/
Expander

https://www.kvraudio.com/product/zerocomp-by-jun-murakami/downloads

https://www.kvraudio.com/product/aurora-by-13x-audio/downloads
Dynamic-EQ for the class testing

https://www.audiodamage.com/pages/roughrider3

/home/bitzer/AudioDev/FreeButcommercialPLugins/
you will find a compressor

