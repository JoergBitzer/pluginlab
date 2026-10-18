# W11.2: reverb (plan)

Status: **plan v1** (2026-10-18, for review; questions for the author in section 9). Part of W11, after the delay (`W11-delay-plan.md`), before
dynamics (`W11-dynamics-plan.md`). Room acoustics has measured reverberation for a century; most of its methods carry over to a plugin, which
is a room without a noise floor and without a loudspeaker.

## 1. What W11.2 has to answer
- **Musician**: does this reverb sound like mine (length, size, brightness, how soon it starts, how smooth the tail is); how do I set it the same?
- **Engineer / teacher**: decay time per band, early decay, clarity, pre-delay, density build-up, stereo width, coloration, modulation; the room
  parameters of ISO 3382-1 for an algorithm.
- **Developer**: is the decay what the knobs say at every sample rate; is the tail free of metallic ringing and flutter; does the modulation
  break the stereo image; does the freeze hold; is it stable at the extreme settings?

## 2. The class
| Kind | Typical knobs | System class |
|---|---|---|
| algorithmic (Schroeder/Moorer, FDN, plate and spring models) | size, decay, pre-delay, damping (high/low multipliers), diffusion, density, early reflections, width, mix | LTI if not modulated |
| modulated algorithmic | modulation rate and depth | time-variant |
| convolution | the impulse response, length, pre-delay, EQ | LTI |
| gated, reverse, shimmer, freeze | gate time, pitch | time-variant or non-linear |
| spring models | tension, drip | LTI, strongly dispersive (chirps) |

## 3. What exists to build on
The synchronized sweep and its impulse response (W7.2; Farina 2000, Novak et al. 2015), the spectrogram (W8), the system class (W8.1), the null
test (W7.8), the matcher's general layer (W9.1), the reports; from W11.1a: long renders with the tail and the output safety limit.

## 4. Measurements
### 4.1 The impulse response
- Stimulus: the synchronized sweep (ISO 18233 describes sweep methods for room acoustics), long enough and followed by silence longer than the
  tail (from a first impulse, as for the delay; at most 30 s; a freeze or an infinite decay is reported as such).
- Per channel pair (L to L, L to R, R to L, R to R): a stereo reverb is a 2 x 2 system; the stereo width comes from the cross terms.

### 4.2 Decay (ISO 3382-1)
- **Energy decay curve** by Schroeder's backward integration of the squared impulse response (Schroeder 1965), per octave and third-octave band
  (the band filters of IEC 61260). A plugin has no acoustic noise floor, but dither, modelled noise and the numerical floor exist: the truncation
  and its compensation after Lundeby et al. (1995).
- **T20, T30, EDT** per band by the regressions of ISO 3382-1; the curvature of the EDC (two-slope decays: a reverb with a fast early and a slow
  late part).
- **Energy decay relief** (Jot 1992): the EDC per frequency bin from the short-time spectrum: the decay time as a continuous function of frequency
  (what "damping" and "low multiplier" knobs do).
- **Clarity C50, C80, definition D50, centre time Ts** (ISO 3382-1), relative to the direct path at the plugin's latency (or to the first sample
  of the response for a 100 % wet reverb: stated in the report).
- **Pre-delay**: from the input (corrected for the latency) to the onset of the reverb (the first sample above a threshold re the peak, and the
  rise of the envelope).

### 4.3 Texture: density, diffusion, coloration
- **Echo density profile** (Abel and Huang 2006): the normalized echo density over time (1 = Gaussian, like noise); its rise shows the diffusion,
  sparse early echoes, flutter; the breakpoints 0.3 and about 0.7 that listening tests found as perceptually distinct groups.
- **Mixing time**: when the response becomes diffuse; perceptually validated predictors include the echo density profile (Lindau, Kosanke,
  Weinzierl 2012).
- **Coloration**: the fine structure of the late spectrum (the peakiness of the smoothed magnitude, long-ringing modes: a metallic tail); modal
  density; the spectral balance of the tail (tail spectrum against the input).
