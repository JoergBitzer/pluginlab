# The fingerprint report: what is measured, how, and how to read it

Version 0.12.0 (2026-10-05). Code: `src/engine/Fingerprint.cpp` (all numbers below are the constants in the top of that file). Report text: `createReport()` in the same file.
Purpose of this document: say exactly what every line of the report means, so that a result can be judged as "the plugin does this" or "our test does this",
and list the places where the test itself is weak (section 6). Section 7 is a short checklist for a surprising result.

## 1. Overview: what happens when a report is made

The report is made by `PluginLabHost --fingerprint <plugin file> <report> [<plugin id>]` (the Developer page starts it in a process of its own). The program scans the file in
its own process, takes the plugin (the one with the identifier, or every plugin of the file), and runs the following steps, always in this order. Every **render** (see 2.3)
loads a **new instance** of the plugin; a report needs about 45 instances plus two per parameter (and two more for every parameter that did not react in the first pass).

| Step | What | Result in the report |
|---|---|---|
| 0 | load one instance at 48 kHz / 512; choose the channel layout | the report is `not measured` if this fails |
| 1 | channel layouts and parameter list (texts, steps) from that instance | "channels", table "Parameters" |
| 2 | which parameters change the audio (noise through the plugin, 2 passes) | column "changes the audio (dB)" |
| 3 | build the setting B (the parameters that matter moved to 0.75) | (used by everything below, not printed) |
| 4 | per sample rate 44.1 / 48 / 96 kHz: latency (impulse), response at setting B (impulse) | table "Sample rates", table "Response" |
| 5 | A, A, B, A test in four ways of delivering parameters | table "Delivery" |
| 6 | block sizes 32 ... 2048 and 509 against 512 | table "Block sizes" |
| 7 | determinism, parameter jumps with recovery, silence in/out | block "Other" |
| 8 | the "Findings" at the top are derived from steps 2 to 7 by fixed rules | list "Findings" |

Nothing in the measurement knows what the plugin is (EQ, compressor, delay, ...). It only runs noise, an impulse and silence through it and moves parameters.

## 2. The ingredients

### 2.1 Test signals
| Signal | Exact definition |
|---|---|
| **Noise** | `juce::Random` with seed 7, uniform white noise in [-0.1, +0.1] (peak -20 dBFS, RMS 0.0577 = -24.8 dBFS). 12288 samples (`kNoiseLength`). The same samples on **every channel** of the plugin (L = R). The very same sequence is used in every render, so two renders are comparable sample by sample. |
| **Impulse** | 16384 samples (`1 << 14`): sample 0 is 1.0 (0 dBFS), all others 0. Also on every channel. |
| **Silence** | zeros. |
| **Poke block** | the first `blockSize` samples of the noise (one block), see 2.3. |

### 2.2 The channel layout
`ChannelAdapter::chooseLayout(instance, 2)`: tries stereo, then mono. The plugin runs with that layout (the report says "channels: mono yes/no, stereo yes/no" from
`checkBusesLayoutSupported`). Only **output channel 0** is analysed. No MIDI is sent.

### 2.3 The building blocks (`Rig`, `Bench`)
- **make(rate, block)**: `createPluginInstance(description, rate, block)`, then the layout above.
- **prepare()**: `setPlayConfigDetails(ch, ch, rate, block)` and `prepareToPlay(rate, block)`.
- **apply(setting)**: `setValue(v)` of **every** parameter of the list (at most the first 64), `v` = normalised value 0 ... 1. A setting is one such vector. `defaults` = `getDefaultValue()` of each.
- **process(signal)**: the signal in blocks of `block` samples (the last block is shorter), output channel 0 is collected. Remembers if any output sample was NaN or infinite.
- **settle()**: **0.25 s of zeros** (12000 / 11025 / 24000 samples at 48 / 44.1 / 96 kHz): smoothers and filters of the plugin settle. (A fixed number of samples was wrong: see the history in `W5-fingerprint.md`.)
- **render(rate, block, setting, input)** = the careful way that every measurement in steps 2 to 7 uses:
  1. make (new instance), prepare
  2. `apply(poke values)`: every parameter is first set to another value: `v - 0.4` if `v > 0.5`, else `v + 0.4`
  3. process one block of noise (so the plugin sees the changes)
  4. `apply(setting)`
  5. settle (0.25 s of zeros)
  6. process(input); output = channel 0
  The poke exists because some plugins read a parameter only when its value changes (the PeakEqualizer template): without it a setting that equals what the plugin saw in `prepareToPlay` is lost.
