# W6: reference processors and test signals (plan for discussion)

Status: proposal, 2026-10-06. Planning §8: "Reference processors (RBJ, Orfanidis, Zölzer) + test signals in C++, plus shared test signals and reference result files
from the Python oracle. Done when: analytic ground truth and oracle files are available for the measurement tests."

## Why W6 comes before the measurements
Every measurement unit of W7 (frequency response, phase, group delay, latency, THD, THD+N, noise, SNR, crosstalk, null test) must be tested against something whose
answer is known exactly. LESSONS_LEARNED §1 and §3: "write the reference first, then the measurement"; a numpy reference made every prototype measurement testable. W6
builds these references in C++, and the signals the measurements use, so that W7 can be written test-first. Nothing of W6 is in the product's user interface yet.

## 1. Reference processors (proposal)
All in a new library `pluginlab_reference` (`src/reference/`, plain C++ with `juce_core`/`juce_audio_basics` only, no GUI, no hosting). Every processor has
**process(buffer)** and, where it exists, its **analytic answer** (for linear filters the complex transfer function H(e^jw) at any frequency; for nonlinear ones the
harmonic levels for a sine; for noise sources the level). Sample-rate correct by construction (designed from the real sample rate, the opposite of the PeakEQ fault).

### 1.1 Linear filters (ground truth for frequency response, phase, group delay, the EQ band model of W8)
| Family | Types | Why |
|---|---|---|
| **RBJ cookbook** (bilinear, Q or bandwidth or slope) | peak, low shelf, high shelf, low-pass, high-pass, band-pass (both gains), notch, all-pass | the standard most plugins use; the prototype's reference |
| **Orfanidis** (prescribed Nyquist gain, "decramped") | peak (and shelf if cheap) | matches the analog shape up to Nyquist: shows the bilinear cramping of the others |
| **Zölzer / DAFX** | 1st and 2nd order low/high shelf, peak | the textbook versions used in teaching (DAFX) |
| **Analog prototypes** (s-domain, no discretisation) | the same peak/shelf/LP/HP as magnitude and phase functions only | the "ideal" curve a plugin is compared with (cramping, the BLT question of the prototype) |
| **Butterworth / Linkwitz-Riley** (cascaded biquads) | LP/HP 1st-8th order, LR 2/4/8 | slopes in dB/octave; crossover filters; higher-order phase |
| **Linear-phase FIR** (windowed design from a target magnitude) | any magnitude, e.g. the RBJ peak magnitude with linear phase | same magnitude, different phase: latency, pre-ringing, null test, phase measurement |
| **Delays** | integer, fractional (Thiran all-pass and Lagrange) | latency measurement with sub-sample truth; alignment in the null test |

### 1.2 Simple processors with known answers
| Processor | Known answer | Tests |
|---|---|---|
| gain, polarity inversion | exact | level, phase, null |
| channel mix (matrix: crosstalk of x dB, M/S width) | exact per channel pair | crosstalk, channel coupling, stereo measurements |
| memoryless polynomial y = x + a2 x² + a3 x³ (+ a5 x⁵) | harmonic levels for a sine in closed form | THD, harmonic sweep (Farina), IMD |
| hard clipper, tanh soft clipper | Fourier series of the clipped sine (computed exactly/numerically once) | THD at levels, compression of a sine |
| quantizer N bits (with/without TPDF dither) | SNR = 6.02 N + 1.76 dB for a full-scale sine; dither noise level | noise, SNR, THD+N, dynamic range |
| noise adder (white/pink, seeded, level in dBFS) | noise level and spectrum | noise floor, SNR, THD+N |
| DC offset, 50/60 Hz hum adder | exact | measurement robustness |
| tremolo (known rate and depth) | exact envelope | time-varying detection, later modulation measurements |

### 1.3 Later, not in W6 (proposal)
Dynamics with a known static curve and known attack/release (W11), delays with feedback and reverbs (W11), oversampled variants (only if the cramping study needs them).

### 1.4 Reference plugins
The same code wrapped as VST3 test plugins (built in `tests/plugins`, like the current test plugins), so that the whole chain host → plugin → measurement is tested
end to end, and so that the fingerprint and the Compare page have plugins whose truth is known. Proposal: one plugin per group with an "algorithm" choice and parameters in
physical units (Hz, dB, Q): **PluginLab Reference EQ** (all filter types of 1.1), **PluginLab Reference Nonlinear** (1.2 nonlinear and noise), **PluginLab Reference
Utility** (gain, polarity, delay, channel mix). They also become teaching objects ("this is what an ideal peak filter does").

## 2. Test signals (proposal)
A library `pluginlab_signals` (`src/signals/`): every signal deterministic (seeded), described by a small struct (type, sample rate, length, level in dBFS peak or RMS,
channels and their relation, seed, fades), generated identically on every platform, and writable as WAV (32-bit float) for the oracle and for listening.