- **Dispersion** (spring models): the group delay over frequency of the early response (the "chirp" of the spring is visible in the spectrogram).

### 4.4 Stereo
The inter-channel coherence of the late tail over frequency (for a binaural room the IACC of ISO 3382-1; for a plugin the plain coherence of its
two outputs), the balance of the cross terms, the width as a number.

### 4.5 Time variance (modulated reverbs)
- The system class flags it. A modulated tail differs between two renders: **synchronous averaging of repeated sweeps cancels the late tail**
  (a known effect in room acoustics), so the decay is taken from each render alone and the spread across renders is reported.
- The **interrupted-noise method** of ISO 3382-1 (noise switched off, the decay of the ensemble average) is natural for time-variant reverbs:
  the decay curves of several noise bursts are averaged in energy, not in amplitude.
- The modulation itself: the spectral spread of a steady sine in the tail (the line broadens into sidebands), the coherence between repeated
  renders over time.

### 4.6 Level dependence
The impulse response at three levels (as the system class): non-linear reverbs (saturated tanks, gated reverbs) show it.

## 5. Comparing two reverbs (and matching)
- Per band: T30 and EDT differences in %, C80 in dB, pre-delay in ms, the spectral balance of the tail in dB, the echo density profile and the
  mixing time, the stereo coherence.
- **Just noticeable differences** for a "the same / audibly close / different" scale: ISO 3382-1 gives 5 % for the reverberation time (from
  Seraphim 1958); studies since then found 3 % to 26 % depending on the signal, and recent work shows a dependence on the listener and on the
  level. Proposal: 5 % as "the same" with the literature range in the report, to be confirmed by the author. The clarity JND (often quoted as
  1 dB) is to be checked against the text of ISO 3382-1 before it is used.
- **Matching**: a "reverb domain" for the general matcher: T30 per octave, EDT, pre-delay, C80, the tail's spectral balance; released
  parameters decay, size, pre-delay, damping (from a mapping). Differentiable reverberation (Lee, Choi, Lee 2022) shows that FDN parameters can be
  estimated from an impulse response; here the plugin stays a black box and the matcher searches its knobs as for the EQ (W9).
- The null test is meaningless for a modulated or diffuse tail (two equal-sounding tails do not cancel); it applies to the early reflections of
  LTI reverbs only.

## 6. Visualising
- **EDC per octave band** (dB over time, one curve per band), with the T30 regression lines.
- **Decay time over frequency** (T30 and EDT per third octave, line plot), two plugins overlaid, with the JND as a band around the master.
- **Energy decay relief** as a heat map (frequency over time, the plasma colour map of the spectrogram): the damping at a glance.
- **Echogram of the first 100 ms** (the early reflections as stems, the pre-delay marked).
- **Echo density profile** over time with the mixing time marked.
- **Stereo coherence** over frequency.
- The spectrogram of the sweep response (exists); for spring reverbs it shows the dispersion.

