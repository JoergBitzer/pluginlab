# W11.1: delay effects (plan)

Status: **plan v1** (2026-10-18, for review; questions for the author in section 9). Part of W11 (`planning.md` §8: dynamics, delay, reverb after
the EQ). The author's order: delay first ("it should be the easiest"), then reverb (`W11-reverb-plan.md`), then dynamics (`W11-dynamics-plan.md`).

## 1. What W11.1 has to answer
For a delay plugin, the questions of the three readers:
- **Musician**: is this delay like mine (time, number and darkening of the repeats, the wobble of a tape echo), and how do I set it to sound the same?
- **Engineer / teacher**: what does the delay do exactly: the echo times, the feedback per repeat, the filter in the loop, the stereo pattern, the
  modulation, the saturation, the tempo sync; is it what the knobs say?
- **Developer**: does my delay keep its time at every sample rate and block size, is the feedback stable, does the tempo sync follow the host,
  does it click when the time knob moves, is the reported latency right?

## 2. The class: what delay plugins have
| Kind | Typical knobs | System class (W8.1) |
|---|---|---|
| clean digital delay | time (ms or note value), feedback, mix, low/high cut in the loop | LTI (at a fixed setting) |
| multi-tap, ping-pong | taps, pan per tap, cross feedback | LTI, two-channel (a 2 x 2 response) |
| tape echo (Echoplex, Space Echo models) | time, feedback, wow and flutter, saturation, tape age | time-variant and non-linear |
| bucket-brigade (BBD) models | time, feedback, modulation, noise | time-variant (clock modulation), band-limited, noisy |
| modulated delay, chorus/flanger region (< 30 ms) | rate, depth, shape | time-variant (see the boundary in 8) |
| ducking delay | threshold, amount | non-linear (dynamics on the wet path) |
| reverse, pitch, shimmer | | time-variant; outside W11.1 at first |

The system class goes first, as everywhere: it decides which of the measurements below mean something.

## 3. What exists to build on
- The synchronized swept sine (Novak, Lotton, Simon 2015) and the impulse response with its window (`SweptImpulse`, W7.2): an LTI delay is fully
  described by one long impulse response.
- Delay and polarity (W7.3), the null test with fractional alignment (W7.8; Knapp and Carter 1976), the spectrogram (W8), the system class
  (W8.1), the fingerprint's latency measurement, the real-time and block-size tests (W5c), zipper noise (0.65.0, the time knob), the matcher's
  general layer (W9.1), the reports and pages.
- What is missing: renders of several seconds with the tail (the fingerprint and the units use short windows), the host's tempo and transport
  (`AudioPlayHead`: a tempo-synced delay needs the BPM), and the stereo 2 x 2 measurement as one unit.

## 4. Measurements
### 4.1 The echo response (LTI delays)
- Stimulus: the synchronized sweep with **silence after it longer than the echo train** (the deconvolution must see the whole tail; the length
  comes from a first impulse: render an impulse until the output stays below -100 dBFS for 1 s, at most 30 s; feedback near 100 % is cut there
  and reported as "does not decay within 30 s").
- From the impulse response: the **echoes** by peak picking on the envelope (Hilbert magnitude, smoothed), refined by fitting the first echo's
  shape to each later one (a matched filter): per echo the arrival time (fractional, as in the null test), the gain (dB), the polarity.
- Derived: the **delay time** (the spacing; the first echo against the dry path), its constancy (the spacing of all echoes), the **feedback** as
  the level ratio of successive echoes (dB per repeat; and the decay of the train in dB per second, a "T60 of the echoes"), the **mix** (the
  direct path at the latency against the first echo), the reported latency against the measured one.
- The **loop filter**: each echo windowed (a few ms around it, as wide as its spread allows), its spectrum; the ratio of the spectra of successive
  echoes is the transfer function of one trip through the loop (low and high cut, the "darkening per repeat"). Averaged over the echoes that are
  above the noise.
