# W11.4: modulation effects - chorus, flanger, phaser (plan)

Status: **plan v1** (2026-10-19, for review; questions for the author in section 9). The author: "Flanger and Chorus and even Phaser are a
different class of effects" (answer to `W11-delay-plan.md`, question 3). Written during the long builds of W10.

## 1. What W11.4 has to answer
- **Musician**: does this chorus / flanger / phaser move like mine: how fast, how deep, how many "voices" or notches, how much whoosh
  (feedback), how wide in stereo?
- **Engineer / teacher**: the LFO (rate, shape, depth, phase between the channels), the delay range or the notch range, the number of stages,
  the feedback and its sign, the mix; the time-varying transfer function made visible.
- **Developer**: is the LFO rate exact and independent of the sample rate and the block size; does it sync to the host tempo; does the
  modulation alias or zip; is it stable at full feedback?

## 2. The class
| Effect | What moves | Typical knobs | Signature |
|---|---|---|---|
| chorus | one or more delays of 5 ... 30 ms, modulated | rate, depth, voices, spread, mix | detuned copies (pitch deviation of a few cents), widening |
| flanger | one short delay (0.1 ... 10 ms), modulated, with feedback | rate, depth, manual (delay), feedback (+/-), mix, through-zero | a comb filter whose notches sweep (spacing 1/tau) |
| phaser | a chain of first-order (or second-order) all-passes whose break frequencies move | rate, depth, stages, centre, feedback, mix | a few notches that sweep, not harmonically spaced |
| vibrato | a delay, 100 % wet | rate, depth | pitch modulation only |

All are time-variant (W8.1): no single transfer function; the measurements describe a transfer function that moves.

## 3. What exists to build on
The system class (time variance), the spectrogram (W8), the delay trajectory methods of `W11-delay-plan.md` 4.3 (impulse train, tone
demodulation, chirp train), the long renders and the host tempo of W11.1a, the live curves of W10.3 (here as a "frozen" view), the reports.

## 4. Measurements
### 4.1 The LFO
- **Chirp train** (Wright and Välimäki, DAFx 2020; JAES 2021): short chirps at regular intervals; each chirp's response gives a momentary
  transfer function; the frequency of the notch (phaser, flanger) or the delay (chorus) per chirp samples the LFO. The JAES version uses shorter,
  more closely spaced chirps synthesised in the frequency domain.
- **Steady tone** (for delay-based effects): the instantaneous frequency of the output gives -dtau/dt, integrated tau(t) (as for the tape echo).
- **Impulse train**: for flanger and chorus the arrival time of each impulse's delayed copy samples tau(t) directly.
- From the sampled LFO: rate (against the knob and, for synced LFOs, against the host tempo at 120 and 97 BPM), shape (sine, triangle, other:
  the harmonic content of the LFO signal), depth (ms for delays, octaves of the notch range for phasers), the phase between left and right
  (stereo LFO offset), drift and jitter (an analog-modelled LFO).

### 4.2 The moving transfer function
- **Flanger**: the comb's notch spacing 1/tau(t) and the notch depth; feedback from the peak-to-notch ratio and the sign from where the peaks
  sit (positive feedback: peaks at multiples of 1/tau, negative: notches there); through-zero (tau passes 0: the comb vanishes momentarily).