| Signal | Parameters | Used for |
|---|---|---|
| unit impulse (with pre-delay) | position | impulse response, latency (already in the fingerprint) |
| step | position | DC behaviour, settling |
| sine | frequency (optionally snapped to an FFT bin), level, phase, fade in/out | THD, THD+N, level, gain, phase at one frequency |
| exponential sine sweep (Farina) + its inverse filter | f1 (10 Hz, lesson of the prototype), f2, length, fades, pre/post silence | frequency response, phase, group delay, harmonic distortion in one run |
| linear sweep | f1, f2, length | comparison, aliasing tests |
| two-tone | SMPTE (60 Hz + 7 kHz, 4:1), CCIF/ITU (19 + 20 kHz, 1:1), free | IMD |
| multitone | log-spaced tones, Schroeder (or Newman) phases for a low crest factor | fast frequency response, noise-in-band |
| white noise (uniform, Gaussian), pink noise | level, seed, band limits | noise, transfer function by cross-spectrum, the fingerprint's noise |
| stereo variants | L = R, L != R (uncorrelated), L only, R only, L = -R | crosstalk, channel coupling, M/S |
| sine bursts and level steps | frequency, levels, burst/gap lengths | dynamics (W11), attack/release, time-varying detection |
| silence | length | noise floor, idle behaviour |
| music excerpts | from the MusicRadar packs via the download script (decided earlier), not in the repository | null test, listening |
Standard levels proposed: -20 dBFS RMS for noise and multitones, -6 dBFS and -20 dBFS peak for sines and sweeps (two levels to see level dependence), 0 dBFS only for
explicit headroom tests.

## 3. The oracle (planning §9 item 4, to decide)
The prototype (`~/AudioDev/measurement_tool`, Python) computes the same references and the same measurements; the C++ results must agree within a stated tolerance.
Proposal:
- **Format:** one JSON file per case (`tests/oracle/<case>.json`): the generator settings of the signal, the processor and its parameters, the measured quantities with their
  frequency axis, and the tolerance the comparison is allowed. Plus the WAV of the signal only where the signal itself is under test (sweeps), else the C++ side regenerates it.
- **Who makes them:** a script in the Python repository (`tools/export_oracle.py`) writes the JSON files; they are committed in `pluginlab/tests/oracle/` with the
  version of the script that made them. C++ tests read them (`juce::JSON`).
- **First cases:** RBJ peak / shelf / LP magnitude and phase at 48 kHz (sweep), THD of the polynomial at 1 kHz, SNR of the 16-bit quantizer, latency of a 37.25-sample
  fractional delay.

## 4. Sub-work-packages (proposal, in this order)
| Step | Content | Done when |
|---|---|---|
| **W6.1** signals | `pluginlab_signals`: generators of section 2, metadata, WAV export | each signal checked against its definition: RMS/peak level, frequency by FFT, sweep convolved with its inverse = band-limited impulse, Schroeder crest factor, noise statistics, the same samples on every platform (hash in the test) |
| **W6.2** linear references | RBJ, Orfanidis, Zölzer, analog prototypes, Butterworth/LR, linear-phase FIR, delays, each with H(e^jw) | impulse response through the processor, FFT, against H(e^jw): < 0.001 dB and < 0.01 degree; stable; correct at 44.1/48/96 kHz; RBJ against the published cookbook formulas |
| **W6.3** nonlinear and noise references | polynomial, clippers, quantizer, noise and hum adders, channel matrix, tremolo, with their analytic answers | a sine through each: harmonic levels against the closed form; SNR against 6.02 N + 1.76; noise level against the setting |
| **W6.4** reference plugins | Reference EQ, Reference Nonlinear, Reference Utility as VST3 test plugins | the fingerprint of each is clean; their parameter text gives the physical values; their output equals the library processor bit for bit |
| **W6.5** oracle | JSON format, `export_oracle.py` in the Python repo, C++ reader, the first cases of section 3 | the C++ references agree with the oracle files within the tolerance stated in each file |
| **W6.6** documentation | one page per reference (formula, parameters, analytic answer, plot) for the teaching material (CC BY-SA) | written |

## 5. Questions for the author
1. **Filter families:** RBJ, Orfanidis, Zölzer, analog prototypes, Butterworth/Linkwitz-Riley, linear-phase FIR and delays: enough, too much, something missing (e.g. Vicanek's matched
   biquads, state-variable filter / TPT/ZDF designs as many modern plugins use them)?
2. **Nonlinear set:** polynomial, clippers, quantizer, noise: enough for W7? Dynamics only with W11?
3. **Signals and levels:** the list and the standard levels of section 2 (-20 dBFS RMS noise, -6 and -20 dBFS peak sines/sweeps), the sweep from 10 Hz?
4. **Oracle:** JSON per case, made by a script in the Python repository, committed in `tests/oracle/`: agreed? Who runs the script when the prototype changes (me on request)?
5. **Reference plugins:** three plugins with an algorithm choice (my proposal), or one plugin per algorithm?
6. **Order:** W6.1 and W6.2 first (W7's frequency response needs them), W6.3 before THD/noise, W6.4 and W6.5 can run in parallel?