- **Tempo sync**: the same at two host tempi (e.g. 120 and 90 BPM through the play head): the delay time must scale with 60/BPM for a note value.
- **Sample rate and block size**: the echo times at 44.1/48/96 kHz and block sizes (the fingerprint's pattern): a delay that rounds to samples or
  blocks shows it here.

### 4.2 Stereo
- The 2 x 2 response: left input only, right input only, each to both outputs (the fingerprint's coupling test, extended to the echo train).
- Ping-pong: the echoes alternate between the channels; multi-tap: several echoes per period with their pans; cross feedback.

### 4.3 Time variance: the delay trajectory
A modulated delay (tape, BBD, chorus region) has a delay time that moves: tau(t).
- **Impulse train**: impulses spaced wider than the delay plus its echo spread; the arrival time of the first echo of each impulse samples
  tau(t) (simple, robust, resolution = the spacing).
- **Steady tone** (3150 Hz, the wow and flutter tone of AES6-2008 / IEC 60386): the output's instantaneous frequency f_out(t) = f (1 - dtau/dt)
  by demodulation; integrated it gives tau(t) at sample resolution; the weighted peak flutter of AES6 as a number for tape models.
- **Chirp train** (Wright and Välimäki, DAFx 2020; JAES 2021): short chirps at regular intervals recover the LFO of phasers and flangers; the same
  for modulated delays.
- From tau(t): the modulation rate, depth (ms and cents of the pitch deviation), shape (sine, triangle, random), drift. The echo response of 4.1 is
  then measured in the "frozen" sense only where the modulation can be switched off; otherwise the echo times are given as mean and spread.

### 4.4 Non-linearity
- The echo response at three levels (-40, -20, -6 dBFS, as the system class): an echo gain that falls with the level is saturation in the loop;
  the harmonic responses of the sweep (Farina 2000; Novak 2015) per echo.
- Self-oscillation: feedback at or above 100 % grows; the measurement stops at a level limit and reports it (a safety limit on the output in all
  W11 renders).
- Ducking: the wet level during and after a loud burst (the dynamics methods of `W11-dynamics-plan.md`, on the wet path).

### 4.5 Noise and band limits
The idle noise (W7.7) with the feedback up (BBD and tape hiss builds up in the loop), the band limit of the wet path (the loop filter at the
first echo).

## 5. Comparing two delays (and matching)
- Echo by echo: time differences (ms, and % of the period), level differences (dB), loop filter difference (dB over frequency), stereo pattern
  (same / different), modulation (rate, depth, shape).
- The null test on the first N echoes after alignment (W7.8), as a single number of how close; the time-domain null is strict (a few samples
  of difference in the delay time break it), so it comes after the parameter comparison.
- **Matching** (the general matcher, W9.1): a new "echo domain" with the features delay time, the levels of the first echoes, the loop filter
  curve; released parameters time, feedback, mix, tone (from a mapping, as for EQs). For an LTI delay the fast render path (W9.3) applies with a
  longer impulse; for tape models the features are the means over the modulation.
- Perception: the audibility of timing and level differences of echoes is to be checked in the literature before a "the same / audibly close"
  scale is fixed (for rhythmic delays a few milliseconds matter; for level the 1 dB of the EQ work is a start).

## 6. Visualising
- **Echogram**: the envelope in dB over time, the detected echoes as stems with their numbers (time, level), one colour per output channel.
- **Echo table**: per echo time, level, polarity, the spacing to the previous one.
- **Loop filter plot**: the magnitude of one trip through the loop; and a "waterfall" of the echo spectra (each echo's spectrum in the colour map
  of the spectrogram, stacked by echo number): the darkening per repeat at a glance.
- **Stereo lanes**: left and right as two lanes of stems (ping-pong visible as alternation).
- **Delay trajectory**: tau(t) over a few seconds for modulated delays, with rate and depth.
- The spectrogram of the sweep response (exists) shows echoes as repeated sweeps.

## 7. Reference plugin (ground truth)
**Reference Delay**: time (ms or note value with the host tempo), feedback, mix, one-pole low and high cut in the loop, ping-pong, an LFO on the
time (rate, depth, sine/triangle), a tanh saturation in the loop, fractional delay by an all-pass or Lagrange interpolation (switchable: the
interpolation leaves its mark in the loop filter). Every measurement of 4 has an exact expected value; tests as for the Reference EQ.

## 8. Work packages
| Step | Content | Done when |
|---|---|---|
| W11.1a | long renders with the tail (tail detection, output safety limit), the host tempo and transport in the rig | a render of the Reference Delay returns the whole echo train; the play head's BPM reaches the plugin |
| W11.1b | echo response: echoes, delay time, feedback, mix, loop filter, latency | the Reference Delay's values within 0.1 sample, 0.05 dB, the loop filter within 0.1 dB |
| W11.1c | stereo 2 x 2, ping-pong, multi-tap | ping-pong and cross feedback of the reference recognised |
| W11.1d | delay trajectory (impulse train, tone demodulation, chirp train), AES6 flutter | the reference LFO's rate and depth within 1 % |
| W11.1e | level dependence and saturation, self-oscillation guard | the reference's saturation shows as level-dependent echo gains |
| W11.1f | report section "Delay" (fingerprint and --measure), Developer page column, the views of 6 | readable report for the reference and two real delays |
| W11.1g | comparison and the echo domain of the matcher | the reference matched to itself with another start; two real delays compared |

The boundary to modulation effects: chorus and flanger (delays below about 30 ms with an LFO) use the trajectory methods of 4.3 but are no
"delays" for the musician; proposal: they get their own small package after W11 (with phaser), reusing 4.3.

## 9. Questions for the author
1. **Which delays** form the test set (free ones: e.g. a clean digital delay, a tape echo model, a BBD model, a ping-pong)?
2. **Tempo sync**: the rig gives the plugin a play head with a tempo; is 120 BPM as the standard tempo right, plus one other for the scaling test?
3. **Chorus and flanger**: in W11.1 or a later package (proposal: later, reusing the trajectory methods)?
4. **Matching**: is matching delays wanted (time, feedback, tone), or only the comparison?
5. **Time knob zipper**: a delay whose time knob moves produces pitch glides (tape) or clicks (digital): measure it as "zipper noise" (0.65.0)
   or as its own behaviour ("time change: glide / jump / crossfade")?

## 10. References
- A. Novak, P. Lotton, L. Simon, "Synchronized swept-sine: theory, application, and implementation," JAES 63(10), 2015.
- A. Farina, "Simultaneous measurement of impulse response and distortion with a swept-sine technique," AES 108th Convention, Paris, 2000,
  preprint 5093.
- C. H. Knapp, G. C. Carter, "The generalized correlation method for estimation of time delay," IEEE Trans. ASSP 24(4), 1976.
- S. Arnardottir, J. S. Abel, J. O. Smith III, "A digital model of the Echoplex tape delay," AES 125th Convention, San Francisco, 2008, paper 7649
  (the measured delay-time trajectory of a tape echo).
- A. Wright, V. Välimäki, "Neural modelling of periodically modulated time-varying effects," DAFx-20, 2020, pp. 281-288; "Neural modeling of
  phaser and flanging effects," JAES 69, 2021 (chirp trains to recover the LFO).
- C. Mitcheltree, C. J. Steinmetz, M. Comunità, J. D. Reiss, "Modulation extraction for LFO-driven audio effects," DAFx-23, 2023
  (arXiv 2305.13262).
- AES6-2008, "Method for measurement of weighted peak flutter of sound recording and reproducing equipment" (technically identical to
  IEC 60386:1972 with its 1988 amendment): the 3150 Hz tone and the weighting.
- V. Välimäki, J. Parker, L. Savioja, J. O. Smith, J. S. Abel, "Fifty years of artificial reverberation," IEEE TASLP 20(5), 2012 (delay lines,
  comb filters and their measurement as the building blocks).

Sources found 2026-10-18:
[AES E-Library: Echoplex model](https://aes2.org/publications/elibrary-page/?id=14800),
[Aalto: periodically modulated time-varying effects](https://research.aalto.fi/en/publications/neural-modelling-of-periodically-modulated-time-varying-effects/),
[arXiv 2305.13262](https://arxiv.org/pdf/2305.13262),
[AES Standards Store: AES6](https://aes2.org/standards-store/?id=15),
[AES E-Library: Farina 2000](https://aes2.org/publications/elibrary-page/?id=10211).
