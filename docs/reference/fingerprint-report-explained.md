# The fingerprint report: what is measured, how, and how to read it

Version 0.15.0 (2026-10-06, after the revision steps R1 to R5 of `docs/design/W5b-fingerprint-revision.md`). Code: `src/engine/Fingerprint.cpp`,
`src/engine/LatencyMeasurer.cpp`, settings `src/engine/FingerprintSettings.{h,cpp}`. Report text: `createReport()`.
Purpose: say exactly what every line of the report means, so that a result can be judged as "the plugin does this" or "our test does this", and list the
places where the test itself is still weak (section 6; the open ones are planned as R4 to R7). Section 7 is a checklist for a surprising result.

The fingerprint is **technical only**: it runs noise, a delayed impulse and silence through a plugin and moves its parameters. Nothing in it assumes a kind of
algorithm (EQ, compressor, ...) or a linear, time-invariant plugin; frequency responses belong to the analyzer (W7).

## 1. Overview: what happens when a report is made

`PluginLabHost --fingerprint <plugin file> <report> [<plugin id>]` (the Developer page starts it in a process of its own) reads the settings (2.5), scans the file in
its own process, takes the plugin (the one with the identifier, or every plugin of the file), and runs these steps in this order. Every **render** (2.3) loads a
**new instance**; a report needs about 50 instances plus two per parameter (and two more for every parameter that did not react in the first pass).

| Step | What | Result in the report |
|---|---|---|
| 0 | load one instance at 48 kHz / 512; choose the channel layout | `not measured` if this fails |
| 1 | channel layouts and parameter list from that instance | "channels", table "Parameters" |
| 2 | which parameters change the audio (noise, two passes) | columns "changes the audio", "measured with" |
| 3 | build the setting B (the parameters that matter at the high position) | (used below; printed with R7) |
| 4 | latency per sample rate 44.1 / 48 / 96 kHz (delayed impulse) | table "Latency at three sample rates" |
| 5 | A, A, B, A test in four ways of delivering parameters | table "Delivery" |
| 6 | block sizes against 512, 1 s render, steady state and whole | table "Block sizes" |
| 7 | determinism, parameter jumps with recovery, silence in/out | block "Other" |
| 8 | "Findings" at the top, derived from steps 2 to 7 by fixed rules | list "Findings" |
| 9 | the settings the measurement used | block "Settings used" |

## 2. The ingredients

### 2.1 Test signals
| Signal | Exact definition |
|---|---|
| **Noise L = R** | `juce::Random` with seed 7, uniform white noise in [-noiseLevel, +noiseLevel] (default 0.1: peak -20 dBFS, RMS -24.8 dBFS), the same samples on every channel. Used in the parameter scan only. |
| **Noise L != R** | seed 7 on channel 1 (L), seed 11 on channel 2 (R) and further channels: uncorrelated. **The signal of every other measurement** (parameter scan as second signal, setting B, delivery, block sizes, determinism, jumps). 12288 samples, `blockRenderSeconds` (1 s) for the block size test. Every render uses the same sequences, so renders are comparable sample by sample. |
| **One channel driven** | the L noise on one channel, silence on the other (channel coupling). |
| **Impulse** | `impulsePreDelaySamples` (4096) samples of silence, then 1.0 (0 dBFS) on every channel, then silence for `latencyObserveSeconds` (1 s). |
| **Silence** | zeros. |
| **Poke block** | the first 512 samples of the noise, **the same length for every block size** (so that the input history before the measured signal is equal). |

### 2.2 The channel layout
`ChannelAdapter::chooseLayout(instance, 2)`: the main buses get stereo, else mono; **other buses (a side-chain input, extra outputs) are switched off** if the plugin allows it,
else they keep their default layout and get silence (the engine and the loader refuse such a plugin for now). **All main-bus output channels are analysed**: a difference is the
difference of the channel where it is largest (the report names the channel when it is not the first). No MIDI is sent.