- **Phaser**: the number of notches (stages / 2 for first-order all-passes), their frequencies over time (the notch range per notch), their
  depth and width; feedback (the notches' sharpness, the resonance between them).
- **Chorus**: the number of voices (the distinct delayed copies of an impulse), their delays and depths, their detuning in cents (from the
  delay slope), the stereo spread.
- **Frozen snapshots**: where the plugin allows the LFO to stop (rate 0, "manual" knob), the transfer function at fixed positions is LTI and
  measured with the EQ tools; the moving case is the sequence of these.

### 4.3 Stereo, mix, side effects
The 2 x 2 response (`W11-delay-plan.md` 4.2), the mix (the direct path against the modulated path), the noise (BBD chorus models), aliasing
of the modulated delay (the spectrum of a high steady tone: images around it), the behaviour at full feedback (self-oscillation guard of
W11.1a).

## 5. Comparing two modulation effects
LFO against LFO (rate, shape, depth, stereo phase), notch trajectories against each other (after aligning the LFO phase), the number of
voices / stages, the feedback; perception: the rate and the depth of the modulation are what listeners hear first; a JND scale for rate and
depth is to be found in the literature before "the same / audibly close" is used. No matching at first (as for the delay).

## 6. Visualising
- **Spectrogram** of noise (or of a chirp train) through the effect: the sweeping notches; the strongest single picture of this class.
- **Notch trajectories** over time (phaser: one line per notch; flanger: the comb's first notches), with the LFO period marked.
- **LFO plot**: tau(t) or the notch frequency over one or two periods, two plugins overlaid, phase-aligned.
- **Voice diagram** for chorus: the delayed copies as stems over time (from the impulse train), left and right.
- **Frozen transfer functions**: the magnitude at the LFO's extremes and middle (a family of curves).

## 7. Reference plugin (ground truth)
**Reference Modulation**: modes chorus (1 ... 4 voices), flanger (with +/- feedback, through-zero), phaser (2 ... 12 first-order stages,
feedback), vibrato; LFO sine / triangle with rate (Hz or note value with the host tempo), depth, stereo phase; a stop switch (frozen LFO at a
chosen phase) for the LTI snapshots. Every measurement of 4 has its exact expected value.

## 8. Work packages
| Step | Content | Done when |
|---|---|---|
| W11.4a | chirp train and LFO extraction; tone demodulation and impulse train (shared with W11.1d) | the reference's LFO rate within 0.5 %, shape recognised |
| W11.4b | flanger: comb, notch trajectory, feedback and its sign, through-zero | the reference flanger's delay range within 0.05 ms, feedback sign right |
| W11.4c | phaser: notches, their range, stages, feedback | the reference phaser's stage count and notch range recognised |
| W11.4d | chorus: voices, delays, detune, spread | the reference chorus's voice count and detune within 1 cent |
| W11.4e | stereo, mix, aliasing, frozen snapshots; report section, views of 6 | readable reports for the reference and two real effects per kind |
| W11.4f | comparison of two effects (no matching) | two real phasers and two flangers compared |

## 9. Questions for the author
1. **Test set**: which free chorus, flanger and phaser plugins (and do you have own implementations in ~/AudioDev to build the reference on)?
2. **Order**: W11.4 after W11.1 to W11.3, or right after the delay (it reuses the delay trajectory methods)?
3. **Rotary speakers (Leslie)**: part of this class (amplitude and Doppler modulation) or later?
4. **Tremolo and auto-pan** (amplitude modulation): in this class (the LFO methods apply) or with dynamics?

## 10. References
- A. Wright, V. Välimäki, "Neural modelling of periodically modulated time-varying effects," DAFx-20, 2020, pp. 281-288 (the chirp train to
  recover the LFO of a phaser and a flanger).
- A. Wright, V. Välimäki, "Neural modeling of phaser and flanging effects," JAES 69, 2021 (shorter, frequency-domain synthesised chirps).
- C. Mitcheltree, C. J. Steinmetz, M. Comunità, J. D. Reiss, "Modulation extraction for LFO-driven audio effects," DAFx-23, 2023
  (arXiv 2305.13262: extracting the LFO from processed audio).
- V. Välimäki, J. Parker, L. Savioja, J. O. Smith, J. S. Abel, "Fifty years of artificial reverberation," IEEE TASLP 20(5), 2012 (delay-line
  structures).
- U. Zölzer (ed.), DAFX - Digital Audio Effects, Wiley, 2nd ed. 2011 (chorus, flanger, phaser, vibrato).
- J. O. Smith III, "Physical Audio Signal Processing," online book, CCRMA (the flanger and phaser as comb and all-pass structures).
