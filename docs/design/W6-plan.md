# W6: reference processors and test signals (plan for discussion)

Status: **agreed plan** (author's answers and comments of 2026-10-06 worked in; the original questions and answers are kept in section 5). Planning §8: "Reference processors (RBJ, Orfanidis, Zölzer) + test signals in C++, plus shared test signals and reference result files
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
| **State-variable filter** (TPT/ZDF, Zavalishin; the Simper/Cytomic form) | LP, HP, BP, notch, peak, shelves from one structure | the structure many modern plugins use; also correct under fast modulation (author: in the first step; more families later if needed) |
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
end to end, and so that the fingerprint and the Compare page have plugins whose truth is known. Agreed: one plugin per group with an "algorithm" choice and parameters in
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
| **synchronized exponential swept sine** (Novak, Lotton, Simon, JAES 63(10), 2015) + its **analytic inverse filter** | f1 (**30 Hz** default, 10 Hz kept in reserve), f2, approximate length; L = k/f1 with k = round(f1 T / ln(f2/f1)), x(t) = sin(2 pi f1 L exp(t/L)), zero phase at start and end; inverse filter X~(f) = 2 sqrt(f/L) exp(-j 2 pi f L (1 - ln(f/f1)) + j pi/4); the higher harmonic impulse responses at -L ln(n) | frequency response, phase, group delay, the higher harmonic frequency responses in one run (author: Novak instead of Farina) |
| **stepped sine** (the Audio Precision method) | log-spaced frequencies from 20 kHz down to 20 Hz (points per octave), per step: the latency of the device + a settling time + a measurement time, both depending on the measurement (THD longer than the transfer function); the signal comes with the list of steps and their measurement windows | magnitude per frequency (no phase), THD, THD+N, crosstalk: slow but very reliable (author) |
| two-tone | SMPTE (60 Hz + 7 kHz, 4:1), CCIF/ITU (19 + 20 kHz, 1:1), free | IMD |
| multitone | log-spaced tones, Schroeder (or Newman) phases for a low crest factor | fast frequency response, noise-in-band |
| white noise (uniform, Gaussian), pink noise | level, seed, band limits | noise, transfer function by cross-spectrum, the fingerprint's noise |
| stereo variants | L = R, L != R (uncorrelated), L only, R only, L = -R | crosstalk, channel coupling, M/S |
| sine bursts and level steps | frequency, levels, burst/gap lengths | dynamics (W11), attack/release, time-varying detection |
| silence | length | noise floor, idle behaviour |
| music excerpts | from the MusicRadar packs via the download script (decided earlier), not in the repository: **drum loops, guitar loops, a piano excerpt, a vocal excerpt, complete mixes (excerpts)** (author) | null test, listening, teaching |
Standard levels (agreed): -20 dBFS RMS for noise and multitones, -6 dBFS and -20 dBFS peak for sines and sweeps (two levels to see level dependence), 0 dBFS only for
explicit headroom tests; more later if needed.

## 3. The oracle (agreed, the author is not fully sure; to be reviewed after the first cases)
The prototype (`~/AudioDev/measurement_tool`, Python) computes the same references and the same measurements; the C++ results must agree within a stated tolerance.
Proposal:
- **Format:** one JSON file per case (`tests/oracle/<case>.json`): the generator settings of the signal, the processor and its parameters, the measured quantities with their
  frequency axis, and the tolerance the comparison is allowed. Plus the WAV of the signal only where the signal itself is under test (sweeps), else the C++ side regenerates it.
- **Who makes them:** a script in the Python repository (`tools/export_oracle.py`) writes the JSON files; they are committed in `pluginlab/tests/oracle/` with the
  version of the script that made them; I run it on request when the prototype changes. C++ tests read them (`juce::JSON`).
- The weak point the author sees: two implementations by the same author (me) can share a mistake. Therefore every oracle case is also checked against analytic ground truth
  where one exists (the references of W6.2/W6.3), and the oracle is used where only a numerical answer exists (sweep deconvolution, windowing choices of THD+N).
- **First cases:** RBJ peak / shelf / LP magnitude and phase at 48 kHz (synchronized sweep), THD of the polynomial at 1 kHz (stepped sine and sweep), SNR of the 16-bit
  quantizer, latency of a 37.25-sample fractional delay.

## 4. Sub-work-packages (agreed order: W6.1 and W6.2 first, W6.3 before THD/noise, W6.4 and W6.5 in parallel)
| Step | Content | Done when |
|---|---|---|
| **W6.1** signals | `pluginlab_signals`: generators of section 2 (with the synchronized swept sine and its analytic inverse filter, and the stepped sine with its step list), metadata, WAV export | each signal checked against its definition: RMS/peak level, frequency by FFT, zero phase at start and end of the sweep, sweep deconvolved with the analytic inverse = band-limited impulse, the harmonic impulse responses of a known polynomial at -L ln(n) with the right levels, stepped-sine frequencies and windows, Schroeder crest factor, noise statistics (the hash test "same samples on every platform" dropped by the author, 2026-10-06: the oracle comparison uses tolerances) |
| **W6.2** linear references | RBJ, Orfanidis, Zölzer, state-variable (TPT), analog prototypes, Butterworth/LR, linear-phase FIR, delays, each with H(e^jw) | impulse response through the processor, FFT, against H(e^jw): < 0.001 dB and < 0.01 degree; stable; correct at 44.1/48/96 kHz; RBJ against the published cookbook formulas |
| **W6.3** nonlinear and noise references | polynomial, clippers, quantizer, noise and hum adders, channel matrix, tremolo, with their analytic answers | a sine through each: harmonic levels against the closed form; SNR against 6.02 N + 1.76; noise level against the setting |
| **W6.4** reference plugins | Reference EQ, Reference Nonlinear, Reference Utility as VST3 test plugins | the fingerprint of each is clean; their parameter text gives the physical values; their output equals the library processor bit for bit |
| **W6.5** oracle | JSON format, `export_oracle.py` in the Python repo, C++ reader, the first cases of section 3 | the C++ references agree with the oracle files within the tolerance stated in each file |
| **W6.6** documentation | one page per reference (formula, parameters, analytic answer, plot) for the teaching material (CC BY-SA) | written |

## Progress

### W6.1 signals (0.17.0, done)
`src/signals/` builds the static library `pluginlab_signals` (namespace `pluginlab::signals`; uses `juce_audio_basics`, `juce_audio_formats`, privately `juce_dsp`):
- `Signals.h`: impulse, step, silence, sine (optionally on an FFT bin, fades), two-tone (SMPTE, CCIF, free), multitone, noise (uniform, Gaussian, pink after
  Voss-McCartney with 16 rows), bursts, the channel relations (L = R, uncorrelated, L only, R only, L = -R), level helpers, `writeWav` (32-bit float).
- `SweptSine.h`: the synchronized swept sine (Novak et al.) with L = k/f1 for integer k, its analytic inverse filter (eq. 43), the deconvolution
  h = IFFT(Y X~) / (fs A) and the position of the n-th harmonic impulse response (-L ln n).
- `SteppedSine.h`: the stepped sine 20 kHz down to 20 Hz, log-spaced (3 steps per octave by default), each step with its measurement window after
  latency + settling time, a whole number of periods long.

`tests/SignalsTests.cpp` checks every signal against its definition. What the tests showed:
- The deconvolved sweep is flat within 0.1 dB from 100 Hz to 10 kHz; its peak is not 1 but the share of the band in 0 ... fs/2 (2 (f2 - f1) / fs = 0.83
  for 30 Hz ... 20 kHz at 48 kHz), as it must be for a band-limited impulse. A delay of 37 samples moves the peak by 37 samples.
- For y = x + 0.1 x^2 + 0.05 x^3 at -6 dBFS the 2nd and 3rd harmonic impulse responses lie at N - L ln(n) fs (within 2 samples), with -32.05 dB
  (closed form -32.02 dB) and -50.37 dB (closed form -50.06 dB) relative to the fundamental at 1 kHz.
- Schroeder's formula -pi k (k-1) / N is meant for linearly spaced tones; for 31 log-spaced tones it did *not* lower the crest factor (12.9 dB against
  11.7 dB for zero sine phases). The library now uses the general form (the group delay of tone k is k/N of the period, phase_k = phase_k-1 - 2 pi tau_k
  (f_k - f_k-1); for linear spacing this is Schroeder's formula): 12.5 dB against 17.9 dB with all tones in phase. Lower values for sparse log-spaced tones
  need an iterative method (clipping and restoring the spectrum); not done, noted for W7 if the fast frequency response needs it.
- Pink noise: the octave bands 63 Hz ... 8 kHz lie within 0.8 dB; white noise rises by 2.99 dB per octave.

Not done in W6.1 (open): the metadata struct per signal (each generator has its settings struct instead; a common description comes with the oracle in W6.5);
the music excerpts (download script, later). Dropped (author, 2026-10-06): the hash test "the same samples on every platform" (`std::sin`/`std::log`
may differ in the last bit between platforms; the oracle comparison in W6.5 uses tolerances).

### The signals as files (0.19.0)
`apps/signals/` builds `PluginLabSignals <folder> [--seconds 5,10] [--rate 48000]`: the standard set as stereo 32-bit float WAV files, one per length
(23 per length): silence; impulse and step (-6 dBFS at 0.1 s); sines 1 kHz (-6 and -20 dBFS), 100 Hz and 10 kHz (-6 dBFS, 10 ms fades); two-tone SMPTE and
CCIF; multitone (31 tones, Schroeder, -20 dBFS RMS); noise at -20 dBFS RMS (white Gaussian L = R, uncorrelated, L only, R only, L = -R; white uniform;
pink L = R and uncorrelated); synchronized sweeps 30 Hz ... 20 kHz (-6 and -20 dBFS; the sweep as long as the integer k allows, at least 0.5 s silence after it);
stepped sines 20 kHz -> 20 Hz (-6 and -20 dBFS; 3 steps per octave, the measurement time as long as fits, at least 4 periods; the steps as CSV); sine bursts
1 kHz with rising levels up to -5 dBFS. `signals.txt` describes every file (for the sweep: L, k, start sample, length). The levels were checked with the
Python prototype's soundfile: all as intended (CCIF shows -6.34 dBFS sample peak: the true peak is -6 dBFS, the samples miss it). CTest `PluginLabSignalsWrites`.
Set written for the author: `~/Music/TestSignals/` (5 s and 10 s, 48 kHz).

0.19.1 (author: the switches between the steps clicked, a click excites other frequencies): every step of the stepped sine now has a Hann fade-in and
fade-out of `fadeSeconds` (default 10 ms, 0 = hard switches): | fade-in | latency + settling | measurement window | fade-out |. The fades lie outside
the settling time and the window. For one 97 Hz step the energy at 1234.5 Hz fell from -57.4 dB (hard switches) to -118.1 dB re the tone.

0.20.0 (author: missing for resampling tests): the linear sweep `makeLinearSweep` (0 Hz ... Nyquist by default, 5 ms Hann fades, silence around it);
in a spectrogram an alias of a resampler shows as a line running the other way. `PluginLabSignals` writes it at -6 and -1 dBFS (25 files per length).

### W6.2 linear references (0.18.0, done)
`src/reference/` builds the static library `pluginlab_reference` (namespace `pluginlab::reference`, only `juce_audio_basics`). Every processor derives from
`LinearProcessor`: `processSample` / `process(buffer)` in double precision with a state per channel, and `getResponse(f)`, its exact H(e^jw).
- `Analog.h`: the RBJ filter types and their analog prototypes (s-domain, no discretisation); `getWarpedFrequency` (bilinear transform with pre-warping).
- `Biquad.h`: coefficients, response, stability, `BiquadCascade` (transposed direct form II).
- `Designs.h`: RBJ (all 9 types, Q from bandwidth or shelf slope), Orfanidis peak (prescribed Nyquist gain, after his `peq.m`), Zoelzer (1st-order shelves
  in the all-pass form, 2nd-order shelves, peak; cut = inverse of boost), Butterworth 1-8 and Linkwitz-Riley 2/4/8 with their analog responses.
- `StateVariableFilter.h`: the TPT state-variable filter (Simper's form), all 9 types from one structure, parameters can change every sample.
- `DirectFormFilter.h`, `LinearPhaseFir.h`, `Delays.h`: a filter of any order with an integer pre-delay; the linear-phase FIR from a target magnitude
  (frequency sampling, Blackman window, symmetric taps); integer, Thiran and Lagrange delays.

`tests/ReferenceTests.cpp`, at 44.1, 48 and 96 kHz. What the tests showed (largest errors over 40 frequencies from 20 Hz to 0.98 Nyquist):
- Processing against H(e^jw) (impulse response of 2^17 samples, DTFT): below 3e-9 dB and 1e-8 degrees for all filters (the plan asked for 0.001 dB / 0.01 degree).
- RBJ, Zoelzer, Butterworth and Linkwitz-Riley against their analog prototypes at the pre-warped frequency: below 3e-9 dB, i.e. the cookbook formulas are the
  bilinear transform of the published prototypes (an independent check of the formulas; the numeric comparison with the Python prototype follows in W6.5).
  Butterworth |H|^2 = 1/(1 + W^2N) within 1e-12; Linkwitz-Riley LP + (-1)^(N/2) HP is an all-pass within 1e-12; Zoelzer cut = -boost in dB within 1e-9.
- The state-variable filter realises exactly the RBJ transfer function of every type (below 1e-9 dB); modulated every sample (100 Hz ... 10 kHz at 200 Hz,
  Q 10) it stays bounded (peak 4.9 for noise of peak 1).
- RBJ bandwidth in octaves is an approximation: 1 octave asked gives 0.9997 octaves at 1 kHz but 0.988 at 10 kHz (44.1/48 kHz). Teaching point.
- Orfanidis hits G at f0, 1 at DC and the analog gain at Nyquist; its largest deviation from the analog peak is 0.6 dB where RBJ has 3.3 dB (10 kHz, +12 dB,
  Q 1, 48 kHz). Limit: the band must lie below Nyquist; for 15 kHz with Q 0.707 at 44.1 kHz Orfanidis was worse than RBJ (5.3 dB against 4.6 dB).
- Linear-phase FIR with 2047 taps for the RBJ peak magnitude (1 kHz, +6 dB, Q 1): within 0.011 dB from 100 Hz to 20 kHz, phase exactly linear, latency 1023.
- Thiran (order 1-4) |H| = 1 within 1e-12 and the delay at DC within 1e-6 samples; Lagrange (order 1-4) the delay and gain 1 at DC.

Open: a leak report of the test program at exit ("4 instances of AudioPluginFormat", JUCE assertion), already there before W6.2 and not part of it; to be looked at
separately.

### W6.3 nonlinear and utility references (0.21.0, done)
In `pluginlab_reference`. A common base `Processor` (`reset`, `process(buffer)` with all channels at once, `getLatencySamples`) for all reference processors;
`LinearProcessor` (W6.2) now derives from it. The noise generator of `pluginlab_signals` became a streaming class `NoiseGenerator` (same samples as before)
with the theoretical RMS of its raw output, so that a noise adder can be scaled without measuring.
- `Nonlinear.h`: `Waveshaper` (polynomial, hard clipper, tanh soft clipper) with the harmonics of a sine: closed form for the polynomial
  (cos^n expansion) and the hard clipper (Fourier series of the clipped sine), numerical integration for any curve; `Quantizer` (N bits, mid-tread,
  TPDF dither of +-1 step optional) with the expected SNR 6.02 N + 1.76 dB + 20 log A (4.77 dB less with dither).
- `Utility.h`: `Gain` (dB, polarity), `ChannelMatrix` (2 x 2; crosstalk, M/S width), `DcOffset`, `HumAdder` (50/60 Hz plus harmonics at relative levels),
  `NoiseAdder` (colour, RMS level, seed per channel), `Tremolo` (exact gain curve 1 - depth (1 - cos) / 2).

`tests/NonlinearReferenceTests.cpp`. What the tests showed:
- Polynomial: closed form = numerical integration within 1e-12; the processed sine matches within 0.001 dB (harmonics above -80 dB re H1).
  Example y = x + 0.1 x^2 + 0.05 x^3 + 0.02 x^5 at A = 0.5: H2 -32.13 dB, H3 -49.17 dB, H5 -82.23 dB re H1, DC 0.0125.
- Hard clipper: the Fourier series = numerical integration within 1e-9; processed within 0.001 dB. Teaching point found on the way: a sampled clipper
  aliases; with fs/f0 = 612/13 its harmonics near 600 folded back onto the 7th harmonic by 0.004 dB. The test uses fs/f0 = 4755/101 (aliases
  land on harmonics only near harmonic 4755). At 6 dB over the threshold: H3 -10.3 dB, H5 -16.3 dB re H1.
- tanh(4 x): no even harmonics, small-signal gain = drive, THD 0.33 % / 4.6 % / 17.3 % / 30.1 % at A = 0.05 / 0.2 / 0.5 / 1.
- Quantizer: SNR within 0.1 dB of the formula at 8, 12 and 16 bits, with and without dither (e.g. 16 bits: 98.05 dB against 98.09 dB; dithered
  93.31 dB against 93.32 dB). A sine of 2 steps at 8 bits: H3 -34 dB re H1 without dither, at the noise floor (-48 dB, one DFT bin) with dither.
- Gain, channel matrix, DC, hum and tremolo exact (float rounding); hum and tremolo continue across blocks. Noise adder at -30 dBFS RMS: white within
  0.01 dB and correlation below 0.003; pink within 0.3 dB and correlation -0.06 (the slowest Voss row changes only every 2^15 samples, so 10 s of pink
  noise holds few independent low-frequency values; the tolerances for pink are wider).

### W6.4 reference plugins (0.22.0, done on Linux)
Author's decisions (2026-10-06): both, test plugins and real plugins; the real ones made with the AdvancedAudioTemplate in the simple form.
`plugins/reference/` (details, controls and the changes to the template code in `plugins/reference/README.md`):
- **PluginLab Reference EQ**: one band, algorithm (RBJ, Orfanidis, Zoelzer, state variable, Butterworth, Linkwitz-Riley) x type; combinations an
  algorithm does not offer pass the audio unchanged and the GUI says so; the GUI draws the exact magnitude response (library, 48 kHz).
- **PluginLab Reference Nonlinear**: drive -> curve (polynomial, hard clip, tanh) -> output -> hum -> noise -> quantizer (optional TPDF dither).
- **PluginLab Reference Utility**: gain/polarity -> delay (integer, Thiran or Lagrange order 3) -> channel matrix (width, crosstalk) -> tremolo -> DC.
All: latency 0 (the template's `SynchronBlockProcessor` with block size 0 processes the host blocks directly), no smoothing, mono or stereo.
Compromise: a parameter change rebuilds the stage it belongs to (memory allocation on the audio thread at a change, none in the steady state).
Not in this step: the linear-phase FIR as an EQ algorithm (its latency would have to change with the algorithm at run time; later if needed).

Tests and results:
- `tests/ReferencePluginTests.cpp` (end to end: the host scans and loads the VST3, sets the parameters by their text, processes): RBJ peak 1 kHz +6 dB
  Q 2 at 48 kHz, SVF high shelf at 44.1 kHz and the not offered Orfanidis shelf identical to the library response (0.000000 dB); Butterworth LP order 5
  at 96 kHz within 0.00001 dB; the polynomial harmonics within 0.001 dB of the closed form; 8 bits: SNR 49.74 dB against 49.65 dB; Utility -6 dB,
  inverted, Thiran 37.5 samples identical; width 0 gives L = R; reported latency 0.
- pluginval (strictness 5 on the Debug build, 1 on the Release build): all runs SUCCESS, no assertions; no compiler warnings (template glue included).
- Fingerprint (`docs/fingerprints/PluginLab_Reference_*.md`): the EQ is clean (latency 0 = measured at all rates, all 4 delivery ways, block size
  independent, deterministic, no findings). Nonlinear and Utility are reported time-varying, channel-coupled and not silent for silence: correct, because
  the fingerprint's setting B (every parameter at 0.75) switches on noise, hum and dither, resp. tremolo, DC, width and crosstalk; the fingerprint
  found exactly what was switched on.
- Installed (Release) into `~/.vst3` for the author's review in a DAW. Not tested: Windows, macOS, a DAW session.

### W6.5 oracle (0.24.0, done)
`measurement_tool/tools/export_oracle.py` (Python repository, 0.7.0) writes `tests/oracle/*.json`; `tests/OracleTests.cpp` reads every file and compares
(list of cases and where each answer comes from: `tests/oracle/README.md`). Results (C++ against the oracle):
- RBJ peak, low shelf (44.1 kHz), high shelf, low-pass (96 kHz), notch: 1e-11 dB or better against **scipy's bilinear transform of the analog prototypes**;
  Butterworth order 5 at 96 kHz against `scipy.signal.butter`: 3e-13 dB; Thiran 37.25 samples: coefficients within 1e-12, response 4e-14 dB, group delay
  37.25 samples at 100 Hz and 1 kHz as scipy.
- The prototype's own **Farina sweep measurement** of the RBJ peak deviates by 0.0029 dB at most from the exact C++ response (50 Hz ... 15 kHz).
- The prototype's **THD measurement** of the polynomial equals the closed form of the C++ library (harmonics and THD -32.0344 dB, below 1e-5 dB apart).
- 16-bit quantizer: SNR 98.0485 dB in both (the formula 6.02 N + 1.76 + 20 log A gives 98.089 dB: the error of a deterministic sine is not quite uniform).
- Synchronized sweep and deconvolution, same procedure in numpy (double FFT) and C++ (float FFT): 9e-7 dB, 3e-6 degrees. Teaching point found on the way:
  the first 8192 samples of the deconvolved impulse response give -0.66 dB at 100 Hz and 10 kHz for a filter that has about 0 dB there: the
  band-limited (30 Hz ... 20 kHz) impulse rings before time 0 (wrapped to the end of the buffer) and is cut; the measurement units of W7 must window it.

Review of the author's reservation ("a little unsure, but no better idea"): for the linear filters the oracle confirms what the analytic tests of W6.2 already
showed, but by a really independent route (scipy designs, not my formulas), cheaply. Its real use is where only a numerical answer exists: the sweep
deconvolution here, and in W7 the measurement units (windowing, THD+N bands, noise weighting), where the prototype's measurement code is the second opinion.
Proposal: keep it, add cases with W7.

### Fix 0.24.1: Lagrange fractional delays of odd order
Found with the teaching figure of W6.6: `makeLagrangeDelay` placed the fractional delay of odd orders outside the middle interval of the taps (order 1 at
10.5 samples: D = -0.5, an extrapolation), so the magnitude rose above 0 dB (order 3 by 0.8 dB). Now odd orders use [(N-1)/2, (N+1)/2); a new test checks
that a Lagrange delay never has a gain above 1. The tests at DC (delay and gain) had passed before. The Reference Utility uses Lagrange order 3 from 1 sample on.

### W6.6 teaching pages (0.25.0, done)
`docs/teaching/references/` (CC BY-SA 4.0): an index and ten pages (RBJ cookbook, cramping and Orfanidis, Zoelzer, state-variable filter, Butterworth and
Linkwitz-Riley, linear-phase FIR, fractional delays, waveshapers, quantizer and dither, utilities), each with the formulas, the parameters, the known
answer with the numbers the tests found, things to try with the reference plugins, and figures. The 17 figures (SVG) come from
`apps/teaching` (`PluginLabTeachingFigures docs/teaching/references/figures`), computed with `pluginlab_reference`; chart style after the dataviz
method (palette order, 2 px lines, hairline grid, neutral ink, legend for two or more series, at most four series per chart), checked by eye
(rendered with Inkscape). The figure of the fractional delays found the Lagrange bug fixed in 0.24.1. Not done: a page for the test signals
(sweep, stepped sine); the formulas use GitHub's math syntax.

## 5. The questions and the author's answers (kept for the record)
1. **Filter families:** RBJ, Orfanidis, Zölzer, analog prototypes, Butterworth/Linkwitz-Riley, linear-phase FIR and delays: enough, too much, something missing (e.g. Vicanek's matched
   biquads, state-variable filter / TPT/ZDF designs as many modern plugins use them)?
   Answer: add state variable filters in the first step, we can add more later if needed.


2. **Nonlinear set:** polynomial, clippers, quantizer, noise: enough for W7? Dynamics only with W11?
Answer: yes
3. **Signals and levels:** the list and the standard levels of section 2 (-20 dBFS RMS noise, -6 and -20 dBFS peak sines/sweeps), the sweep from 10 Hz?
Answer: I would start 30 Hz and have 10 Hz in reserve, if something needs it. The levels are fine, we can add more later if needed.
4. **Oracle:** JSON per case, made by a script in the Python repository, committed in `tests/oracle/`: agreed? Who runs the script when the prototype changes (me on request)?
Answer: I am a little unsure here, but have no better idea.
5. **Reference plugins:** three plugins with an algorithm choice (my proposal), or one plugin per algorithm?
Answer: three plugins with an algorithm choice is fine, we can add more later if needed.
6. **Order:** W6.1 and W6.2 first (W7's frequency response needs them), W6.3 before THD/noise, W6.4 and W6.5 can run in parallel?
fine planning:

## Comments:
for sweep signals we should not use Farina, but the slightly better signals from Novak et al. You can find the papers here <../../../../Lehre/AkuMess_Master/NonLinearMEasurement/Paper/LTI SweptSIne Measurement wit h NL/Novak et al SweptSIne, Theroie, Application and Implmentation18042.pdf> and here <../../../../Lehre/AkuMess_Master/NonLinearMEasurement/Paper/LTI SweptSIne Measurement with NL/Novak Nonlinear measurement with exp sinesswept.pdf>

if you look at the measurement methods of Audio Precision, they use step sines coming from 20 kHz down to 20 Hz log-spaced. The length is determined by the latency of the device and the settling time of the measurement procedure (e.g THD need a little longer than transfer function). This method is very reliable, but will only measure the magnitude not the phase. However it is also very good for THD, THD+N and Crosstalk.

For the samples: We should include
* drum loops
* guitar loops
* piano excerpt
* vocal excerpt
* complete mixes (excerpt)
