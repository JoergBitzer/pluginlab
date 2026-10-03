# Findings ("audio detective" case studies from the measurements)

All numbers are measured with pluginlab on Linux, 48 kHz (see `examples/`). Causes marked *hypothesis* are not yet verified in source code.

## M3: one peaking band, five plugins matched to a master
Master: own PeakEqualizer (RBJ peaking: +9 dB, 2.5 kHz, Q 2.2). `examples/m3_match_eq.py` finds the settings of each other plugin by CMA-ES on the swept magnitude response (~400 renders each, 22 s for all five in parallel):

| plugin | rms deviation | max deviation | found settings |
|---|---|---|---|
| ZeroEQ | 0.005 dB | 0.030 dB | 9.02 dB, 2496 Hz, Q 2.20 |
| FreeEQ8 | 0.001 dB | 0.004 dB | 9.02 dB, 2502 Hz, Q 2.21 |
| ZL Equalizer 2 | 0.012 dB | 0.024 dB | 9.02 dB, 2501 Hz, Q 2.24 |
| Biquads | 0.015 dB | 0.068 dB | gain knob 4.48 dB, 2502 Hz, Q knob 0.77 |
| own PeakEQ | 0.025 dB | 0.098 dB | 9.03 dB, **2306 Hz**, Q 2.17 |

All five are within the technical tolerance (0.1 dB) of the master. Four of them are the same filter with a different parameter scale; two of them are not what the label says:

1. **Own PeakEQ: the frequency knob is off by a constant factor** (`docs/m3_calibration.txt`): measured f0 = 1.087 x knob at every frequency (500 -> 543 Hz, 8 kHz -> 8640 Hz). 48000/44100 = 1.088. *Hypothesis:* the filter coefficients are designed for 44.1 kHz and the plugin runs at 48 kHz. Check the sample rate used in the design code.
2. **Biquads: the gain knob is doubled** (3 dB -> 6.0 dB, 6 -> 12.0, 9 -> 18.0 measured). *Hypothesis:* the peaking filter uses A = 10^(dB/20) where the RBJ cookbook uses 10^(dB/40) (A is the square root of the gain).
3. **Biquads: Q is a normalized 0..1 knob** with a nonlinear map (0.2 -> Q 0.64, 0.4 -> 0.85, 0.6 -> 1.27, 0.8 -> 2.53, measured at 2 kHz, +18 dB measured gain).
4. **Q seems to grow with f0 near Nyquist** in the own plugins (Q measured 2.0 at 500 Hz, 2.5 at 8 kHz for a knob value of 2.0): the usual bilinear-transform frequency warping, visible with the half-gain-bandwidth definition.

## Second batch (Multi-Q, WayQ, 4K-EQ) matched to the same master
Same master and method, 8 plugins in 67 s (`examples/m3_match_eq.py`, all within the 0.1 dB tolerance except WayQ):

| plugin | rms | max | found settings |
|---|---|---|---|
| Dusk Multi-Q (band 2) | 0.004 dB | 0.024 dB | 9.00 dB, 2502 Hz, Q 2.25 |
| Dusk 4K-EQ (high-mid band, Brown) | 0.014 dB | 0.035 dB | 9.02 dB, 2500 Hz, **Q knob 3.75** for Q 2.2 |
| WayQ (mid band) | **0.124 dB** | **0.305 dB** | 9.08 dB, 2480 Hz, Q 2.29: not within tolerance |

- **WayQ is not an RBJ peaking filter.** The centre frequency moves with the gain at a fixed knob frequency (knob 2500 Hz: measured 2530 Hz at +3 dB, 2517 Hz at +12 dB) while gain and Q follow the knobs. The best match still deviates by 0.3 dB. *Hypothesis:* a different filter design (e.g. analog-matched or Orfanidis-type), not checked.
- **4K-EQ: the Q knob is not Q** (knob 2.0 measures Q about 1.2 in the Brown mode, about 2.2 in the Black mode); the two EQ types differ.
- BaxEQ (Baxandall shelving tone control; its parameters have no numeric range in the host) and WSTD MSEQ (mid band without a Q control) have no peak-band mapping and are not matched.

