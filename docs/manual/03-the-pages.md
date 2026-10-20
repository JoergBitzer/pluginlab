# The pages

| Page | Application | Plugin | What it is for |
|---|---|---|---|
| Easy | yes (last tab) | yes (first tab) | the musician's walkthrough: compare, match, listen (`02-first-steps.md`) |
| Plugins | yes | yes | find, validate and load plugins; their parameters; their editors |
| EQ Compare | yes | yes | listen and switch between the slots (dry and up to four plugins, gapless); the live transfer functions of all slots while you turn knobs, up to Nyquist, against a reference slot |
| Matcher | yes | yes | the matcher with every option: master (a plugin or a band curve), the released knobs per target, tolerance (exact 0.01 dB, strict 0.1, medium 0.25, loose 0.5), frequency range, time per target, start from the model; live deviation; report |
| Expert | yes | yes | the measurement units on chosen slots (system class first, gain, frequency response, delay and polarity, phase, THD and THD+N, intermodulation, noise, crosstalk, maximum level, linearity) and the null test against a master; results side by side; measurement presets |
| Developer | yes | no | for plugin developers: the fingerprint (how a plugin behaves as a plugin: parameters, latency, delivery, block sizes, real time, zipper noise, system class), the GUI review, the parameter survey and mapping, calibration and interactions, reports |

## The live curves (EQ Compare)
Each slot is measured by a copy of its plugin in a separate process (the playing plugin is not touched): when you turn a knob, its curve
follows within a fraction of a second, at most five times per second per plugin. A slot that is not linear and time-invariant at its setting
(a compressor, a saturating EQ, a modulated effect) is drawn grey with the reason: its "curve" is not a transfer function.

## Measurement presets (Expert)
A preset stores which units run, whether the dry slot is measured and which slot is the null-test master (by role: the first plugin, the
dry slot). Shipped presets for teaching: "Is it LTI?", "Full AES17 set", "Null test of two EQs", "Orfanidis against RBJ". Presets are shared by
the application and the plugin.

## Crash protection
Everything that measures runs in a process of its own: scanning, the jobs of the Developer page, the matcher, the Expert measurements, the
live curves. A plugin that crashes there is reported, and the product goes on. Listening runs inside the application or the DAW: a plugin
that crashes while it plays takes it down; the application then offers the last session again at the next start and names what did not load.