- **renderAfterPrepare**: the same without steps 2 and 3 (used for one of the four delivery ways).

### 2.4 The difference in dB
`differenceDb(a, b, from)` = 20 log10( RMS(a - b) / RMS(b) ) over the samples from `from` to the end; RMS(b) is floored at 1e-12, RMS(a - b) at 1e-24, result floored at -200 dB.
- -200 dB means the signals are identical (bit exact).
- It is **relative to the level of the second signal**, not an absolute level. Examples: output scaled by 0.25 against the original: 20 log10(0.75) = **-2.5 dB**; scaled by +12 dB (factor 3.98): 20 log10(2.98) = **+9.5 dB**. A positive value means the difference is larger than the signal.
- The "last 8192 samples" window (`kCompareLength`) is used for the noise comparisons (the first 4096 of the 12288 are skipped so that filter transients are over). Exception: block sizes (whole signal).

### 2.5 Decision thresholds (all in `Fingerprint.cpp`)
| Constant | Value | Used for |
|---|---|---|
| `kReactsAboveDb` | -80 dB | a parameter "changes the audio" |
| `kDifferentAboveDb` | -60 dB | B differs from A; a candidate parameter is kept in B |
| `kSameBelowDb` | -80 dB | two outputs count as the same (repeatable, as after a change, recovers) |
| `kBlockIndependentBelowDb` | -100 dB | block size independent |
| `kFeatureMinimumDb` | 0.5 dB | the response has a "feature" |
| `kRateFollowRatio` | 1.5 | the feature moved by more than this factor between 44.1 and 96 kHz: "follows the sample rate" |
| `kRateDifferentDb` | 1.0 dB | the responses differ between the rates |
| determinism | <= -199 dB | exactly equal |

## 3. The report, section by section

### 3.1 Header
`file`, `format`, `manufacturer`, `version`: from the plugin description. `channels: mono yes/no, stereo yes/no`: `checkBusesLayoutSupported` for a 1-in/1-out and a 2-in/2-out layout.
"no" for both means the plugin has another layout (surround, side chain only, instrument without input): the report is then `not measured`.

### 3.2 Parameters
One row per parameter (at most 64): `no.` (position in the host's list), `name`, `min`, `default`, `max` (the plugin's own text for normalised 0, default, 1, `getText(v, 32)`),
`steps` (`getNumSteps()`; **2147483647 means continuous**, 2 is a switch), `automatable`, and **`changes the audio (dB)`**:

*Exact sequence (step 2):*
1. `baseline` = render(48 kHz, 512, defaults, noise) (this is "A")
2. **Pass 1:** for every parameter p and for the values 0.25 and 0.75: `setting` = defaults with p replaced; `output` = render(...setting...); `change` = differenceDb(output, baseline) over the last 8192 samples. The number in the table is the **larger** of the two. If it is above -80 dB the parameter "changes the audio".
3. All parameters that changed the audio in pass 1 **except switches** (2 steps, or "bypass" in the name) are set to 0.75 → `withReacting`.
4. **Pass 2:** the parameters that did **not** react in pass 1 are tried again the same way, but against `withReacting` as base and reference (the frequency of an EQ does nothing while its gain is 0 dB).

*How to read the number:* it is the size of the change relative to the output level (see 2.4), **in the context in which the parameter reacted**:
- a parameter that reacted in pass 1 (for example Gain): the change against the plugin at its defaults;
- Reading the number: **0 dB** = the change is as large as the signal itself (ShapeIt "Gain In": +6 dB gives a difference of exactly 1.0 x the level, printed -0.0); **+6 dB** = twice the signal (a polarity inversion: output = -input, difference = 2 x input; ShapeIt "Phase invert L" reads 6.0); **-2.5 dB** = 0.75 x the signal (a level change to 0.25 x or 1.75 x); **-40 dB** = a change of 1 % of the level.
- a parameter that reacted only in pass 2 (for example Frequency, Q of an EQ, or **Bypass**): the change against the plugin with the other parameters at 0.75. A bypass reads "changes the audio" **only because in pass 2 the gain is +12 dB and bypass removes it**. That is correct but it is not obvious from the table (see 6.9).
- `no`: below -80 dB in both passes. That does **not** mean the parameter is useless: it can act on other signals (MIDI, higher levels, other channels, other time scales), see 6.

