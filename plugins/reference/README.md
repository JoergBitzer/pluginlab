# PluginLab reference plugins (W6.4)

Three plugins whose answer is known exactly: the processors of `src/reference/` (`pluginlab_reference`, tested in
`tests/ReferenceTests.cpp` and `tests/NonlinearReferenceTests.cpp`) wrapped as VST3 (and Standalone). They are test objects for the
host, the fingerprint and the measurements, and teaching objects ("this is what an ideal peak filter does").

| Plugin | Chain | Known answer |
|---|---|---|
| **PluginLab Reference EQ** | one band: algorithm x type | H(e^jw) of the design (the GUI draws it at 48 kHz) |
| **PluginLab Reference Nonlinear** | drive -> curve -> output -> hum -> noise -> quantizer | harmonics of a sine (closed form or numerical), SNR 6.02 N + 1.76 dB, noise and hum levels |
| **PluginLab Reference Utility** | gain/polarity -> delay -> channel matrix -> tremolo -> DC | all exact |
| **PluginLab Reference Delay** (W11.1, 0.74.0) | delay line -> loop (feedback, low and high cut) -> mix | every echo: time, gain, the loop filter (`reference::FeedbackDelay`) |

All three: latency 0 (no rebuffering; the Reference EQ with the linear-phase algorithm: the FIR's latency), no parameter smoothing (a change acts at the next block: the reference is the exact algorithm, a
click is part of the truth), mono or stereo (in = out). A new setting rebuilds the stage it belongs to (noise, hum, dither and tremolo
start again; a new delay starts empty); this allocates memory on the audio thread at a parameter change, a compromise accepted for
reference plugins (no allocation while the parameters stay the same).

## Made from the AdvancedAudioTemplate
Each plugin is an instance of the AdvancedAudioTemplate (`~/AudioDev/AdvancedAudioTemplate`, branch AAT2, commit 1c91c87) in the simple
form: `WITH_PRESETHANDLERGUI` and `WITH_DAYNIGHT` off. From the template, with `YourPluginName` replaced:
`<Name>/PluginProcessor.*` and `<Name>/PluginEditor.*` (glue, CRLF line endings kept); shared by the three: `tools/` and `PluginSettings.h`.
Changes to the template code:
- `PluginSettings.h`: `g_desired_blocksize_ms = 0` (the template's `SynchronBlockProcessor` then processes the host's blocks directly:
  latency 0), one GUI size for all three (640 x 400 minimum).