## 7. Reference plugin (ground truth)
**Reference Reverb**: an FDN with absorption filters that give an exact T60 per band (Jot's design), an exact pre-delay, a few early reflection
taps, a modulation switch (time-variant on demand) and a freeze; plus a "convolution" mode with a synthetic impulse response (exponentially
decaying Gaussian noise with a known T60 per band): every value of 4 has an expected value.

## 8. Work packages
| Step | Content | Done when |
|---|---|---|
| W11.2a | impulse response with the tail, 2 x 2; band filters (IEC 61260) | the reference's response complete, its bands as designed |
| W11.2b | EDC with Lundeby truncation, T20/T30/EDT, C50/C80/D50/Ts, pre-delay | the reference's T60 per band within 2 %, pre-delay within 0.1 ms |
| W11.2c | EDR, echo density profile, mixing time, coloration measure | the synthetic noise tail gives an echo density of 1; a sparse reference setting does not |
| W11.2d | stereo coherence; time variance (per-render decay, interrupted noise) | the modulated reference: decays agree across renders within the stated spread |
| W11.2e | report section "Reverb", Developer page, views of 6 | readable reports for the reference and two real reverbs |
| W11.2f | comparison with JNDs, the reverb domain of the matcher | the reference matched to itself; two real reverbs compared per band |

## 9. Questions for the author
1. **Test set**: which free reverbs (an FDN algorithm, a plate, a spring, a convolution reverb with a known IR)?
2. **Bands**: octaves (ISO 3382-1 default) or third octaves for the reports?
3. **100 % wet**: should the rig set the mix to 100 % (from the mapping) before measuring, or measure as set and separate the direct path?
4. **JND scale**: 5 % for T30 as "the same", the range from the literature in the report: right?
5. **Convolution reverbs**: is it interesting to recover the loaded IR and compare it to the file (a null test against the file)?

## 10. References
- ISO 3382-1:2009, "Acoustics - Measurement of room acoustic parameters - Part 1: Performance spaces" (T20, T30, EDT, C50, C80, D50, Ts, IACC;
  the interrupted-noise and integrated-impulse methods; the JND of 5 % for the reverberation time).
- ISO 18233:2006, "Acoustics - Application of new measurement methods in building and room acoustics" (sweeps and MLS).
- IEC 61260-1:2014, octave-band and fractional-octave-band filters.
- M. R. Schroeder, "New method of measuring reverberation time," JASA 37(3), 1965, pp. 409-412.
- A. Lundeby, T. E. Vigran, H. Bietz, M. Vorländer, "Uncertainties of measurements in room acoustics," Acustica 81, 1995, pp. 344-355 (the
  truncation point and its compensation).
- J.-M. Jot, "An analysis/synthesis approach to real-time artificial reverberation," Proc. IEEE ICASSP 1992 (the energy decay relief).
- J. S. Abel, P. Huang, "A simple, robust measure of reverberation echo density," AES 121st Convention, San Francisco, 2006, paper 6985; and
  "Aspects of reverberation echo density," AES 123rd Convention, 2007.
- A. Lindau, L. Kosanke, S. Weinzierl, "Perceptual evaluation of model- and signal-based predictors of the mixing time in binaural room impulse
  responses," JAES 60(11), 2012, pp. 887-898.
- V. Välimäki, J. Parker, L. Savioja, J. O. Smith, J. S. Abel, "Fifty years of artificial reverberation," IEEE TASLP 20(5), 2012, pp. 1421-1448.
- S. Lee, H.-S. Choi, K. Lee, "Differentiable artificial reverberation," IEEE/ACM TASLP 30, 2022 (arXiv 2105.13940).
- A. Farina, "Simultaneous measurement of impulse response and distortion with a swept-sine technique," AES 108th Convention, 2000.
- The JND literature on the reverberation time: e.g. the review and measurements in arXiv 1902.06880; L. Treybig et al., "Just noticeable
  reverberation difference at varying loudness levels," AES Europe 2023 (to be read in full before the scale is fixed).

Sources found 2026-10-18:
[Schroeder integration (overview)](https://vocal.com/resources/schroeder-integration-and-reverberation-time/),
[pyrato: Lundeby EDC](https://pyrato.readthedocs.io/en/latest/modules/edc.html),
[EDR, J. O. Smith, PASP](https://ccrma.stanford.edu/~jos/pasp/Energy_Decay_Relief.html),
[Lindau et al. 2012 (open access)](https://depositonce.tu-berlin.de/items/47045cb9-0ae3-489e-9e87-b175e0f5ae02/full),
[Differentiable artificial reverberation](https://arxiv.org/abs/2105.13940),
[RT JND discussion](https://ar5iv.arxiv.org/html/1902.06880),
[Fifty years of artificial reverberation](https://aes2.org/publications/elibrary-page/?id=18061).