### 3.3 Sample rates
Per rate 44.1, 48 and 96 kHz (block 512) two separate measurements:

**Latency (step 4a).** New instance, `measureLatency`: `setPlayConfigDetails`, `prepareToPlay(rate, 512)`, **`reported` = `getLatencySamples()` read right after `prepareToPlay`**; then 1 s of processing in blocks of 512 with an impulse (1.0 on all channels) in sample 0 and zeros after it; **`measured` = the position of the largest absolute output sample** (all channels). It runs with the plugin's **default parameters, without poke or settle**.

**Response (step 4b).** `render(rate, 512, B, impulse)`: output channel 0 (16384 samples) → FFT (size 16384, rectangular window) → magnitude of bins 0 ... 8192 → on a **log axis of 128 points from 100 Hz to 19800 Hz**, the magnitude is linearly interpolated between the bins and converted to dB (20 log10, no normalisation: the impulse has magnitude 1 at every frequency). That gives the response on the same frequency axis for all three rates (the printed table shows every 10th point).
- **feature frequency / feature gain**: the axis point with the largest |dB| and its value; "none" if that value is below 0.5 dB.
- **the setting B** (step 3, not printed): start with the defaults; go through the parameters that change the audio **in the order of the list**, skipping switches; for each: set it to 0.75, render noise, and keep it if the result differs from A by more than -60 dB (it still does something). So for an EQ B is "gain +12 dB, frequency at 0.75 of its knob, Q at 0.75 of its knob" **in normalised units**, not in Hz.
- Below the table: **largest difference of the response between the rates**: the largest |dB(rate 1) - dB(rate 2)| over all axis points and over the three pairs.
- **Feature frequency at the highest rate over the lowest** and the ratio of the rates (2.177) are printed only if both rates have a feature.

### 3.4 Response at the setting B
The response (dB) on the common axis for the three rates, every 10th point. Use it to see what the "feature" really is: a bell (a bump around one frequency), a shelf, a flat gain (all rows the same number), a filter slope.

### 3.5 Delivery of parameters (A, A, B, A)
The question: if a host sets parameters the way we do, does the plugin do what the parameters say? Every row is one **way of delivering**, and four renders in that way, each with a new instance except the first way:

| Way | Sequence |
|---|---|
| stream | **one** instance: prepare, settle, `apply(defaults)`, process noise = A1, process noise = A2, `apply(B)`, process noise = B, `apply(defaults)`, process noise = A3 |
| fresh, before prepare | four instances, in each: `apply(setting)` **before** `prepareToPlay`, prepare, settle, process noise (A, A, B, A) |
| fresh, after prepare | four instances: prepare, `apply(setting)`, settle, process (A, A, B, A) |
| fresh, poked | four instances with `render()` (poke, then the setting) |

All use the same 12288 noise samples; the last 8192 samples are compared. Reference outputs: **refA = A3 of the stream way** (defaults reached by a change), **refB = B of the stream way**.
- **repeatable**: A2 and A3 differ from A1 by less than -80 dB. (In the stream way A2/A3 follow A1 on the same instance, so the filter state at the start of A2 is the end state of A1; transients are skipped by the 4096-sample offset.)
- **reacts**: B differs from A1 by more than -60 dB.
- **as after a change**: A1 and B equal refA and refB (< -80 dB). This catches a plugin that ignores a setting equal to what it saw in `prepareToPlay` (A1 is the hard-coded response, refA the real one).
- result **ok** if all three, else **FAILS**. The last column shows the numbers: "A again x / y dB, B against A z dB, A and B against the references u / v dB".
- "Most careful way that works" = the last passing row in the list (poked is the last).

### 3.6 Block sizes
`render(48 kHz, block, B, 16384 samples of noise)` for block sizes 32, 64, 128, 256, 1024, 2048 and 509 (a prime), each against block 512, `differenceDb` over the **whole** 16384 samples. Every value must be below -100 dB for "block size independent". (`prepareToPlay` is called with that block size as the maximum; the poke block is `blockSize` samples long.)

