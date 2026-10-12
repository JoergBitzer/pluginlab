# W9: the matcher (plan for review)

Status: **draft v1, for the author's review** (2026-10-12). Nothing here is decided. Milestone of `planning.md` §8: "**Matcher** (user chooses
parameters, start point, frequency range, tolerance; real-time deviation display). Done when: master reproduced on each test plugin or the
residual quantified and attributed." The questions for the author are in section 8.

## 1. What W9 has to answer
For a **master** (a plugin at a setting, or a curve) and a **target** plugin:
1. **Which setting of the target comes closest to the master?** Within the parameters the user releases, the frequency range and the tolerance
   the user chooses, from a start point the user or the tool chooses.
2. **How close is it?** The deviation over frequency (magnitude), its largest and rms value, and the null depth after gain and delay
   alignment (a magnitude match is not a null: prototype, ZL EQ 2 matched within 0.03 dB reached only -58.6 dB null depth).
3. **Why not closer?** If the tolerance is not reached, the residual is **attributed**: a knob at the end of its range, a different design
   family (Orfanidis against RBJ near Nyquist), a band the target does not have, a sample-rate fault (W8.6), bands that do not add (W8.7), a
   non-linear or time-variant target (W8.1), or "not found" (the optimizer, with the spread of several starts).
4. **Live:** while the user turns the target's knobs by hand, the deviation is shown and updated a few times per second.

What W9 does **not** do: correct knob laws (W8.6: only deviations are reported; the matcher measures, it does not trust the knobs), match
dynamics, delay or reverb (W11), or match by listening (later, perceptual metrics).

## 2. What already exists (and what W9 builds on)
| From | What | Used for |
|---|---|---|
| W4 | the engine: plugin slots that render in parallel, latency compensation, gapless switching | master and target side by side, listening to the match |
| W7 | swept-sine frequency response, null test with alignment, phase and group delay | the deviation and the null depth |
| W7.11 | the plugin device (careful delivery: fresh instance, poke, settle) | every render of the optimizer |
| W8.1 | system class | whether a magnitude match means anything; the attribution |
| W8.2/W8.3 | band model and band fit | the master's bands, the start point, the family attribution |
| W8.4/W8.5 | survey, mapping (which parameter is which band quantity, requirements, switch steps), `makeSettingForBand` | the parameters to optimize, the start point, the switches |
| W8.6 | calibration (deviations of the knobs at 44.1/48/96 kHz) | the attribution, the "examples" the author mentioned |
| W8.7 (open) | interactions between bands | the attribution for multi-band matches (W8.7 should come first, see section 7) |
| prototype | CMA-ES on the magnitude response, one peaking band, ~400 renders, 0.1 dB tolerance | lessons: seeds, knob ranges, mapping |