### 2.3 The building blocks
- **make(rate, block)**: `createPluginInstance(description, rate, block)`, then the layout.
- **prepare()**: `setPlayConfigDetails(ch, ch, rate, block)` and `prepareToPlay(rate, block)`.
- **apply(setting)**: `setValue(v)` of every examined parameter (the first `maximumParameters` = 64), `v` normalised 0 ... 1. `defaults` = `getDefaultValue()`.
- **process(signal)**: in blocks of `block` samples; output channel 0 collected; a NaN or infinity is remembered.
- **settle()**: `settleSeconds` (0.25 s) of zeros, a time, not a number of samples (12000 / 11025 / 24000 samples at 48 / 44.1 / 96 kHz).
- **render(rate, block, setting, input)**, the careful way used by every measurement in steps 2, 3, 6, 7:
  1. make (new instance), prepare
  2. apply the **poke values**: every parameter first set to another value (`v - pokeDistance` if `v > 0.5`, else `v + pokeDistance`; default 0.4)
  3. process the poke block (512 samples of noise)
  4. apply(setting)
  5. settle
  6. process(input) → output channel 0

  The poke exists because some plugins read a parameter only when it changes (the PeakEqualizer template).
- **renderAfterPrepare**: the same without steps 2 and 3 (one of the delivery ways).

### 2.4 Differences: relative and absolute
Every difference of an output a from a reference b over a window is given as **relative / absolute**:
- **relative** = 20 log10( RMS(a - b) / RMS(b) ) in dB ("null depth relative to the reference");
- **absolute** = 20 log10( RMS(a - b) ) in dBFS;
- **identical** = bit exact (RMS(a - b) = 0).

Reading the relative value: **0 dB** = the change is as large as the signal itself (+6 dB of gain: difference 1.0 x the signal); **+6 dB** = twice the signal
(a polarity inversion); **+9.5 dB** = +12 dB of gain (difference 2.98 x); **-2.5 dB** = 0.75 x (gain 0.25 or 1.75); **-40 dB** = 1 %.
Why relative to the reference and not symmetric (decision D1 of W5b): for nearly equal signals every denominator gives the same up to a constant, and for large
differences the reference form keeps the size of the change (a symmetric sum form is capped at 0 dB). If the reference is silent (RMS below `silentReferenceDbfs`,
-150 dBFS) the relative value has no meaning: it is marked "(silent reference)" and the **absolute value decides**.

### 2.5 Settings and decision thresholds
`~/.config/pluginlab/fingerprint_settings.json` (application data folder on Windows/macOS; the environment variable `PLUGINLAB_FINGERPRINT_SETTINGS` names another file, the
tests use it). A missing file is written with the defaults; a missing key keeps its default; a file that is not JSON gives the defaults and a line "settings: ..." in the report
header. Every report ends with the values it used ("Settings used").

| Key | Default | Used for |
|---|---|---|
| `reactsAboveDb` | -80 | a parameter changes the audio |
| `differentAboveDb` | -60 | B differs from A; a candidate parameter is kept in B |
| `sameBelowDb` | -80 | two outputs count as the same (repeatable, as after a change, recovers) |
| `blockIndependentBelowDb` | -100 | block size independent (steady state) |
| `silentReferenceDbfs` | -150 | a reference below this level is silent: the absolute difference decides |
| `noiseLevel` | 0.1 | peak of the noise |
| `settleSeconds` | 0.25 | silence after every parameter change before a measured signal |
| `impulsePreDelaySamples` | 4096 | silence before the impulse |
| `latencyObserveSeconds` | 1.0 | how long the impulse response is watched |
| `latencyMinimumPeak` | 1e-4 | below this peak "nothing came out" |
| `blockRenderSeconds`, `blockCompareSeconds` | 1.0, 0.1 | block size test: render length and the end that is compared |
| `blockSizes` | 32, 64, 128, 256, 1024, 2048, 509 | compared with 512 |
| `lowSetting`, `highSetting` | 0.25, 0.75 | the two test positions of a parameter |
| `pokeDistance` | 0.4 | see 2.3 |
| `maximumParameters`, `maximumJumpedParameters` | 64, 16 | limits |
Fixed in the code: block size 512 and 48 kHz as reference, 12288 noise samples of which the last 8192 are compared, "none" below -120 dB in the latency table,
determinism = bit exact.

## 3. The report, section by section