### 3.7 Other
- **deterministic**: render(48 kHz, 512, B, noise) twice with two instances; the outputs must be equal (<= -199 dB, i.e. bit exact).
- **output stays finite**: no NaN or infinity in any output sample of the jump test (below).
- **recovers from parameter jumps**: one instance: prepare, `apply(B)` (no poke), settle, process noise; then for each parameter that changes the audio (at most 16, **switches included**): set it to 0, process one block of noise, set it to 1, process one block; then `apply(B)`, settle, process noise, and process noise once more; that last output must equal render(B) within -80 dB (last 8192 samples).
- **digital silence in gives digital silence out**: one instance: prepare, `apply(B)` (no poke), settle, then 8192 zeros; yes only if every output sample is exactly 0. If not, the peak is printed (dBFS) as the idle level.

### 3.8 Findings
Fixed rules, in this order, each appears only if its condition holds:
1. **Latency**: for each rate with reported != measured: "Latency at ... the plugin reports X samples, measured Y samples".
2. **No parameter changed the audio** (list of reacting parameters empty).
3. **The response moves with the sample rate**: both rates 44.1 and 96 kHz have a feature and `feature(96 kHz) / feature(44.1 kHz) > 1.5`. (`rateRatio` = 2.177.)
4. otherwise, if the largest difference between the rates is above 1.0 dB: "The response differs by up to x dB between the sample rates".
5. **Delivery 'way' fails: reason** for each way that failed (only if some parameter reacts).
6. block size dependence, non-determinism, NaN/inf, "does not come back after parameter jumps", "digital silence in does not give silence out".

## 4. What a correct result looks like (the plugins of known behavior in the tests)
- **PluginLabTestGain** (gain plugin; Gain is the only parameter that matters): Gain changes the audio; B = gain +12 dB, a **flat** response; no findings; every delivery way ok.
- **PluginLabTestEq** (correct RBJ peaking EQ): feature at 7598.6 Hz at all three rates, gain 11.4 ... 11.6 dB (a bell at the setting B = gain +12 dB, frequency at 0.75 of the knob, Q at 0.75 of the knob, about 7.6); feature ratio 1.000; largest difference between the rates 0.9 dB (the bell is narrow: see 6.5); no findings.
- **PluginLabTestEqFs** (the 44.1 kHz design): 7598.6 / 8258.5 / 16762.1 Hz: ratio 2.206: finding "the response moves with the sample rate".
- **PluginLabTestEqPrepare** (resets in `prepareToPlay`): stream: A again differs; before/after prepare: "the first delivery was lost"; only the poked way passes.
- **PluginLabTestLatencyLie**: reports 0, measured 100 at all rates: finding "Latency ... reports 0 samples, measured 100".

## 5. Worked example: the own PeakEQ
At 44.1 / 48 / 96 kHz the feature is at 3304 / 3591 / 7289 Hz (factors 1.087 and 2.206), while the rates differ by 1.088 and 2.177. The correct EQ with the same procedure shows 7599 Hz at all rates. Cause in the source: `PeakEQAudio::m_fs` stays at 44100
(`docs/design/W5-fingerprint.md`). The same report shows latency reported 0, measured 88 / 96 / 192 (the constructor reports before `prepareToPlay` has added the synchronous block size).

## 6. Where the test itself is weak: results that may come from the test, not from the plugin
Every item says what you would see, why, and how to check it. These are the candidates for the strange results on simple plugins.