- `PluginProcessor.cpp`: `loadfromFileAllUserPresets()` only `#if WITH_PRESETHANDLERGUI` (the simple form has no preset bar; otherwise
  every instance would create a preset folder in the user's home, also in the tests).

Own code: `<Name>/<Name>.h/.cpp` (parameters, algorithm, GUI) and `ReferenceControls.h/.cpp` (a choice parameter defined once, like the
template's float parameters in `tools/ParameterSpec.h`, and the grid of knobs and choice boxes with the help texts as tooltips).

## Controls
### PluginLab Reference EQ
| Control | Range | Meaning |
|---|---|---|
| Algorithm | RBJ cookbook, Orfanidis, Zoelzer (DAFX), State variable (TPT), Butterworth, Linkwitz-Riley, Linear phase (RBJ magnitude) | the design (from `pluginlab_eqmodel` since 0.44.0); linear phase: a 2047-tap FIR at 48 kHz, latency 1023 samples, reported to the host |
| Type | low-pass, high-pass, band-pass, band-pass 0 dB, notch, all-pass, peak, low shelf, high shelf | the filter type |
| Frequency | 20 ... 20000 Hz | corner or centre (limited to 0.45 fs) |
| Gain | -24 ... 24 dB | peak and shelves |
| Q | 0.1 ... 20 | RBJ definitions (shelves: 0.71 = no overshoot) |
| Order | 1 ... 8 | Butterworth 1 ... 8; Linkwitz-Riley 2, 4, 8 (rounded up); Zoelzer shelves 1st or 2nd order |

Offered combinations: RBJ and state variable all types; Orfanidis the peak; Zoelzer the shelves and the peak; Butterworth and
Linkwitz-Riley low-pass and high-pass. In the GUI the types an algorithm does not offer are greyed out, and a type that is not offered after a change of
the algorithm moves to the algorithm's default (peak, resp. low-pass for Butterworth and Linkwitz-Riley; author's request, 0.27.1). Set by automation, a
combination that is not offered passes the audio unchanged, and the GUI says so.

### PluginLab Reference Nonlinear
| Control | Range | Meaning |
|---|---|---|
| Curve | Off, Polynomial, Hard clip, Soft clip (tanh) | y = x + a2 x^2 + a3 x^3; clip at the threshold; y = tanh(x) |
| Drive | -24 ... 24 dB | gain before the curve (for tanh the drive) |
| a2, a3 | -1 ... 1 | polynomial coefficients |
| Threshold | -40 ... 0 dBFS | hard clipper |
| Output | -24 ... 24 dB | gain after the curve |
| Quantizer | Off, On, On with TPDF dither | at the end of the chain |
| Bits | 2 ... 24 | word length (full scale +-1, mid-tread) |
| Noise | Off, White, Pink | seeded; channels uncorrelated |
| Noise level | -140 ... 0 dBFS | RMS per channel |
| Hum | Off, 50 Hz, 60 Hz | a mains sine |
| Hum level | -140 ... 0 dBFS | peak |

### PluginLab Reference Utility
| Control | Range | Meaning |
|---|---|---|
| Gain | -60 ... 24 dB | all channels |
| Polarity | Normal, Inverted | |
| Delay | 0 ... 4800 samples (0.01 steps) | an effect: the reported latency stays 0 |
| Interpolation | Thiran (all-pass), Lagrange (FIR) | for fractional delays, order 3 (Thiran: order 1 below 2.5 samples, Lagrange: order 1 below 1 sample; below 0.5 samples always Lagrange order 1) |
| Width | 0 ... 2 | M/S: 0 mono, 1 unchanged |
| Crosstalk | -120 ... 0 dB | each channel into the other; -120 dB = off |
| DC offset | -0.5 ... 0.5 | |
| Tremolo rate, depth | 0.1 ... 20 Hz, 0 ... 1 | gain between 1 - depth and 1; depth 0 = off |

### PluginLab Reference Delay
The author's `jade::BasicDelayEffect` ported with the corrections of `docs/design/W11-delay-plan.md` 7. Unlike the other three, a change
of a parameter keeps the echoes in the line (a jump: the author's "digital" switch); the line is allocated in prepareToPlay.

| Control | Range | Meaning |
|---|---|---|
| Time | 1 ... 2000 ms | the delay (fractional samples: the interpolation); greyed out when synced |
| Sync | Off (ms), 1/4, 1/8, 1/8 dotted, 1/8 triplet, 1/16 | a note value of the host's tempo (120 BPM without one) |
| Interpolation | Lagrange (FIR), Thiran (all-pass) | Lagrange of order 3 (the author's tape glide: darkens every repeat a little), Thiran of order 1 (|H| = 1) |
| Feedback | -95 ... 95 % | gain of one trip; negative inverts every repeat |
| Loop filter | Off, On | the low and high cut in the loop |
| Low cut, High cut | 20 ... 2000 Hz, 500 ... 20000 Hz | first-order Butterworth high- and low-pass in the loop |
| Loop order | Exact, Author's original | the original adds the previous sample's output: echoes at D, 2D + 1, 3D + 2, ... |
| Mix | 0 ... 100 % | (1 - mix) dry + mix wet |

## Build, test, install
They are built with pluginlab (`plugins/reference/CMakeLists.txt`) and copied into `<build>/reference_plugins` (used by
`tests/ReferencePluginTests.cpp`: the host loads them, sets parameters by text and compares the output with the library). A Release build
installs them into `~/.vst3` (macOS: `~/Library/Audio/Plug-Ins/VST3`), option `PLUGINLAB_INSTALL_REFERENCE_PLUGINS` (default on for a local
Release build, off in CI and on Windows):
```console
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target PluginLabReferenceEq_VST3 PluginLabReferenceNonlinear_VST3 PluginLabReferenceUtility_VST3
```
Their fingerprints: `docs/fingerprints/PluginLab_Reference_*.md`.
