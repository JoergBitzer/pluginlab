# W9: the matcher (plan)

Status: **plan v2, W9.1 done** (v1 2026-10-12 for review; v2 the same day after the author's answers and comments, section 9). Milestone of
`planning.md` §8: "**Matcher** (user chooses parameters, start point, frequency range, tolerance; real-time deviation display). Done when:
master reproduced on each test plugin or the residual quantified and attributed."

## 1. What W9 has to answer
**The main case (the author):** a user owns one EQ and wants to hear how several new ones compare. The owned EQ at its setting is the
**master**; each new EQ is a **target**. The matcher sets every target so that its transfer function equals the master's (within the
parameters the user releases), and the user then **listens** to the matched plugins and hears that there is no magic once the transfer
function is fitted. The reference plugins serve the tutorials, the teaching and "myth and reality".

For a master and one or more targets:
1. **Which setting of each target comes closest to the master?** Only the parameters the user releases are moved, in the frequency range
   and to the tolerance the user chooses, from the start point the user (or the tool) chooses.
2. **How close is it?** The deviation over frequency, its largest and rms value; the null depth after gain and delay alignment as a
   second number (a magnitude match is not a null).
3. **Why not closer?** The residual attributed: a knob at the end of its range, a different design family (Orfanidis against RBJ near
   Nyquist), a band the target does not have, bands that do not add (W8.7), or "not found".
4. **Live:** while the user turns a target's knobs, short clicks or sweeps measure master and target, and the deviation is shown. **No
   audio plays during matching**; listening comes after it.

Not in W9: phase matching (later: linear phase, minimum phase), non-LTI targets (refused, later), files as masters (no file-based
processing), parallel renders.

## 2. Architecture: a general matcher, EQ as its first domain
The author's comment: the design must not be tied to EQs; matching a compressor or a reverb works completely differently. What all have
in common: **parameters are optimized against a given objective.** So W9 has two layers.

**The general layer** (no knowledge of EQs):
| Part | What | For an EQ (W9) | For dynamics / reverb (later, W11) |
|---|---|---|---|
| `ParameterSpace` | the released parameters: index, normalised bounds (a sub-range of 0 ... 1 if the user narrows it), start value; locked parameters with their values | frequency, gain, Q of the bands the user releases | threshold, ratio, attack, release; decay, pre-delay, damping |
| `Probe` | what is sent through a device and what is read from it: a stimulus and a feature vector (with weights) | an impulse or a short sweep; magnitude in dB on a log grid in the user's range | level steps and bursts: static curve and time constants; an impulse: decay per band |
| `Objective` | the residual between the master's and the target's features (a vector, for least squares) and its scalar (largest, rms); the tolerance | dB difference, third-octave weights | curve and time-constant differences; decay-time differences |
| `Optimizer` | moves the parameters to reduce the residual within a budget | Levenberg-Marquardt (numerical derivatives) | the same or another (CMA-ES can be added behind the same interface) |
| `StartPoint` (optional) | a domain's guess of a good start | from the band model and the mapping (W8) | from the domain's own model |
| `Attribution` (optional) | why the residual stays | the EQ causes of section 4.4 | the domain's own |
| `MatchJob` | master, targets, the parts above, the budget; runs the targets one after another; progress; the result per target | | |

**The EQ domain** (W9) supplies the probe (magnitude response), the objective (weighted dB), the start point (band fit and mapping) and the
attribution. A later domain (dynamics, reverb) supplies its own probe, objective and attribution and reuses the rest unchanged.

## 3. What already exists
| From | What | Used for |
|---|---|---|
| W4 | the engine: plugin slots, latency compensation, gapless switching | master and targets side by side; **listening after the match** |
| W5 / W5b | the fingerprint: delivery strategy that works, settle time, determinism | the fast render path (section 4.2) |
| W7 | swept-sine response, null test with alignment | the probe, the null depth |
| W8.1 | system class | refusing non-LTI targets |
| W8.2/W8.3 | band model, band fit | the master's bands, the start point, the family attribution |
| W8.4/W8.5 | survey, mapping, requirements, `makeSettingForBand` | which parameters a user can release, the start point |
| W8.7 (open) | interactions between bands | the attribution for several bands |
| prototype | CMA-ES on the magnitude response, ~400 renders | lessons: knob ranges, seeds, mappings |