1. **A flat response gets a "feature".** For a pure gain (flat +12 dB at every frequency) the "feature" is the axis point with the largest |dB|. All 128 points are equal up to rounding (12.000001 versus 11.999999), so the point is arbitrary (often the first one, 100 Hz; ShapeIt showed "feature 100 Hz, 12.00 dB" at all three rates while its table "Response" has 12.00 dB in every row: a flat gain, no feature at all). Finding 3 compares the feature frequencies at 44.1 and 96 kHz: for a flat response these are **random points** and the ratio can exceed 1.5 by chance, giving a **false "moves with the sample rate"**. A feature should only count if the response varies across the axis (maximum minus minimum above 0.5 dB). *Check:* look at the table "Response": identical numbers in every row = flat gain, ignore the "feature" columns.
2. **Latency "measured 0" can mean "nothing came out".** `measureLatency` has a flag `found` (peak below 1e-4) but the report does not print it. A plugin that is silent for the impulse at its default parameters (a mix at 0, a gate, a compressor that ducks the impulse, an instrument) shows `measured 0`. *Check:* a measured latency of 0 together with "no parameter changed the audio" is not a measurement.
3. **Reported latency is read once, right after `prepareToPlay`, before any audio.** A plugin that sets its latency at the first `processBlock` or via an asynchronous latency-changed message shows reported 0. (The own PeakEQ does report 0 for real: constructor only.) *Check:* compare with the latency the Compare page shows after the plugin has run.
4. **The latency is the largest peak, measured at the default parameters, with a 0 dBFS impulse.** For a plugin with a long tail (reverb, delay) or a first reflection below the later peak, the position of the largest sample is not "the latency". For an EQ with a peak that is not at the start (high-pass, band-pass, linear phase) it is the filter's delay, which is what is wanted for alignment, but it is not a block latency. A polarity inversion does not matter (absolute value).
5. **"Response differs between the rates" is sensitive for narrow bells.** The bell of the setting B has a Q of about 7.6. A bilinear design has the bell slightly warped differently at 44.1 and 96 kHz; with a high Q this gives a dB difference of about 1 dB on the skirts although the design is correct (the correct test EQ shows 0.9 dB). The 1.0 dB threshold is borderline for such a setting. *Check:* compare the "Response" rows near the feature.
6. **The setting B is in normalised units.** "Frequency at 0.75 of the knob" is a different frequency in Hz for each plugin, and for a plugin whose knob range depends on the sample rate (range up to Nyquist) it is a different frequency at each rate. Then "the response differs between the rates" says something about the knob, not about the filter. The ZeroEQ (gain 335 dB feature) and EQoder (13.6 kHz / 7.3 kHz / 212 Hz) results of the first runs are suspect for this reason. The comparison at equal frequencies in Hz needs the mapping per plugin (W8).
7. **The impulse response is the response to a 0 dBFS impulse and is cut at 16384 samples.** (a) For a nonlinear plugin (compressor, distortion, saturation, anything with a threshold) this is not the small-signal frequency response: the impulse is far above any threshold. (b) 16384 samples are 0.37 s at 44.1 kHz and 0.17 s at 96 kHz: a long tail is truncated differently at the three rates, and the frequency resolution is 2.7 Hz and 5.9 Hz. (c) A plugin with a latency above 16384 samples would put the impulse response outside the window.
8. **Noise is the same on L and R.** A stereo effect that acts on the difference (a width control, a mid/side balance, a stereo delay with cross feedback) shows "no" for its parameter, because with L = R there is no side signal. Only output channel 0 is analysed, so a pan or balance control shows a change only through the gain of channel 0. *Check:* run a stereo plugin through the Compare page with a stereo file.
9. **The number in "changes the audio (dB)" mixes two contexts.** The table does not say whether it was measured at the defaults (pass 1) or with the other parameters at 0.75 (pass 2). The Bypass of the gain plugin reads -2.5 dB = 20 log10(0.75), exactly the effect of removing the +12 dB of the pass-2 base (|1 - 3.98| / 3.98 = 0.75); the Bypass of the EQ (-7.2 dB) is the same kind of effect (bypassing the +12 dB bell). Neither is a bypass that changes the audio at the defaults: the level of both test plugins with bypass at 0, 0.25, 0.5, 0.75 and 1 is the input level to four digits (checked separately, ratio 1.0000). The same -2.5 dB is printed for the Bypass of ShapeIt. Also: "no" does not mean "does nothing": see item 8, items 11 and 12.
10. **Continuous parameters show `steps` 2147483647** (JUCE's "infinite"). It only means continuous.
11. **Parameters are only moved to 0.25 and 0.75 with noise at -20 dBFS.** A parameter that acts only at higher levels (a threshold at -10 dB), on MIDI, on a time scale longer than the 4096-sample comparison offset (a very slow LFO, an attack of seconds) or only at extreme values (0 / 1) can read "no". Only the first 64 parameters are examined.
12. **Time-varying plugins fail several tests by design.** A chorus, a phaser or any plugin with a free-running LFO, a random element or dither gives: not deterministic, "repeatable: no", "does not come back after jumps", "silence in does not give silence out". The test assumes that the same input and the same parameters give the same output after the settling time.
13. **Poked delivery is the reference for everything, but the stream way defines "correct".** refA and refB come from the stream way (the same instance, parameters changed after prepare). A plugin that already ignores parameter changes in the stream way makes all references wrong in the same way, and the A, A, B, A test can pass although the plugin ignores parameters.
14. **Block size test: the poke block has the length of the block.** The input history before the measured signal differs between block sizes (the poke block is `blockSize` samples), and the comparison runs over the whole 16384 samples including the first samples. A plugin with a memory longer than the 0.25 s settle time (reverb, long delay) can show differences that are not block size dependence. Latency compensation inside the plugin that depends on the block size shows up in the first samples as well.
15. **Recovery test moves switches too.** Bypass, on/off and choice parameters (also 0 and 1 of a "mode" choice) are jumped like the others; a plugin that changes its state on a mode change (a reset, a different algorithm with a different latency) can show "does not come back".
16. **Silence test is exact.** Any output above zero (denormal guard noise, dither, DC offset of a filter, an LFO) is "not silent"; the peak is printed. -149 dBFS (Multi-Q) is not audible but fails the exact test.
17. **Instruments and plugins without audio input** give "no parameter changed the audio" and latency 0 (nothing comes out), because no MIDI is sent.
18. **Wrapper effects.** The VST3 hosting adds a hidden Bypass parameter to plugins that do not declare their own; it appears in the list (and in the "reacting" parameters through item 9). Parameter texts come from the hosting layer (`getText`); some VST2 plugins return an empty text.

## 7. Checklist: is this result right?
1. **Does the plugin run at all?** "channels" yes, parameters listed, `loaded`. If the report is missing, the plugin crashed in the measurement process.
2. **Look at the Response table first.** Same number in every row: flat gain; ignore feature, ratio and rate findings. A bell: check that the feature is at the same frequency at the three rates; if not, check item 6 (knob in normalised units) before blaming the filter.
3. **Latency**: measured 0 and "no parameter changed the audio" = nothing was measured. Otherwise compare with the latency shown on the Compare page.
4. **Delivery**: read the last column of every way. `A again` large and `A and B against the references` -200 means the plugin's state depends on the history of the parameters; the first delivery being lost shows as the references being far from A1.
5. **Block sizes**: a single block size with a large value (the prime 509, or 32) points to a plugin that processes in internal blocks and has a latency or a smoothing step that depends on the block; all values large points to a time-varying plugin.
6. **Run it twice.** Two reports of the same plugin must agree in everything but the date (the measurement is deterministic apart from plugins that are not). If they do not, the plugin is time-varying (item 12).
7. **Reproduce a doubtful finding by hand.** The Compare page plays a file through the plugin; the `--fingerprint` command line produces the same report; `tests/FingerprintTests.cpp` shows the expected result for plugins with known behavior (section 4). A finding that the test plugins do not reproduce for a correct plugin is probably real; one that appears for a correct test plugin in the same situation is a defect of the test.

## 8. Changes that would make the report easier to read (proposals, not done)
1. Print `found` with the latency ("measured 0 (nothing came out)").
2. Give a feature only if the response varies (maximum - minimum >= 0.5 dB) and say "flat, +12.0 dB" otherwise; never compare feature frequencies of a flat response.
3. Name the context in the column "changes the audio": "at defaults" or "with the others at 0.75"; show "continuous" instead of 2147483647.
4. Print the setting B (the parameter names, their normalised values and their texts) at the top of "Sample rates" so that "feature at 0.75 of the knob" can be read in Hz of the plugin's own text.
5. Compare the three rates at the same frequency in Hz when the plugin's text gives the frequency (from the texts of the parameters), otherwise say that the setting is in normalised units.
6. Add a short sentence under every table that says what it measures (the text of section 3), and a legend of the dB difference.
7. A second noise run with different channel content (L != R) for stereo plugins and a 0 dBFS / -40 dBFS pair of levels for the response of nonlinear plugins.
8. Show the numbers behind a finding in the Findings list itself (they are in the tables, but not next to the sentence).