### 3.0 Summary (first section, R5)
One row per single result: the test, the result, and the number behind it. **Bold** results are worth a look; each has a finding with the details. The rows, in
order (key in the JSON in brackets): loads and runs with mono or stereo (`loads`); channel layouts (`channels`); parameters / changing the audio (`parameters`); latency at
48 kHz reported / measured, all rates in the detail (`latency`); reported latency = measured at all rates (`latencyAgrees`); output before the peak (`outputBeforePeak`, never
bold: it is a property, not a fault); output before the impulse (`ownSignal`); delivery: how many of the four ways work, the most careful one in the detail (`delivery`);
block size independent, the largest steady-state difference in the detail (`blockSizes`); deterministic (`deterministic`); output stays finite (`finite`); recovers
(`recovers`); digital silence in gives silence out, the idle peak in the detail (`silence`).
The same rows are written as JSON next to the report (`<report>.json`: plugin, identifier, date, and the list of items with key, test, result, detail, good). The Developer
page reads it and shows the results as columns, one row per plugin, orange where a result is worth a look, the detail as tooltip.

### 3.0b Channels and buses (R4)
- **Bus table**: every bus of the plugin as it is created (input/output, index, name, default layout or "disabled").
- **Layout table**: whether the plugin accepts these main-bus layouts (other buses switched off if possible): mono, stereo, mono in / stereo out, LCR, quad, 5.1, 7.1,
  ambisonics 1st order (the author's list of the common ones).
- side chain (more than one input bus), MIDI in/out, instrument flag.
- **Channel coupling** (only with two channels, at the setting B): render with noise on L and silence on R, and the other way round; the level of the silent output relative
  to the driven output (dB re the driven channel, and its level in dBFS, "silent" for exact zeros). Below `couplingBelowDb` (-100 dB) in both directions = channels independent.
  Coupling is a property, not a fault (a widener, a cross feed, a stereo reverb couple on purpose): it is never bold in the summary.
Summary keys of R4: `layouts` (the accepted ones), `sideChain` (with the names of the extra input buses), `midi`, `coupling`.

### 3.1 Header
`file`, `format`, `manufacturer`, `version`, `measured` (date), a settings warning if any, `channels: mono yes/no, stereo yes/no`. "no" for both: another layout
(surround, side chain only, instrument): `not measured`. Then a short legend of the differences (2.4).

### 3.2 Findings
Fixed rules, each only when its condition holds, with the numbers:
1. per rate: "nothing came out for the impulse" (no latency measured); "the plugin reports X samples, measured Y" (reported after the audio against measured);
   "the reported latency changed after audio (X after prepare, Y later)"; "output before the impulse arrived (x dBFS): the plugin makes signal of its own".
2. "No parameter changed the audio" (possible reasons named: instrument, analyser, parameters that act only together, parameters not read after prepare).
3. "Delivery '<way>' fails: <reason with the differences>" for each failing way (only if some parameter reacts).
4. "The output depends on the block size also after 1.00 s (largest at block size N: <difference>)".
5. non-determinism, NaN or infinity, "does not come back after parameter jumps", "digital silence in does not give digital silence out (peak x dBFS)".

### 3.3 Parameters
One row per examined parameter: `no.`, `name`, `min`, `default`, `max` (the plugin's text for normalised 0, default, 1), `steps` (**continuous**, **switch** for two
steps, else the number), `automatable`, **`changes (L = R)`**, **`changes (L != R)`** (differences, 2.4; the second only with more than one channel) and **`measured with`**.
A parameter that changes the audio only with L != R acts on the difference of the channels (a width control, a mid/side balance): with L = R there is no side signal.
Sequence (step 2):
1. `baseline` = render(48 kHz, 512, defaults, noise) ("A").
2. **Pass 1**: every parameter at 0.25 and at 0.75 (the others at their defaults); difference against the baseline over the last 8192 samples; the larger counts;
   above -80 dB = changes the audio; "measured with: defaults".
3. Every parameter that reacted in pass 1, **except switches** (two steps or "bypass" in the name), is set to 0.75 → base of pass 2.
4. **Pass 2**: the parameters that did not react are tried again against this base; "measured with: the others at 0.75".
A switch that reacts only in pass 2 (a bypass) changes the audio only because the base of pass 2 has changed the audio: the gain plugin's Bypass reads
-2.5 dB "with the others at 0.75" = exactly the removal of +12 dB (|1 - 3.98| / 3.98 = 0.75).
"no" does not mean useless: a parameter can act on other signals, only together with another one (a band switch and its gain: Venn Audio Free EQ), on MIDI, at other
levels, or on L - R (R4, R6).
The **setting B** (step 3): the defaults; then, in the order of the list, each reacting non-switch parameter is set to 0.75 and kept if the output still differs
from A by more than -60 dB.

### 3.4 Latency at three sample rates
Per rate 44.1, 48, 96 kHz, block 512, a new instance **at its default parameters, without poke or settle**:
`setPlayConfigDetails`, `prepareToPlay` → **reported after prepare** = `getLatencySamples()`; the delayed impulse (2.1) is processed for 4096 + 1 s of samples;
**reported after audio** = `getLatencySamples()` again; **measured** = position of the largest output sample (over all channels) after the impulse, minus the pre-delay;
"nothing came out" if that peak is below 1e-4; **output before the peak** = the largest output between the impulse and the peak, relative to the peak (dB): present for
a linear-phase filter or a look-ahead (the test plugin with a 255-tap FIR at a quarter of the sample rate: -3.9 dB, the tap next to the centre), "none" below -120 dB; **output before the impulse** = the largest output before
the impulse arrived (dBFS): signal of the plugin's own (noise, an oscillator).

### 3.5 Delivery of parameters (A, A, B, A)
The question: if a host sets parameters the way we do, does the plugin do what the parameters say? Four ways, each with four renders A, A, B, A
(A = defaults, B = the setting B), compared over the last 8192 samples of the 12288 noise samples:

| Way | Sequence |
|---|---|
| stream | **one** instance: prepare, settle, apply(A), process noise = A1, process noise = A2, apply(B), **settle**, process = B, apply(A), **settle**, process = A3 |
| fresh, before prepare | four instances: apply(setting) **before** `prepareToPlay`, prepare, settle, process |
| fresh, after prepare | four instances: prepare, apply(setting), settle, process |
| fresh, poked | four instances with render() (poke, then the setting) |

References: refA = A3 of the stream way, refB = B of the stream way (settings reached by a change, settled).
**repeatable** = A2 and A3 the same as A1 (< -80 dB); **reacts** = B differs from A1 (> -60 dB); **as after a change** = A1 and B the same as refA and refB;
result ok if all three. The columns give the differences: "A again (2nd ; 3rd)", "B against A", "A, B against the references (A ; B)".
"Most careful way that works" = the last passing way in the list.
(The settle after the changes in the stream way was added in 0.13.0: without it a plugin that smooths its parameters, for example BL-Gain, was measured while still
ramping, and all ways failed against wrong references.)

### 3.6 Block sizes
Per block size (default 32, 64, 128, 256, 1024, 2048, 509): render(48 kHz, block, B, 1 s of noise) against the same with block 512.
**steady state** = the difference over the last 0.1 s: it decides (below -100 dB = independent); **whole** = over the full second, for information.
A plugin that smooths its parameters once per block differs in "whole" (the ramp has a different shape per block size) but not in the steady state (the test plugin
"Block Smoothing"); a plugin whose processing depends on the block size differs in both (the test plugin "Block Fault": a low-pass whose state is reset in every block).
The poke block is 512 samples for every block size. (Before 0.13.0 it was one block long, so the input history differed between block sizes; that gave the small
differences of the BL gain plugins, which are now identical.)

### 3.7 Other
- **deterministic**: render(B, noise) twice with two instances, bit exact.
- **output stays finite**: no NaN or infinity in the jump test.
- **recovers from parameter jumps**: one instance: prepare, apply(B) (no poke), settle, process noise; each reacting parameter (at most 16, **switches included**) to 0, one
  block of noise, to 1, one block; apply(B), settle, process noise, process noise again: the last output against render(B) < -80 dB.
- **digital silence in gives digital silence out**: prepare, apply(B), settle, 8192 zeros: every sample exactly 0; otherwise the peak in dBFS.

### 3.8 Settings used
The JSON of the settings the report was made with (2.5).

## 4. What a correct result looks like (the plugins of known behavior)
| Test plugin | Expected report |
|---|---|
| Gain | Gain changes the audio ("defaults"), Bypass only "with the others at 0.75"; latency 0 at all rates, no output before the peak; no findings; every way ok |
| Latency (64, reported 64) | latency 64 at all rates, no finding |
| Latency Lie (100, reported 0) | "reports 0 samples, measured 100" at all rates |
| Linear Phase (255-tap FIR, reported 127) | measured 127, reported 127, output before the peak present; no finding |
| Block Smoothing | block size independent (steady state); "whole" differs |
| Block Fault | "depends on the block size also after 1.00 s" |
| Cross Feed (R += L / 2) | coupling L to R -6.0 dB, R to L silent: channels not independent |
| Width (gain acts on L - R) | the gain changes the audio with L != R only |
| Side Chain (stereo side-chain input, on by default) | the side chain is listed and switched off; measured normally |
| EQ Prepare (resets in `prepareToPlay`) | stream: A again differs; before/after prepare: "the first delivery was lost"; only the poked way passes |

## 5. Example: the own PeakEQ and the BL gain plugins
PeakEQ: latency reported 0 (after prepare and after audio), measured 88 / 96 / 192 samples at 44.1 / 48 / 96 kHz (the constructor reports before `prepareToPlay` has
added the synchronous block). Its 44.1 kHz design fault (W5) is no longer in the fingerprint: it is a statement about the frequency response and comes back with the analyzer.
BL-Gain12 and BL-Gain24: nothing unusual; every way ok; all block sizes identical.

## 6. Where the test is still weak (open; the planned step in brackets)
1. **Parameters that act only together** (R6): all bands of an EQ off by default (Venn Audio Free EQ): nothing reacts.
2. **Time-varying plugins** (R6): an LFO, a random element, dither or a noise generator gives "not deterministic", "repeatable: no", "does not come back",
   "not silent". They are not yet labelled "time-varying".
3. **The recovery test moves switches and choices together with the rest** (R6); a mode change that resets the plugin can read "does not come back".
4. **Silence is exact** (R6): any output above zero fails (-149 dBFS of a denormal guard as well); a threshold will come into the settings.
5. **The stream way defines "correct"** (R6): a plugin that ignores parameter changes in the stream way makes all references wrong in the same way.
6. **The setting B is not printed** (R7): "0.75 of the knob" cannot be read in the plugin's units yet.
7. **Latency is the largest peak at the default parameters** (by design): for a plugin with a long tail or a later peak (reverb, delay, a first reflection below a
   later one) it is not a "block latency"; for a plugin that is silent at its defaults (mix 0, a gate) nothing comes out.
8. **Parameters are moved to 0.25 and 0.75 with noise at -20 dBFS** (settings): a threshold at -10 dB, a slow LFO, an attack of seconds or an effect only at 0 or 1 reads "no".
9. **Instruments and plugins without audio input**: "no parameter changed the audio" and "nothing came out" (no MIDI is sent).
10. **Wrapper effects**: the VST3 hosting adds a hidden Bypass parameter to plugins that do not declare one; parameter texts come from the hosting layer.

## 7. Checklist: is this result right?
1. **Does it run at all?** "channels" yes, parameters listed. No report: the plugin crashed in the measurement process.
2. **Latency**: "nothing came out" = not measured (look at the defaults of the plugin). "Output before the impulse" = the plugin makes signal of its own: expect
   determinism and silence to fail too.
3. **Parameters**: read "measured with". A parameter that reacts only "with the others at 0.75" depends on them.
4. **Delivery**: read the difference columns. "A again" large and "references" identical = the state depends on the history of the parameters; references far
   from A1 = the first delivery is lost.
5. **Block sizes**: only "whole" differs = smoothing per block (fine); "steady state" differs = real dependence.
6. **Run it twice**: two reports must agree except the date (unless the plugin is time-varying).
7. **Compare with the test plugins** (section 4): a finding that appears for a correct test plugin in the same situation is a defect of the test.
