# W7: measurement units (plan)

Status: **plan, W7.1 started** (author's request 2026-10-08: "for each measurement unit a detailed file.md explaining the measurement routine, the scientific
source (norms and standards if possible) and results for known test signals"). Planning §8, W7: "Measurement units (frequency response, phase, group delay,
latency, THD, THD+N, noise, SNR, crosstalk, null test with alignment), easy to add new ones. Done when: each agrees with analytic ground truth and with the
oracle files within a stated tolerance; band of validity documented."

## 1. The basis: AES17-2015
**AES17-2015, "AES standard method for digital audio engineering - Measurement of digital audio equipment"** (the author's copy:
`~/Lehre/AMT/Standard Audio unit MEasurement aes17-2015-f.pdf`) describes nearly every unit of W7 for digital-to-digital equipment, which is what a plugin is.
Its normative references are ITU-R BS.468-4 (noise weighting) and IEC 61260-1 (fractional-octave filters; a copy of the older IEC 1260 is in
`~/Downloads/iec1260_nur_engl.pdf`). What AES17 fixes and pluginlab adopts:
- **Levels** (3.12): dBFS is an **rms** level relative to a full-scale **997 Hz** sine (a full-scale sine is 0 dBFS, a full-scale square wave +3.01 dBFS).
  Our signal generators give sine levels as peak dBFS; for a sine both numbers are equal.
- **Passband** (3.4, 4.3): 20 Hz to the upper band-edge frequency, 20 kHz at 44.1 and 48 kHz.
- **Analyzer** (5.2): true-rms level meter integrating at least 25 ms and whole periods (5.2.3); standard low-pass filter (5.2.5: passband +-0.1 dB to 20 kHz,
  stop band >= 60 dB above 24 kHz); standard notch (5.2.8: Q 1.2 ... 3); CCIR-RMS weighting = ITU-R BS.468-4 - 5.63 dB (5.2.7, table 1); standard band-pass
  (5.2.9, IEC 61260-1); frequency-domain band-pass ("window-width") filters (5.2.10, annex B). Accuracy three times better than the specification (5.2.11).
- **Methods** (clause 6) and the fast FFT methods (annex A: synchronous multitone A.3, exponential sweep A.4).

Plugins are digital-to-digital: analogue impedances, power supply, jitter and interface clauses do not apply. AES17's "maximum input level" (6.2.1) is the
reference of many levels; for a plugin it is 0 dBFS unless the unit "maximum input level" (W7.9) measures less.

## 2. Architecture
- New library **`pluginlab_measure`** (`src/measure/`, namespace `pluginlab::measure`), depending on `pluginlab_signals` (stimuli) and `pluginlab_reference`
  (only in the tests), no hosting, no GUI.
- **Device**: what is measured, as a function "render this buffer at this sample rate from a fresh state" (`Device`). Adapters: a reference processor
  (W7.1), a hosted plugin with the delivery protocol of the fingerprint (W7.10, engine side), later the host's slots.
- **Analyzer** (W7.1): level meter, standard low-pass filter; later notch, weighting (CCIR-RMS, A-weighting after IEC 61672-1 as an addition),
  frequency-domain band-pass, octave/third-octave bands.
- **Unit**: one function per measurement, `measureX(device, settings) -> XResult`; the result carries the numbers, the unit, the band of validity and the
  clause it follows. Easy to add a unit = a new file pair + its document + its tests.
- **Documentation**: `docs/measurements/<unit>.md`, one per unit, same structure: purpose; standard and sources; stimulus; routine step by step;
  analysis with formulas; band of validity and limits; results for known test signals (from the tests, with the numbers); implementation (files, API).
- **Tests**: every unit against the reference processors of W6 (analytic answer) and, where the prototype has the same measurement, against an oracle
  file (W6.5); tolerances stated in the test and in the document.

## 3. The units (sub-work-packages)
| Step | Unit | Standard / sources | Known test signals and answers |
|---|---|---|---|
| **W7.1** | **foundation + level and gain** (and gain matching between channels): library, device, level meter, standard low-pass filter, document template | AES17 3.12, 5.2.3, 5.2.5, 6.2.2, 6.2.4 | Gain of -6 dB, polarity inverted, RBJ peak at 997 Hz (exact), a channel 0.5 dB lower; oracle: the prototype's lock-in gain |
| W7.2 | **frequency response** (magnitude): stepped sine (AES17 6.2.3, the AP method), synchronized sweep (annex A.4; Novak et al. 2015), multitone (annex A.3) | AES17 6.2.3, A.3.4, A.4.5; Novak, Lotton, Simon JAES 2015; Farina AES 108th conv. 2000 | RBJ, Butterworth, Linkwitz-Riley, FIR: within 0.01 dB of H(e^jw); the three methods against each other |
| W7.3 | **delay (latency) and polarity** | AES17 6.8.2 (impulse response and cross-correlation), 6.2.8; Knapp and Carter 1976 (GCC) | integer and fractional delays (Thiran, Lagrange), inverted gain, linear-phase FIR (127) |
| W7.4 | **phase response and group delay**, inter-channel phase | AES17 6.8.3, 6.8.4, 6.2.7, A.4.8 | all-pass, Butterworth, FIR (linear phase), Thiran |
| W7.5 | **THD+N and THD** (vs level, vs frequency), harmonics by the sweep | AES17 6.3.1-6.3.3, A.4.7; IEC 60268-3; Novak et al. 2015 | polynomial (closed form), hard clip (Fourier series), quantizer; oracle: the prototype's THD |
| W7.6 | **intermodulation**: difference frequency (19/20 kHz) and modulation distortion (41 Hz + 7993 Hz, 4:1) | AES17 6.3.5, 6.3.6; IEC 60268-3; SMPTE RP120 | polynomial: closed-form IMD products |
| W7.7 | **noise**: idle channel noise, dynamic range (SNR at -60 dBFS), mains products; CCIR-RMS and A weighting | AES17 6.4.1, 6.4.2, 6.5.1, 5.2.7; ITU-R BS.468-4; IEC 61672-1 | noise adder (known level and colour), quantizer (6.02 N + 1.76), hum adder (known lines) |
| W7.8 | **crosstalk** and gain matching | AES17 6.5.2, 6.2.4, A.3.8 | channel matrix with known crosstalk and width |
| W7.9 | **maximum input level and gain non-linearity** | AES17 6.2.1, 6.3.7 | hard clipper (exact threshold), tanh (compression), quantizer |
| W7.10 | **null test with alignment** (difference of two devices after delay and gain alignment) | no standard; Knapp and Carter 1976; Laakso et al. 1996 (fractional delay) | two reference EQs that differ by a known amount; the same plugin twice |
| W7.11 | **plugins and host**: the plugin device (delivery protocol), the units on the host's Developer page and in reports | - | the reference plugins: measured = library answer |

Order: W7.1 first (it fixes the conventions), then W7.2 ... W7.4 (linear; W8 needs them), W7.5 ... W7.9, W7.10, W7.11. Each step: design note in this file,
code, tests, document, version (minor), commit.

## Progress
### W7.1 foundation, level and gain (0.26.0, done)
`src/measure/` builds `pluginlab_measure`: `Device` (+ `makeProcessorDevice`), the analyzer (`rmsToDbfs` after AES17 3.12, `StandardLowPass` after 5.2.5,
`getRms`, `getToneAmplitude`, `getWholePeriodLength`) and the unit `measureGain` (broadband gain after AES17 6.2.2 through the standard low-pass filter,
selective gain and phase at the test frequency, gain matching after 6.2.4). Documents: `docs/measurements/README.md` (conventions, analyzer) and
`docs/measurements/level-and-gain.md` (the template for the other units). Tests `tests/MeasureGainTests.cpp`: exact devices within 1e-4 dB at 44.1/48/96 kHz,
the standard low-pass meets the specification at 88.2/96/192 kHz (ripple 0.0022 dB, stop band below -69.4 dB), closed forms for a polynomial and a
dithered quantizer; oracle case `gain_rbj_peak_997_48k` (export script 1.1.0): the prototype's lock-in gain and phase agree.
Two expectations were corrected on the way: the reported input level is the level as generated (the filter ripple of 0.0009 dB at 997 Hz showed at 96 kHz);
an undithered quantizer moves even the selective gain (0.03 dB at 8 bits: the error is correlated with the sine), so the known answer uses TPDF dither.

