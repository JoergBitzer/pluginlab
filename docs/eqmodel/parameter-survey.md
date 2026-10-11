# Parameter survey

Unit of W8.4. Code: `src/engine/ParameterSurvey.cpp` (`surveyParameters`,
`makePluginSubject`, `createSurveyReport`); tests
`tests/ParameterSurveyTests.cpp` (one CTest per plugin,
`PluginLabParameterSurvey_*`). Plan: `docs/design/W8-plan.md`, W8.4.

## Purpose
Before a mapping can say "parameter 3 is the frequency of band 2" (W8.5), we
have to know what each parameter does. The survey moves every parameter alone
over its range and measures what changes. It also finds parameters that act
only together with another one (a band gain that does nothing while the band
is off), and it reports them as coupled instead of guessing. The texts the
plugin shows for each value are recorded for the knob laws of W8.5.

## What one point measures
One render per setting, through the careful delivery of the plugin device
(W7.11: fresh instance, every parameter poked, the setting, settled):

| Part | What | Used for |
|---|---|---|
| 0.5 s silence | rms of the second half, both channels; counts only if stationary (second half within 3 dB of the first) | idle output: noise, hum, DC |
| synchronized sweep, -12 dBFS, 1 s, left input only | left -> left: the direct path H(f), 12 points per octave, 20 Hz ... 20 kHz | level, bands (W8.3 fit), polarity |
| | left -> right: the cross path | stereo (crosstalk, width) |
| | peak of the impulse response | latency |
| | largest H_n / H_1, n = 2 ... 5, 50 Hz ... 0.9 Nyquist / 5, pass band only (H_1 within 60 dB of its maximum) | distortion (with the class) |

The system class (W8.1, long sweeps at three levels) is measured at both ends
of each parameter that acts, and once at the default setting.

## Routine
| Step | What |
|---|---|
| 1 | the default setting: one point and its system class |
| 2 | each parameter: a probe at two values (continuous: 0.05 and 0.95 of the normalised range; discrete: every step up to 4, else first, middle, last). If nothing changes, step 3, else step 4 |
| 3 | the search for a context in which it acts: contexts in which other parameters acted (found earlier); one other parameter moved (continuous ones first: 0.95, 0.05 and an irregular 0.3713, which gives fractional values such as a fractional delay; then every step of discrete ones); a found context with one more parameter; any two other parameters (discrete pairs first; the ends of continuous ones). At most 200 renders per parameter. Found: coupled, surveyed there. Not found: "does nothing" |
| 4 | the survey: continuous parameters at 5 points (0.05 ... 0.95: the very ends are often outside what a band model describes), discrete ones at every step (up to 16) |
| 5 | the role (below) |
| 6 | a switch (discrete parameter) is surveyed once more in the context that made most parameters act (e.g. the design switch of an EQ with a band that has gain) |

## Roles
Decided in this order:

| Role | Evidence |
|---|---|
| does nothing | no change in any measurement, in any context tried |
| idle output (noise, hum, DC) | the stationary idle output changes by more than 3 dB (above -140 dBFS). First, because added noise or hum also makes two sweeps differ, which the class test calls time-variant |
| modulation (time-variant) | the system class at an end is time-variant |
| distortion | the harmonics change by more than 1 dB (above -100 dB) **and** the class at an end is non-linear |
| stereo (cross path) | the cross path changes by more than 0.01 dB (above -100 dB) |
| polarity | the magnitude is unchanged, H_k = -H_0 |
| latency | the magnitude is unchanged, the delay changes |
| level | the direct path changes by the same dB at every frequency (within 0.02 dB) |
| band frequency / gain / width (Q) / order | the W8.3 fit of every point (up to 3 bands); the band quantity that changes most (frequency 0.02 octave, gain 0.05 dB, Q 3 %); the others are listed as "also" (a gain knob with proportional Q) |
| switch | a discrete parameter that changes the structure: the band types, the design family (pass filters of RBJ, Butterworth and Linkwitz-Riley count as one family: RBJ Q 0.71 = Butterworth 2), a band on and off, a curve, the noise |
| other | changes the response, but no band quantity explains it (or the bands cannot describe it) |

A discrete parameter with one pure effect (a polarity switch, a stepped
frequency selector) gets that effect's role.

## Results for the reference and test plugins
From `ParameterSurveyTests` (2026-10-11, 0.46.0), 48 kHz, Debug build. Every
parameter is classified as the code of the plugin says:

| Plugin | Parameter | Role | Coupled (acts only when) |
|---|---|---|---|
| Test EQ Drive | Gain | band gain | - |
| | Frequency | band frequency | Gain = 21.6 |
| | Q | band width (Q) | Gain = 21.6 |
| | Drive | distortion, class LTI -> memoryless / other | - |
| | Output | level | - |
| | Display | does nothing | - |
| | Bypass | switch (band on/off) | Gain = 21.6 |
| Test EQ Switch | Gain | band gain | Enabled = On |
| | Frequency, Q | band frequency, band width | Enabled = On, Gain = 21.6 |
| | Enabled | switch (band on/off) | Gain = 21.6 |
| Reference EQ | Algorithm | switch (design family; latency of the linear-phase design) | - |
| | Type | switch | - |
| | Frequency | band frequency | Gain = 21.6 dB |
| | Gain | band gain | - |
| | Q | band width (Q) | Gain = 21.6 dB |
| | Order | band order | Algorithm = Butterworth, Type = Low-pass |
| Reference Utility | Gain | level | - |
| | Polarity | polarity | - |
| | Delay | latency | - |
| | Interpolation | switch (Thiran flat, Lagrange falls) | Delay = 1782.24 samples |
| | Width, Crosstalk | stereo (cross path) | - |
| | DC offset | idle output | - |
| | Tremolo rate | modulation | Tremolo depth = 0.950 |
| | Tremolo depth | modulation, class LTI -> time-variant | - |
| | Bypass | latency | Delay = 1782.24 samples |
| Reference Nonlinear | Curve | switch, class LTI -> memoryless | - |
| | Drive, Output | level (with the curve off) | - |
| | a2, a3 | distortion | Curve = Polynomial |
| | Threshold | distortion | Curve = Hard clip |
| | Bits | distortion | Quantizer = On |
| | Quantizer, Noise, Hum | switch | - |
| | Noise level, Hum level | idle output | Noise = White, Hum = 50 Hz |
| | Bypass | switch | Curve = Polynomial |

"Bypass" is the parameter that JUCE's VST3 wrapper adds. It acts only where
the plugin does something; with the Utility's delay it is honestly "latency".

Time per plugin (Debug, one process): Test EQ Switch 83 s, Reference Utility
164 s, Test EQ Drive 218 s, Reference Nonlinear 337 s, Reference EQ 544 s
(190 renders 171 s, system class 192 s, band fits 181 s). With
`tools/ctest_offscreen.sh -j10` the five run side by side.

**What the survey taught** (corrections on the way)
- A band at 21 Hz with +21.6 dB rings for a long time after the setting
  arrives (17 dB per 100 ms; still -66 dBFS after 0.25 s). That is not idle
  output: the idle output counts only if it is stationary.
- The harmonics of a short sweep are not proof of a non-linearity (a strong
  low-frequency boost lifts them to -72 dB); the system class decides.
- W8.1 divided harmonics by H_1 deep in a stop band (an 8th-order low-pass is
  -96 dB at 4 kHz): noise. Fixed in `classifySystem` (frequencies more than
  60 dB below the maximum of H_1 are left out; a test case added).
- W8.3 knew Butterworth orders 1 ... 4 only, and fitted the noise floor of a
  steep low-pass with extra bands. Fixed: the order is estimated from the
  level one octave beyond the -3 dB point (6.02 dB per order) and tried with
  its neighbours (1 ... 8); below 90 dB under the largest level the residual
  is clamped (a test case for order 7 added).
- A fit may add a small band at the edge of the band (a shelf at 10 Hz next to
  a peak at 28 Hz); for a continuous parameter the dominant band decides and
  the small bands are listed.

## Limits
- A parameter that acts at the default setting is surveyed there (and a
  switch once more in the most telling context). That it does something else
  in another context (a drive that only distorts with a curve on) is the
  interaction question of W8.7.
- The search tries at most two other parameters at a time and a few values of
  each; a parameter that needs three is reported as "does nothing".
- 48 kHz in the tests; the plan asks for 44.1, 48 and 96 kHz in the
  calibration (W8.6), where the sample-rate faults matter.
- Channel 1 only for the class; a mono plugin has no cross path.

## Implementation
```cpp
pluginlab::engine::SurveySubject subject = pluginlab::engine::makePluginSubject(formats, description, 48000.0);
pluginlab::engine::SurveyResult result = pluginlab::engine::surveyParameters(subject, pluginlab::engine::SurveySettings{});
// result.parameters[i].role, .coupled, .inContext->contextText, .atDefault.points[k].text / .fit
juce::String markdown = pluginlab::engine::createSurveyReport(result, "My EQ");
```
A `SurveySubject` is any set of parameters with a device per setting, so the
survey also runs on things other than hosted plugins.