## Host findings: how plugins take over parameter values (all found while measuring)
Setting a parameter and rendering did not give the same result in every plugin. This is why `PedalboardHost` tries three render strategies (`never`, `always`, `stream`) and accepts the first whose output is repeatable, reacts to the parameters and does not depend on the history (target, target, alternative, target must give A, A, B, A):
- **Own PeakEqualizer:** `prepareToPlay` re-initialises with hard-coded values (see M1); only a fresh instance per render works (`always`).
- **Dusk 4K-EQ:** values set before the first process call are ignored; a value only takes effect when it *changes after* the first process call (`stream`: prime with zeros, deliver each parameter through a slightly different value).
- **Dusk Multi-Q:** values set before the first process call are applied with the default 44.1 kHz sample rate: the frequency came out 8.75 % too high (2718 Hz for 2500 Hz) at 48 kHz in the `never` and `always` strategies, correct (2498 Hz) in `stream`. A wrong-but-repeatable strategy therefore passes the self-test; `stream` is tried first for this reason.
- **Own PeakEQ still shows the factor 1.0875 in `stream`** (2306 Hz knob for a measured 2500 Hz), so for this plugin the 44.1 kHz hypothesis of the first finding stays open: it is not explained by parameters being set before the sample rate was known.
- **Block size:** the "block-size dependence" of Multi-Q, 4K-EQ and Pult-EQ seen in M0 was a bug of the host, not of the plugins: with `reset=True` on the first block pedalboard handles the plugin latency differently from the following blocks (output jumped by 60/49 samples after the first block). The streaming path now never uses `reset=True`; Multi-Q, 4K-EQ and WayQ give bit-identical output for all block sizes (64 to 16384), Pult-EQ differs by 6e-8 (float rounding), ZeroEQ by up to 1.5e-8.
- **Multi-Q goes silent (NaN state, never recovers without reset) after a Q jump from 25 to 2** in the `stream` strategy. The host nudges parameters by only 2 % of their range and reloads the plugin when a render comes back dead.

## M1: own PeakEqualizer template
See `docs/plugin_shortlist.md`: it equals the RBJ filter exactly; its 96-sample delay is a reported block-size latency; its `prepareToPlay` re-initialises with hard-coded values (4 kHz, Q 9, +20 dB) and parameters are only re-read when they change.

## Pult-EQ
Tube-style EQ: THD about -57 dB at 1 kHz, -20 dBFS (the only plugin with measurable harmonics), idle noise -157 dBFS; output depends on block size and is not bit-exact between runs (6e-8). Not yet investigated.

## High-frequency / bilinear-transform test (`examples/m5_blt.py`, raw numbers `renders/m5_blt.json`)
Each plugin's gain, frequency and Q knobs were fitted (CMA-ES, 300 renders) to the *analog* peaking prototype (+9 dB, Q 2.2) at four centre frequencies, fs = 48 kHz, compared over 40 Hz - 19 kHz. Table: max deviation in dB (rms in brackets).

| | 2.5 kHz | 10 kHz | 14 kHz | 18 kHz |
|---|---|---|---|---|
| PeakEqualizer | 0.06 | 0.65 | 1.14 | 6.00 |
| PeakEQ | 0.11 | 8.64 | 1.06 | 0.41 |
| ZeroEQ | 0.06 | 0.65 | 1.09 | 0.41 |
| FreeEQ8 | 0.06 | 0.62 | 1.32 | 1.38 |
| FreeEQ8 oversampling 8x | 0.21 | 0.30 | 0.17 | 0.60 |
| Biquads | 0.87 | 0.63 | 1.09 | 0.64 |
| Biquads oversampling 16x | 0.87 | 0.63 | 1.09 | 0.64 |
| ZL EQ 2 minimum phase | 0.06 | 0.13 | 1.10 | 0.18 |
| ZL EQ 2 matched phase | 0.09 | 1.87 | 1.20 | 1.16 |
| Multi-Q | 0.06 | 0.62 | 1.16 | 0.42 |
| Multi-Q oversampling 4x | 0.06 | 1.89 | 0.10 | 0.47 |
| WayQ | 1.18 | 0.74 | 0.73 | 0.28 |
| 4K-EQ | 0.06 | 5.81 | 7.18 | 8.12 |

**What the table supports:** at 10 and 14 kHz most plain designs (PeakEqualizer, PeakEQ, ZeroEQ, FreeEQ8, Biquads, Multi-Q, ZL min. phase at 14 kHz) end up around 0.6 dB and 1.1 dB: the same size of deviation from the analog shape, as expected from a bilinear transform. FreeEQ8 with 8x oversampling (0.17 dB at 14 kHz) and Multi-Q with 4x (0.10 dB at 14 kHz) come closest to the analog shape there.

**What it does not support (do not read more into the table):**
- Several numbers are limits of the knobs, not of the filter: PeakEqualizer's frequency knob ends at 15 kHz (6.0 dB at 18 kHz), 4K-EQ's high-mid band ends at 7 kHz (5.8-8.1 dB).
- Some numbers look like optimizer failures (PeakEQ 8.64 dB at 10 kHz but 0.41 dB at 18 kHz; ZL min. phase 1.10 dB at 14 kHz but 0.13 and 0.18 at its neighbours; Biquads 0.87 dB and WayQ 1.18 dB at 2.5 kHz). They were not repeated with other seeds.
- "Biquads oversampling 16x" is identical to plain Biquads to all digits: the setting probably had no effect (not checked).
- The comparison band ends at 19 kHz, so for an 18 kHz peak the part of the response where the cramping is largest (towards 24 kHz) is not compared. The 18 kHz column is therefore weak evidence.
- ZL "matched phase" did not do better than minimum phase here, which I did not expect and have not explained.
A better test would compare up to about 23 kHz (sweep end above 20 kHz) and repeat each fit with several seeds.
