# The matcher: the fast render path

Unit of W9.3. Code: `src/engine/LivePluginDevice.cpp` (`LivePluginTarget`,
`checkLiveTarget`), the impulse stimulus of `MagnitudeProbe`
(`src/match/EqDomain.cpp`); tests `tests/MatchPluginTests.cpp` (CTest
`PluginLabFastRenderPath`). Plan: `docs/design/W9-plan.md`, 4.2.

## Purpose
The author's budget: 10 ... 15 s per target, no parallel renders. A match of
three parameters needs about 40 renders, so a render may take about 0.25 s.
The careful delivery of the measurements (a fresh instance per render, a
sweep, W7.11) took 0.28 s even for a biquad in Debug, and longer for plugins.

## How
| Part | What |
|---|---|
| one live instance | created and prepared once per match; a render sets every parameter to another value first (the "poke": a plugin that reads a parameter only when it changes cannot miss the setting), runs one block of silence, sets the setting, settles 0.25 s in silence (the ringing of the last render decays, smoothing ends), then runs the input |
| impulse instead of sweep | an impulse at the probe level (-12 dBFS); 0.35 s of response; H(f) = DTFT of the output / the impulse's amplitude on the probe's grid |
| check before the match | the system class at the start setting (careful delivery, W8.1) must be LTI, otherwise the match is refused with the reason (the author: "Refusal, that is for later"); the same setting rendered twice through the live instance must agree within 0.001 dB (no state carried over, deterministic) |

## Results
From `MatchPluginTests` (2026-10-13, 0.58.0), Debug, 48 kHz:

| Plugin | Fast against careful | Per fast render |
|---|---|---|
| Test EQ (3 settings) | the fast path against the exact RBJ curve: at most 0.0004 dB; the careful sweep: up to 0.047 dB (Q 8: its short window) | 41 ms |
| Test EQ Prepare Fault (reads a parameter only when it changes) | 0.004 dB | 41 ms |
| Test EQ Switch (band switched on) | 0.004 dB | 42 ms |
| Reference EQ (RBJ peak, Butterworth low-pass order 4) | 0.004 dB (above -60 dB) | 40 ms |

About seven times faster than the careful sweep, and more exact. The poke
makes the Prepare Fault plugin (the PeakEqualizer template fault) work.
Refused with their reasons: the Test EQ Drive with the drive on ("not LTI:
other, the harmonic responses change their shape with the level") and the
Reference Utility with the tremolo on ("not LTI: time-variant, 2.77 dB").

## Limits
- **Ringing longer than the response window:** a +19 dB peak at 20 Hz with
  Q 8 rings for about 1.2 s down to -80 dB (time constant Q / (pi f)); 0.35 s
  of impulse response (and the sweep's 0.25 s window) cut it off, and both
  paths are wrong there, differently. Rule: the window should exceed about
  9 Q / (pi f) at the lowest band; 0.35 s covers Q 2.4 at 20 Hz, Q 12 at
  100 Hz.
- **Slow smoothing:** a plugin that smooths its parameters for longer than
  the settle time (the fingerprint's "settles within") gives a ramp; the
  check of two renders does not catch a smoothing that ends the same way
  twice. The settle time can be raised (`LiveDeviceOptions`).
- One sample rate per live instance.
