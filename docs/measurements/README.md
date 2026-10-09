# Measurement units

One document per measurement unit of pluginlab (`src/measure/`, library `pluginlab_measure`; plan `docs/design/W7-plan.md`). Every document has the same
parts: purpose; standard and sources; stimulus; routine step by step; analysis with formulas; band of validity and limits; results for known test
signals (the numbers come from the tests); implementation.

| Unit | Document | Standard | Status |
|---|---|---|---|
| level and gain, gain matching | [level-and-gain.md](level-and-gain.md) | AES17-2015 6.2.2, 6.2.4 | W7.1, done |
| frequency response (stepped sine, multitone, synchronized sweep) | [frequency-response.md](frequency-response.md) | AES17-2015 6.2.3, annex A.3/A.4; Novak et al. 2015 | W7.2, done |
| delay and polarity | [delay-and-polarity.md](delay-and-polarity.md) | AES17-2015 6.8.2, 6.2.8; Knapp and Carter 1976 | W7.3, done |
| phase response and group delay, inter-channel phase | [phase-and-group-delay.md](phase-and-group-delay.md) | AES17-2015 6.8.3, 6.8.4, 6.2.7 | W7.4, done |
| THD+N, THD (against level and frequency), harmonics from the sweep | [thd-and-thdn.md](thd-and-thdn.md) | AES17-2015 6.3.1 ... 6.3.3, A.3.6, A.4.7; IEC 60268-3; Novak et al. 2015 | W7.5, done |
| intermodulation: difference-frequency (18 + 20 kHz), modulation distortion (41 + 7993 Hz) | [intermodulation.md](intermodulation.md) | AES17-2015 6.3.5, 6.3.6, 5.2.10, annex B | W7.6, done |
| noise: idle channel noise, dynamic range, mains products; CCIR-RMS and A weighting | [noise.md](noise.md) | AES17-2015 6.4.1, 6.4.2, 6.5.1, 5.2.7; ITU-R BS.468-4; IEC 61672-1 | W7.7, done |
| crosstalk (selective and broadband, every channel driven in turn) | [crosstalk.md](crosstalk.md) | AES17-2015 6.5.2, A.3.8 | W7.8, done |
| maximum input level, gain non-linearity | (W7.9) | AES17-2015 6.2.1, 6.3.7 | planned |
| null test | (W7.10) | (no standard) | planned |

## The common basis: AES17-2015
**AES17-2015**, *AES standard method for digital audio engineering - Measurement of digital audio equipment*, Audio Engineering Society, New York 2015.
It is written for converters and digital equipment; a plugin is digital-to-digital equipment ("EUT", equipment under test), so the clauses on analogue
impedances, power supplies, jitter and digital interfaces do not apply. The conventions pluginlab takes from it:

- **dBFS is an rms level** relative to a full-scale sine (3.12): $L = 20\lg(x_\text{rms}\sqrt2)$ dBFS. A full-scale sine (amplitude 1) is 0 dBFS, a full-scale
  square wave +3.01 dBFS. For a sine, rms dBFS and peak dBFS are the same number (the signal generators of `pluginlab_signals` give peak levels).
- **997 Hz** is the standard test frequency (3.12.1, 5.4): not a divisor of the common sample rates, so the sine exercises many different codes. With 1 s of
  signal it still has a whole number of periods at every integer sample rate (997 periods).
- **Passband** 20 Hz to the upper band-edge frequency, 20 kHz at 44.1 and 48 kHz (3.4, 4.3); pluginlab keeps 20 kHz at higher sample rates.
- **Maximum input level** (6.2.1) is the reference of most test levels ("-20 dB relative to the maximum input level"); for a plugin it is 0 dBFS until
  the unit "maximum input level" (W7.9) measures less.
- **The analyzer** (5.2), as far as built:
  - *Level meter* (5.2.3): true rms, integrating at least 25 ms and a whole number of periods (`getRms`, `getWholePeriodLength`).
  - *Standard low-pass filter* (5.2.5): passband +-0.1 dB from 20 Hz to 20 kHz, at least 60 dB of attenuation above 24 kHz (`StandardLowPass`). At 44.1 and
    48 kHz nothing above 24 kHz exists, the filter is the identity. Above, a linear-phase FIR designed with a Kaiser window (Kaiser 1974; Oppenheim and
    Schafer, *Discrete-Time Signal Processing*, ch. 7) for 70 dB: 97 taps at 88.2 kHz, 105 at 96 kHz, 209 at 192 kHz; measured passband ripple 0.0022 dB,
    stop band at most -69.4 dB (test `MeasureGainTests`).
  - *Frequency-domain band-pass* (5.2.10, annex A.2/B): on a whole number of periods a single DFT bin with a rectangular window isolates a tone exactly
    (`getToneAmplitude`); this is the "synchronous" analysis of annex A.
  - Later units add the standard notch (5.2.8), the CCIR-RMS weighting (5.2.7, ITU-R BS.468-4), the standard band-pass (5.2.9, IEC 61260-1).
- **Accuracy** (5.2.11): the analyzer must be three times better than the specification it verifies. The tests below state the accuracy reached against
  devices with an exact answer.

## The device under test
A unit measures a `Device`: a function that renders an input buffer at a sample rate from a fresh state. `makeProcessorDevice` makes one from a reference
processor of `pluginlab_reference` (used by the tests); the plugin device with the delivery protocol of the fingerprint follows in W7.11.

## Known test signals
The units are tested against the reference processors of W6 (`src/reference/`, teaching pages `docs/teaching/references/`), whose answers are known
exactly, and, where the Python prototype has the same measurement, against its oracle files (`tests/oracle/`).