## 3. Design (proposals)
### 3.1 Master
- **A plugin at a setting** (a slot of the host, or a plugin file and its parameter values): measured like the target.
- **A band-model curve** (bands typed in or taken from a band fit): exact, no render; the teaching case ("make this EQ do an RBJ peak at
  1 kHz, +6 dB, Q 2").
- **A measured response** (a file, e.g. from a hardware unit): proposed for later (section 8, question 3).

### 3.2 The deviation (the objective)
- Magnitude in dB on a log grid (12 points per octave), in the user's frequency range (default 20 Hz ... 20 kHz, at most 0.45 fs), the
  master's gain removed only if the user releases "output gain"; weights per third octave (as the band fit, W8.3), optionally more weight
  where the master deviates from flat (where the EQ acts).
- Reported: largest and rms deviation, the curve; the **null depth** after alignment (W7 null test) as a second number, not optimized by
  default (question 2).
- Tolerance: default 0.1 dB largest deviation (the prototype's value); the optimizer stops when it is reached.

### 3.3 The parameters
- **Continuous** parameters normalised to 0 ... 1 (the host's values), bounded; the mapping (W8.5) says which are band quantities, the user
  releases, locks or ignores each one. Locked: kept at its value; ignored: kept at the start value.
- **Discrete** parameters (type, family, band on/off, stepped knobs): not inside the continuous optimizer; an outer loop over the steps the
  mapping offers for the wanted band type (few: a peak-type switch has one or two candidate steps), each step optimized, the best kept.
- **Requirements** of the mapping (a band switch on, a gain that makes a frequency act) are set before the optimizer starts.

### 3.4 The start point
1. **From the models** (no render): the band fit of the master (W8.3) gives its bands; the target's mapping gives a setting for them
   (`makeSettingForBand`, measured laws). For twins (both RBJ) this start is already within the tolerance; otherwise it is near.
2. **By the user**: the target as it is in its slot.
3. **Several starts** (seeds) around 1 or 2; the best is kept and the spread reported (prototype lesson).
Multi-band: the master's bands are assigned to the target's bands (by type and frequency; a band the target cannot make is reported, not
forced).

### 3.5 The optimizer
- **Stage 1, local and fast:** Levenberg-Marquardt on the deviation curve, numerical derivatives (one render per parameter and step),
  from the model start. For twins it converges in a few iterations (tens of renders).
- **Stage 2, global and robust, only if stage 1 misses the tolerance:** CMA-ES (the prototype's method, ~300 ... 500 renders for 3 parameters)
  with several seeds.
- Every render through the plugin device; renders of the same setting cached; a budget (renders and seconds); progress like `--map`; runs in a
  process of its own (a crashing target does not take the host down).

### 3.6 Attribution of the residual
After the best match, in this order (each with its evidence):
| Cause | Test |
|---|---|
| target not LTI | system class of the target at the found setting (W8.1) |
| knob at the end of its range | a released parameter within 1 % of 0 or 1, and the deviation falls when the bound is relaxed in the model |
| band missing | the master's band fit has more bands of a type than the target's mapping offers |
| design family | the target's best response fitted by the band model: a family other than the master's, and the model of the master's bands in the target's family reproduces the residual |
| sample rate | the calibration (W8.6) of the target shows a rate-dependent deviation, and the match holds at another rate |
| interaction | bands do not add (W8.7): the single-band matches are within the tolerance, the combination is not |
| not found | none of the above; the spread over the seeds is large |

### 3.7 Sample rates
Matched at 48 kHz; the found setting checked at 44.1 and 96 kHz (does the match hold?). A target that is right at one rate only is
reported (the PeakEQ case: matched at 48 kHz with the knob at 919 Hz for a 1 kHz master).

### 3.8 Non-LTI targets and masters
Magnitude matching assumes LTI. For a non-linear target the match is made at the level of the sweep (-20 dBFS) and marked "small signal";
for a time-variant one it is refused with the reason (question 5).

### 3.9 Live deviation (the "real-time display")
While the user turns the target's knobs in its editor (Matcher page of the host): a short synchronized sweep (about 0.3 s) through master
and target, repeated a few times per second, both curves and their difference drawn, the largest and rms deviation as numbers. The renders
use copies of the parameter values of the hosted instances (offline, not the audio path), so listening continues.

## 4. Work packages
| Step | Content | Done when |
|---|---|---|
| **W9.1** | **Deviation** (`Comparison`): magnitude difference on the grid in a range with weights, largest and rms; the null depth after alignment; a master as plugin setting or band-model curve | Two reference processors give the analytic difference (1e-6 dB); the null depth agrees with W7's null test; RBJ against Orfanidis at 1 kHz / 10 kHz shows the known Nyquist difference |
| **W9.2** | **Optimizer core** on devices without plugins (reference processors): normalised bounded parameters, LM stage, CMA-ES stage, seeds, budget, cache, discrete outer loop | From 20 random starts the known setting of an RBJ peak, a shelf, a Butterworth low-pass (order as discrete) and a three-band reference is recovered (frequency 0.1 %, gain 0.01 dB, Q 0.5 %); renders and spread reported |
| **W9.3** | **Start point from the models**: master band fit, band assignment, the target's mapping (`makeSettingForBand`); user start | For twins (Reference EQ RBJ master, Test EQ target) the start alone is within 0.1 dB; for the others it saves at least half the renders of a random start |
| **W9.4** | **Plugin matching**: target through the plugin device, parameters released / locked / ignored from the mapping, requirements, progress, own process; `PluginLabHost --match <master> <target> <report>` | The test pairs of section 5 within their tolerance or attributed; a crashing target ends the job, not the host |
| **W9.5** | **Attribution** (section 3.6) | Every test pair whose tolerance is missed names the cause the code has built in |
| **W9.6** | **Sample rates and robustness**: check at 44.1/96 kHz; seeds and spread; the knob-range limits | The Fs Fault pair is reported as rate-dependent; a knob-range limit is found as such |
| **W9.7** | **Host: Matcher page**: master and target slots (or a band curve), parameter list (release / lock / ignore), range, tolerance, start; the live deviation display while turning knobs; run with progress; apply the result to the hosted target; listen (gapless switching of W4); the report | The author matches a plugin to a master from the host and hears both |
| **W9.8** | **Report and documents**: Markdown report (master, target, found setting in the plugin's texts, deviation plot, null depth, attribution, renders and time, seeds), `docs/matching/` documents, a teaching page ("same EQ, different knobs") | Report readable in the host's report window; documents and the teaching page reviewed by the author |

Order: W9.1 and W9.2 need no plugin (reference processors), then W9.3 ... W9.6 with the test plugins, then W9.7 and W9.8. Each step: design
note in this file, code, tests, document, version, commit (as in W7 and W8).

## 5. Test pairs (master -> target; our own plugins only, section 5 of the W8 plan)
| Master | Target | Expected |
|---|---|---|
| Reference EQ, RBJ peak 1 kHz +6 dB Q 2 | Test EQ (RBJ) | exact (twins), start point alone within the tolerance |
| Reference EQ, Orfanidis peak 10 kHz +6 dB Q 1 | Test EQ (RBJ) | residual near Nyquist, attributed to the design family |
| Reference EQ, RBJ peak 1 kHz | Test EQ Fs Fault at 48 kHz | match with the knob at about 919 Hz; does not hold at 44.1/96 kHz: sample rate |
| Reference EQ, RBJ peak | Test EQ Knob Faults | exact match although every knob lies (frequency x 1.05, gain x 0.5, Q a bandwidth): the matcher measures |
| Reference EQ, Butterworth low-pass order 4 | Reference EQ, RBJ (only Q released) | residual: the order cannot be reached; attributed to "band missing / family" |
| Reference EQ, peak at 18 kHz | Test EQ with its frequency knob limited (a new test build) | knob at the end of its range |
| Band-model curve, two peaks | Test EQ (one band) | band missing |
| Reference EQ peak | Test EQ Drive (Drive on) | matched as small signal, marked non-linear |
| Reference EQ peak | Test EQ Switch | requirements (the switch) set by the mapping |
The author's two test objects: local runs, results in the author's words (as in W8).

## 6. Risks
- **Renders are slow** (Debug: 0.3 ... 1 s per render with the careful delivery): LM first, cache, short sweeps for the live display, worker
  processes for CMA-ES in parallel (W4 slots) if needed.
- **Plugins whose parameters are texts** (Soundly Shape it: 1001 frequency texts): treated as discrete with many steps; the optimizer needs a
  continuous view (the step index normalised). To test with such a plugin locally.
- **Smoothing and settling** (W5): the plugin device's settle time must cover the target's smoothing, or the optimizer chases a ramp.
- **Band assignment** for multi-band EQs with several bands at the same default frequency (W8.5 limit): the user may have to assign.

## 7. Dependencies on W8
W8.7 (interactions) is open. Single-band matching (W9.1 ... W9.4) does not need it; the multi-band attribution (W9.5) does. Proposal: W9.1 and
W9.2 now, W8.7 before W9.5. W8.8 (confirming a mapping and the calibration in the host) and W9.7 (the Matcher page) share the host work; the
mapping confirmation could move into W9.7.

## 8. Questions for the author
1. **Optimizer**: LM from the model start with CMA-ES as fallback (proposal), or CMA-ES only as in the prototype?
2. **Objective**: magnitude only (dB, weighted), null depth reported but not optimized (proposal); or an option to match the complex response
   (phase, linear-phase EQs)?
3. **Master types**: plugin setting and band-model curve now (proposal); a measured response file (hardware) later or now?
4. **Live display**: is "a few updates per second with a short sweep, drawn on the Matcher page" what you mean by real time, or should it run
   on the audio path (music through both, a running transfer-function estimate)?
5. **Non-LTI**: match a non-linear target as small signal with a mark (proposal), refuse it, or match at several levels?
6. **Discrete parameters**: may the matcher change type and family switches by itself (within the steps the mapping offers), or only the
   user?
7. **Budget**: default limit per match (e.g. 500 renders or 5 minutes in Debug)? Parallel renders (several instances) allowed?
8. **Results**: where does a found setting go: applied to the hosted target only, saved as a preset of the target, or as a "match file"
   (master, target, setting, deviation) next to the mappings?
9. **Teaching**: which matches should the teaching page show (proposal: RBJ against Orfanidis near Nyquist; the knob-fault plugin; the
   PeakEQ sample-rate case)?