## 4. Design of the EQ domain and the job
### 4.1 Master and targets
- **Master**: a plugin at its setting (a slot of the host; the user's own EQ), or a **band-model curve** for the teaching (no render; "an
  RBJ peak at 1 kHz, +6 dB, Q 2"). No files.
- **Targets**: one or more plugins (slots); matched one after another against the same master, each with its own released parameters.
- Every master and target must be LTI at the setting used (system class, W8.1); otherwise it is **refused** with the reason.

### 4.2 The fast render path (the budget: 10 ... 15 s per target, no parallel renders)
A match of three parameters with LM needs about 4 renders per iteration and up to 10 iterations: some 40 renders, so a render may take
about 0.25 s. The careful delivery of the plugin device (a fresh instance per render, W7.11) is too slow for this. So:
- **one instance per target stays alive** during the match; parameters are set on it the way its fingerprint says works (the delivery
  strategy of W5), then the plugin settles for the time its fingerprint measured, then the probe;
- **the probe is short**: an impulse (or a sweep of about 0.2 s) after the settling; the response read up to the length the band model needs;
- **a self-check at the start and the end**: the same setting rendered twice must agree (determinism, W5); otherwise the fast path is
  not valid for this plugin and the match is refused with the reason;
- renders of a setting already measured come from a cache.
The budget is a time limit (default 15 s per target) and a render limit; when it is reached, the best setting so far is the result.

### 4.3 The parameters and the start point
- **Only the user decides what is optimized** (the author: plugins have many parameters). The Matcher page lists the target's parameters
  (with the mapping's names and quantities where a mapping exists); the user releases some, optionally narrows their range, the others stay
  as they are. Switches (type, family, band on/off) are set by the user, never by the matcher.
- **Start point**: the target as the user left it; or, if the user asks for it and a mapping exists, the setting from the model (band fit of
  the master, the target's mapping, `makeSettingForBand` for the released quantities); for twins this start is already within the tolerance.
- Requirements of the mapping (a band switch, a gain that makes a frequency act) are shown to the user, not set behind their back.

### 4.4 The objective, the optimizer and the attribution
- **Objective**: magnitude in dB on a log grid (12 points per octave) in the user's range (default 20 Hz ... 20 kHz, at most 0.45 fs),
  third-octave weights; the tolerance default 0.1 dB largest deviation (`planning.md`: "equal" = within 0.1 dB). Output gain is matched only
  if the user releases a gain parameter for it. Phase: later.
- **Optimizer**: Levenberg-Marquardt with numerical derivatives in the normalised parameters, bounded (projected onto the bounds); stops at
  the tolerance, when it stalls, or at the budget. CMA-ES only if LM proves not enough (the author: "we can add CMA-ES later").
- **Attribution** (when the tolerance is missed), in this order:
  | Cause | Test |
  |---|---|
  | knob at the end of its range | a released parameter at its bound |
  | design family | the band fit of the matched target names another family than the master's, and the master's bands in the target's family reproduce the residual |
  | band missing | the master has more bands of a type than the released parameters can make |
  | interaction | bands do not add (W8.7) |
  | not found | none of these (the optimizer; a second start from the model start point is tried once) |

### 4.5 Results and listening
- Per target: the found setting (in the plugin's texts), the deviation curve, largest and rms deviation, null depth, the attribution,
  renders and seconds.
- The found setting is applied to the target in its slot and can be **saved as a preset of the target** (the plugin's state; the author:
  "preset is a good idea").
- **Listening** after the match: the engine of W4 plays the user's audio through master and matched targets with gapless switching and
  latency compensation (A/B/C/D), so that the user hears that the matched EQs sound alike.

### 4.6 The live deviation
On the Matcher page, while the user turns a target's knobs in its editor (no audio playing): the fast render path probes master and target
a few times per second (an impulse each), both magnitude curves and their difference are drawn, largest and rms deviation shown.

## 5. Work packages
| Step | Content | Done when |
|---|---|---|
| **W9.1** | **General layer**: `ParameterSpace`, `Probe`, `Objective`, `Optimizer` (LM), `MatchJob` with budget, cache, progress; tested on devices without plugins | A synthetic objective (a function with a known minimum) and a reference processor with known settings are matched from several starts; budget and cache work |
| **W9.2** | **EQ domain**: magnitude probe, weighted dB objective, band-model curve as master; matching reference processors (no plugin) | RBJ peak, shelf, Butterworth low-pass (frequency, gain, Q released) recovered from user starts (frequency 0.1 %, gain 0.01 dB, Q 0.5 %); Orfanidis master against an RBJ target leaves the known residual near Nyquist |
| **W9.3** | **Fast render path** for plugins: one live instance, delivery and settle time from the fingerprint, short probe, self-check, refusal of non-LTI and non-deterministic targets | The response of the fast path agrees with the careful one (W7.11) within 0.01 dB for the reference and test plugins; a render takes at most 0.25 s (Release); the tremolo and the drive are refused with their reason |
| **W9.4** | **Plugin matching**: master and targets as plugins, the user's released parameters and ranges, the model start point (optional), several targets one after another, 15 s per target; `PluginLabHost --match` for tests | The test pairs of section 6 within the tolerance or attributed, each within 15 s (Release) |
| **W9.5** | **Attribution** (section 4.4); needs W8.7 for the interaction cause | Every test pair that misses the tolerance names the cause the code has built in |
| **W9.6** | **Host: Matcher page** in the stand-alone application (two main targets, `planning.md` §1): master slot (or a band curve), target slots, the parameter list (release, range), frequency range, tolerance, start point; the live deviation; run with progress; apply; save as preset; listen with gapless switching | The author matches two or three plugins to his own EQ from the host and listens to them |
| **W9.7** | **Report, documents, teaching**: Markdown report per job (master, targets, settings, deviation plots, null depths, attribution); `docs/matching/`; the teaching page "Orfanidis against RBJ" (later: a Butterworth low-pass against a matched design) | Report in the host's report window; documents and the teaching page reviewed by the author |

Order: W9.1 and W9.2 need no plugin; W9.3 is the risk (speed) and comes before W9.4; W8.7 before W9.5. Each step: design note here, code,
tests, document, version, commit.

## 6. Test pairs (our own plugins; the author's test objects locally)
| Master | Target (released) | Expected |
|---|---|---|
| Reference EQ, RBJ peak 1 kHz +6 dB Q 2 | Test EQ (frequency, gain, Q) | exact (twins) |
| Reference EQ, Orfanidis peak 10 kHz +6 dB Q 1 | Test EQ (frequency, gain, Q) | residual near Nyquist, attributed to the design family (the teaching case) |
| Reference EQ, RBJ peak | Test EQ Knob Faults (frequency, gain, Q) | exact although every knob lies: the matcher measures |
| Reference EQ, RBJ peak 1 kHz at 48 kHz | Test EQ Fs Fault (frequency, gain, Q) | exact at 48 kHz with the knob at about 919 Hz |
| Reference EQ, peak at 18 kHz | a test build with the frequency knob limited | knob at the end of its range |
| Band-model curve, two peaks | Test EQ (one band) | band missing |
| Reference EQ peak | Test EQ Switch (frequency, gain, Q; the user switched the band on) | exact |
| Reference EQ peak | Test EQ Drive (drive on) | refused: not LTI |
| Reference EQ peak | Reference Utility, tremolo on | refused: not LTI |
| Reference EQ, RBJ peak | three targets at once (Test EQ, Knob Faults, Fs Fault) | each matched within its 15 s; the job's report lists all three |

## 7. Risks
- **Speed** (W9.3): plugins that smooth slowly (the fingerprint's "settles within") make each render longer; the match then takes more than
  15 s or is stopped by the budget with the best setting so far (reported as such).
- **Delivery quirks** (W5): a plugin whose parameters arrive only on a change, or only after prepare: the fast path uses the strategy the
  fingerprint found; the self-check catches the rest.
- **Plugins with text parameters** (1001 frequency texts): discrete with many steps; LM on the step index normalised (to be tried locally).
- **Many bands**: the user releases only the bands that matter; the model start point helps with the assignment.

## 8. Dependencies
W8.7 (interactions) before W9.5. W8.8 (confirming a mapping and the calibration in the host) and W9.6 (the Matcher page) share the host
work. W9.6 is built in the stand-alone application first (two main targets, `planning.md` §1); the plugin gets the Matcher with W10.

## 9. Answers of the author (2026-10-12)
To the questions of v1, in the author's words:
1. **Optimizer** (LM with CMA-ES as fallback, or CMA-ES only): "LM is enough, we can add CMA-ES later if needed."
2. **Objective** (magnitude, or the complex response): "Amplitude first, linear-phase or minimum phase later."
3. **Master types** (plugin setting, band-model curve, a measured file): "No file based processing."
4. **Live display** (short sweeps on the Matcher page, or audio through both): "No audio during matching. You can send clicks or sweeps
   through to get the transfer function."
5. **Non-LTI**: "Refusal, that is for later."
6. **Discrete parameters** (may the matcher change switches): "Only the user: Several plugin have so many parameters. It is the users choice
   what to optimize."
7. **Budget and parallel renders**: "No parallel renders, matching should stop after 10-15s. For LTI Systems this should be enough."
8. **Results**: "preset is a good idea. The main idea is that users listen to the matched different EQs to see there is no magic, if the
   Transfer function is fitted."
9. **Teaching**: "No, Orfanidis vs. RBJ. And in the long run Butterworth Lowpass vs a matched Design."

Comments:
- "The whole design looks to me to close to the EQ problem. We need a very open systems, since matching dynamic processor reverb processor
  will work completely different. What is common is: parameters are optimized against a given optimization routine." → section 2.
- "The main case will be a user who owns one EQ and wants to compare the sound to several new ones. Our reference plugins are for tutorials
  and teaching and of course to divide myth from reality." → section 1, 4.1, 4.5, 6.

## Progress
### W9.1 general layer (0.56.0, done)
Library `pluginlab_match` (`src/match/Match.h`): `ParameterSpace`, `Probe`, `Objective` (`WeightedDifference`), Levenberg-Marquardt with bounds,
budget (evaluations, seconds) and "at bound" flags, `MatchProblem` with its cache. Document `docs/matching/matcher-core.md`; tests
`tests/MatchTests.cpp` (linear least squares exact, Rosenbrock in 64 evaluations, bounds, budgets, parameter space, cache).

