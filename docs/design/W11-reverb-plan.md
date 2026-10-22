# W11.2: reverb (plan)

Status: **plan v3** (v1 2026-10-18; v2 2026-10-19 with the author's answers; v3 2026-10-21 with the author's AES 121 papers and Matlab
tools: the monaural and the stereo analysis of artificial reverberation, sections 4.3, 4.4, 4.7, 6, 8). Part of W11, after the delay (`W11-delay-plan.md`), before
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
- **First stimulus: a delta impulse** (as in the author's AES 121 papers: a plugin is digital, so a single delta gives the noise-free impulse
  response of the device directly, without deconvolution; as in `W11-delay-plan.md` 4.1), followed by silence until the output has decayed (at
  most 30 s; a freeze or an infinite decay is reported as such). The synchronized sweep (ISO 18233) only where the impulse is not enough: the
  harmonic responses of a non-linear reverb (4.6).
- Per channel pair (L to L, L to R, R to L, R to R): a stereo reverb is a 2 x 2 system; the stereo width comes from the cross terms.

### 4.2 Decay (ISO 3382-1)
- **Energy decay curve** by Schroeder's backward integration of the squared impulse response (Schroeder 1965), per octave and third-octave band.
  The band filters: third octaves after IEC 61260 (the author's Terzband design with its check against the IEC tolerance masks,
  `~/SoftDev/MatlabTools/matlabOldThings/MatlabTools/Filterbank/Terzband/`); octaves also by the author's doubly complementary filterbank of
  coupled all-passes (FB_Init / FB_process there: Butterworth bands whose sum is flat in power and in magnitude, after Vaidyanathan and Mitra), so
  that the bands add up to the broadband response. Not used: the older QuasiTerzbank, whose bands below 500 Hz are 100 Hz wide (not IEC 61260). A plugin has no acoustic noise floor, but dither, modelled noise and the numerical floor exist: the truncation
  and its compensation after Lundeby et al. (1995).
- **T20, T30, EDT** per band by the linear regressions of ISO 3382-1 (the old AnalyseTime.m counted the samples between -5 and -35 dB instead of
  fitting a line); the curvature of the EDC (two-slope decays: a reverb with a fast early and a slow
  late part).
- **Energy decay relief** (Jot 1992): the EDC per frequency bin from the short-time spectrum: the decay time as a continuous function of frequency
  (what "damping" and "low multiplier" knobs do).
- **Clarity C50, C80, definition D50, centre time Ts** (ISO 3382-1), relative to the direct path at the plugin's latency (or to the first sample
  of the response for a 100 % wet reverb: stated in the report).
- **Pre-delay**: from the input (corrected for the latency) to the onset of the reverb (the first sample above a threshold re the peak, and the
  rise of the envelope).
- **Direct path delay (DPD) and direct-to-reverberant ratio (DRR)** (the author: mix is often a parameter): the delay of the direct path against
  the input (the latency, and any extra delay of the dry signal), and the energy of the direct path (a short window around it) against the energy
  of the rest of the response, in dB; reported at the plugin's setting and, with the mix known from the mapping, as a check of the mix knob.

### 4.3 Texture: density, diffusion, coloration
- **Echo density profile** (Abel and Huang 2006): the normalized echo density over time (1 = Gaussian, like noise); its rise shows the diffusion,
  sparse early echoes, flutter; the breakpoints 0.3 and about 0.7 that listening tests found as perceptually distinct groups.
- **Mixing time**: when the response becomes diffuse; perceptually validated predictors include the echo density profile (Lindau, Kosanke,
  Weinzierl 2012).
- **Coloration**: the fine structure of the late spectrum (the peakiness of the smoothed magnitude, long-ringing modes: a metallic tail); modal
  density; the spectral balance of the tail (tail spectrum against the input).
- **Dispersion** (spring models): the group delay over frequency of the early response (the "chirp" of the spring is visible in the spectrogram).

From the author's monaural toolbox (Extra, Simmer, Fischer, Bitzer, AES 121, 2006), tools that tell good from poor artificial reverberation:
- **Echo count after Griesinger**: blocks of 20 ms, per block the number of reflections within 20 dB of the block's largest one (Griesinger
  asks for about 10000 per second for percussive sounds); next to the normalized echo density of Abel and Huang above.
- **Short-time histogram and kurtosis**: the impulse response normalized by the square root of its EDC (h(n) / sqrt(EDC(n)): the decay
  removed, a stationary signal remains), cut into blocks of 1024 samples; per block the histogram of the amplitudes (a heat map over time) and
  the kurtosis (0 for Gaussian noise). Moorer found that the best reverberation sounds like exponentially decaying Gaussian noise: a late
  kurtosis near 0 is good; Schroeder's reverberator stays far from it.
- **Normalized autocorrelation** of the EDC-normalized response (Griesinger): repetitions in the tail (periodic peaks) are audible as a metallic or
  grainy sound; a natural room shows none.

### 4.4 Stereo
From the author's stereo toolbox (Bitzer, Extra, AES 121, 2006; Bitzer, AES 122, 2007). A stereo reverb's two outputs are not two ears, but the
binaural measures, adapted, describe its spatial impression:
- **IACC** (interaural cross-correlation): the maximum of the normalized cross-correlation of L and R within +-1 ms, over the first 100 ms;
  **with and without the direct sound** (with it the IACC is dominated by the dry signal; from 0.1 to 100 ms it shows the reverb's own
  spaciousness: lower = wider), with the lag of the maximum; also per octave or third octave.
- **IAD** (interaural difference, Griesinger): 10 lg((l - r)^2 / (l^2 + r^2)) over time and frequency (40 ms RMS per octave band) as a
  time-frequency plot; in the author's version without the normalization by l^2 + r^2, which is easier to read.
- **LEDT** (lateral early decay time, Griesinger): the decay of the interaural difference (the Schroeder sum of (l - r)^2, IADDC) against the sum of
  the two EDCs, by two definitions (Griesinger's 350 ms point and the -10 dB point), referred to -60 dB.
- **RLE** (room level early, Trautmann; Blau): the early part of the room level from the first 80 ms in 2 ms segments; per segment the dominant
  lateral angle from the lag of the cross-correlation (low-passed at 800 Hz, weighted by the level difference), weighted by a growing factor
  q = 1 - exp(-t / 15 ms); **computed with and without the direct sound** (E_D = 0): without it the values spread from -8 to +3 dB between the six
  algorithms of the paper, positive values as in good concert halls; the author: "a good candidate for quality evaluation".
- **Time-angle phase scope (Spatial Distribution Meter)**: the phase scope (L against R, turned by 45 degrees: centre up) of overlapping blocks
  (50 % overlap; 50 ms for the whole response, 20 ms for the first 0.3 s), the circle split into 32 sectors, per sector the mean energy in dB: a
  heat map of the direction over time. It shows where the early reflections come from (the centre, one side), when the response becomes
  spatially diffuse, and negative correlation (nL, nR) - the most telling stereo picture of the paper.
- The plain inter-channel coherence of the late tail over frequency, the balance of the cross terms of the 2 x 2 response.

### 4.5 Time variance (modulated reverbs)
- The system class flags it. The author's method (AES 121): many impulse responses (the paper used 1000) of the same setting; single peaks
  (early reflections) are picked and the distribution of their values over the renders shown as a histogram: a modulated reflection spreads,
  a fixed one does not. With a plugin the renders follow each other without the gap of about one second the hardware needed. A modulated tail differs between two renders: **synchronous averaging of repeated sweeps cancels the late tail**
  (a known effect in room acoustics), so the decay is taken from each render alone and the spread across renders is reported.
- The **interrupted-noise method** of ISO 3382-1 (noise switched off, the decay of the ensemble average) is natural for time-variant reverbs:
  the decay curves of several noise bursts are averaged in energy, not in amplitude.
- The modulation itself: the spectral spread of a steady sine in the tail (the line broadens into sidebands), the coherence between repeated
  renders over time.

### 4.6 Level dependence
The impulse response at three levels (as the system class): non-linear reverbs (saturated tanks, gated reverbs) show it.

### 4.7 Porting the author's Matlab tools: what to correct
The tools are from 2006 ... 2018 (the author: "check the code for mistakes and errors, it's quite old"). Found while reading them for this plan;
each port gets a test against an analytic case (a synthetic stereo response with known delays, levels and angles):
- **RLE.m**: the 6 kHz low-pass does nothing: `b = a./a(1)` makes the numerator equal to the denominator (meant: `b = [1 1]./a(1)`); the
  800 Hz filter above it is a correct first-order low-pass (bilinear, not pre-warped), the comment "das ist kein TP" there is wrong; the
  analysis window `ir(i_0:i_80*4, :)` multiplies the absolute index (meant: from i_0 for 4 x 80 ms) and fails when the direct sound does not
  start at 0; the input checks test argument 1 four times; the direct-sound energy scaling `(Fs/10000*74+1)/75` (a correction for the 10-sample
  hop) is to be derived again, it does not match the overlap of the 2 ms segments at 44.1 or 48 kHz; `n_delays` assumes a time vector that starts
  at 0.
- **EchoDensity.m** (Abel and Huang): the Hann-weighted mode counts unweighted and does not normalize by the window (its values are a window
  length too large); the blocks do not overlap (Abel and Huang slide the window sample by sample); the port uses the weighted, normalized,
  sliding definition of the paper.
- **SDM.m**: the sector edges start at a quarter of a sector (pi / (2N) instead of half a sector, pi / N), so the last sector's computed
  value is overwritten by the wrap-around sector and part of it lost; the angle axis labels do not match the sector centres.
- **STH.m**: the direct sound and the pre-delay are found by absolute thresholds (0.004, the mean of |h|) that depend on the level; the histogram
  range is fixed (+-0.05) although the EDC normalization changes the scale; the port uses relative thresholds and a range from the data.
- **IACC.m** (found while porting, 2026-10-22): `if MaxVal > MinVal` is always true, so the negative correlation was never chosen; meant:
  the value of the larger magnitude (`abs(MaxVal) > abs(MinVal)`). The port returns the signed value of the largest |r|.
- **IAD_TimeFreq.m**: `mode` has no default although the comment says "default 0" (a call with two arguments fails).
- **AnalyseTime.m**: T60 counted instead of regressed (4.2); the EDC start found by an absolute energy difference (`Edc_Dif > -1`); the output
  names in the comment are swapped (Edt_vek / RT_vek).

## 5. Comparing two reverbs (and matching)
- Bands: octaves and third octaves, both (the author).
- Per band: T30 and EDT differences in %, C80 in dB, pre-delay in ms, DPD and DRR, the spectral balance of the tail in dB, the echo density profile and the
  mixing time, the stereo coherence.
- **Just noticeable differences** for a "the same / audibly close / different" scale: ISO 3382-1 gives 5 % for the reverberation time (from
  Seraphim 1958); studies since then found 3 % to 26 % depending on the signal, and recent work shows a dependence on the listener and on the
  level. Decided (the author): 5 % as "the same" to start with, the literature range in the report. The clarity JND (often quoted as
  1 dB) is to be checked against the text of ISO 3382-1 before it is used.
- Recovering the IR of a convolution reverb and comparing it to its file: not wanted (the author).
- **Matching** (later): a "reverb domain" for the general matcher: T30 per octave, EDT, pre-delay, C80, the tail's spectral balance; released
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
- **Short-time histogram** as a heat map over time with the **kurtosis** below it (4.3).
- **Normalized autocorrelation** of the tail (repetitions as peaks).
- **Echo count per 20 ms** (Griesinger) as stems over time.
- **IAD time-frequency plot** (octave bands over time, dB).
- **Time-angle phase scope / Spatial Distribution Meter**: direction (nR, L, C, R, nL) over time, energy as colour, 50 ms frames for the whole
  response and 20 ms for the first 0.3 s.
- **Stereo table**: IACC with and without the direct sound (and its lag), LEDT (both definitions), RLE with and without the direct sound.
- For the manual of Jade Benchmark the author's figures may be used (the author's permission): the EPS files in
  `~/Research/Paper/OldFiles/AES_121_Stereo/matlab/Tools/` (ExplainTimePhase, ExplainLEDT, ExamplePhaseScope, IAD_Comparison, PS_Comparison)
  and the figures of the two papers; redrawn from our measurements where possible.

## 6b. The 100 % wet question (the author)
The measurements want the reverb alone; listening wants the dry signal too. The products guide the user: measure with the plugin at 100 % wet
(the mix from the mapping, or the user is asked to set it), and our own mixer adds the direct path for listening (dry/wet in the product, not in
the plugin), so that nobody has to listen to 100 % wet. Where the plugin's mix is left as it is, DPD and DRR (4.2) describe the direct path.

## 7. Reference plugins (ground truth)
The author's choice: our own **Moorer-type reverb** (Freeverb as the candidate: parallel combs with damping, series all-passes) in the
reference set; if source code is found, **Dattorro's plate tank** and **Gardner's nested (coupled) all-pass** reverbs; **convolution** with
impulse responses from open libraries; a **spring** reverb is a special case (strongly dispersive, a chirp in the impulse response) and needs a
free example. In addition, for exact values:
**Reference Reverb**: an FDN with absorption filters that give an exact T60 per band (Jot's design), an exact pre-delay, a few early reflection
taps, a modulation switch (time-variant on demand) and a freeze; plus a "convolution" mode with a synthetic impulse response (exponentially
decaying Gaussian noise with a known T60 per band): every value of 4 has an expected value.

## 8. Work packages
| Step | Content | Done when |
|---|---|---|
| W11.2a | impulse response with the tail, 2 x 2; band filters (IEC 61260) | the reference's response complete, its bands as designed |
| W11.2b | EDC with Lundeby truncation, T20/T30/EDT, C50/C80/D50/Ts, pre-delay, DPD, DRR | the reference's T60 per band within 2 %, pre-delay within 0.1 ms, DRR within 0.1 dB |
| W11.2c | EDR, echo density profile, mixing time, Griesinger echo count, short-time histogram and kurtosis, normalized autocorrelation, coloration | the synthetic noise tail gives an echo density of 1 and a kurtosis near 0; a Schroeder setting of the reference shows repetitions in the autocorrelation |
| W11.2d | time variance (per-render decay, interrupted noise, the distribution of single peaks over many renders) | the modulated reference: decays agree across renders within the stated spread; its modulated reflections spread in the histogram |
| W11.2g | stereo: IACC (with and without the direct sound), IAD time-frequency, LEDT (both definitions), RLE (with and without the direct sound), time-angle phase scope, coherence; the corrections of 4.7 | a synthetic stereo response with known delays, levels and angles gives its expected values in every tool; the six example cases of the paper's figure 3 reproduced |
| W11.2e | report section "Reverb", Developer page, views of 6 | readable reports for the reference and two real reverbs |
| W11.2f | comparison with JNDs, the reverb domain of the matcher | the reference matched to itself; two real reverbs compared per band |

## Progress
### W11.2a, W11.2b the bands, the reference tail, the decay (0.81.0, done)
- **Bands** (`measure::getBands`): IEC 61260-1, base 10 (centres 1000 * 10^(3x / 10b), edges * 10^(+-3 / 20b)), octaves and third octaves.
  **Filters** (`reference::designButterworthBandPass`): the Butterworth low-pass prototype of order 3 transformed to the band (edges
  pre-warped), bilinear, one section per pole pair: exactly 0 dB at the centre's digital image and -3.01 dB at both edges (tested for every
  band from 31.5 Hz to 16 kHz). A bug found by that test: for a wide band near Nyquist (the 16 kHz octave) the real prototype pole becomes two
  real band-pass poles, which the first version dropped.
- **Reference tail** (`reference::SyntheticReverb`): direct path, pre-delay, reflection taps, and noise with a flat magnitude spectrum (random
  phases) decayed per 1/12-octave sub-band by 10^(-3 t / T60(f)) in a short-time spectrum; optional noise floor. Every band's expected decay is
  computed from the sub-bands' energies through the band filter (the overlap of neighbouring bands included).
- **Decay** (`measure::analyseReverb`): the direct path (peak within 1 ms after the latency, within 40 dB of the largest sample), DPD, DRR
  (+-2.5 ms around it), the onset (first sample within 20 dB of the reverb's peak) and pre-delay; per band and broadband: Lundeby's iteration
  (a floor only if the last tenth no longer falls), the compensated Schroeder curve, T20, T30, EDT by least squares, C50, C80, D50, Ts.
- **Tests**: a flat T60 of 2 s and a T60 falling from 2.5 s to 0.8 s: every octave and third octave within 2 % of its expected decay; a floor
  55 dB down (Lundeby): within 2 %; pre-delay within 0.1 ms; DRR within 0.1 dB; 100 % wet with a reflection. **The 2 % need the mean of
  several noise decays** (4 seeds): one decay of narrow-band noise scatters by about 1 / sqrt(B T) (the 125 Hz third octave, B = 29 Hz: about
  6 %); for narrow low bands the tolerance is that scatter. A single measured reverb carries the same uncertainty; the report should give it.
- Not yet: the doubly complementary octave filterbank (FB_Init), the views.

### W11.2c the texture (0.82.0, done)
- `measure::analyseTexture`: the echo density profile (Abel and Huang 2006: the weighted, normalized, sliding definition of the paper, every
  millisecond, 20 ms Hann; the corrections of 4.7 for EchoDensity.m), the mixing time (density first at 0.95), Griesinger's echo count per
  20 ms, the short-time histogram and the excess kurtosis of h / sqrt(EDC) per 1024 samples (the range from the data: +-4 standard deviations,
  the correction of 4.7 for STH.m), the late kurtosis (after the mixing time; never mixed: the second half), the normalized autocorrelation of
  h / sqrt(EDC) after the mixing time (by FFT) and its largest peak beyond 1 ms, the coloration (the scatter in dB of the late spectrum's fine
  structure around its third-octave smoothing). `measure::getDecayRelief`: the EDR (Jot), the decay time per bin.
- `reference::SchroederReverb`: four parallel combs (Freeverb's delays, gains for a T60), two all-passes in series.
- Tests: the noise tail: density 1.000, mixed after 10 ms, late kurtosis 0.010, repetition 0.011, coloration 5.60 dB (Rayleigh: 5.57 dB), the
  EDR's decay time 2.0 s within 5 % (500 Hz ... 4 kHz); Schroeder's reverb: repetition 0.26 at a comb delay (1422 samples), never mixed, late
  kurtosis 59 (the paper: "far from" Gaussian).

### W11.2d time variance (0.83.0, done)
- `reference::FdnReverb`: 8 coprime delay lines, a Householder matrix, per line the gain for an exact T60 (Jot and Chaigne), pre-delay, two
  outputs; optionally an LFO per line on its length (Lagrange per sample).
- `measure::analyseTimeVariance`: one instance, impulses 2.5 s apart; each response's T30 alone (averaging would cancel a modulated tail) and
  their spread; the 8 strongest early peaks of the first response followed through all responses (the author's AES 121 method): the spread of
  their values; time-variant above 5 %. `measure::getInterruptedNoiseT30`: ISO 3382-1's interrupted noise, the decays averaged in energy.
- Tests: the fixed FDN: T30 1.5028 s in every response, the peaks do not move, interrupted noise 1.504 s; the modulated FDN (1 Hz, 8 samples):
  time-variant (peak spread 1.3), the responses agree (spread 0.6 %), the 500 Hz octave keeps 1.52 s, interrupted noise as the impulses.
- Found: the modulation shortens the broadband decay (1.41 s for 1.5 s): the interpolation in the loop damps the highs on every circulation
  (the same mark of the interpolation as in the delay's loop filter, W11.1b). A modulated reverb's T60 at high frequencies is shorter than its
  fixed version's: worth a line in the report of a real modulated reverb.

### W11.2g stereo, part 1 (0.84.0, done)
- `measure::getIacc` (with the corrected sign; with and without the direct sound by the window), `getCoherence` (Welch, Hann, 50 %),
  `getInterauralDifference` (per octave, 40 ms RMS, normalized and the author's unnormalized version), `getSpatialDistribution` (the
  time-angle phase scope: 50 ms blocks, 50 % overlap, 32 sectors centred on k 2 pi / N, so that C, L, R, nL, nR lie in the middle of sectors).
- Tests (synthetic): IACC 1 for identical channels, 1 at +0.5 ms for a delayed copy, -1 for an inverted one (IACC.m would not have found it),
  about 0 for independent noise; coherence 1 and about 0; IAD +3.01 dB for L = -R, 0 dB for independent noise, the floor for L = R; the
  SDM puts a centred direct sound at C, a left reflection at L, a negatively correlated one at -pi/2.
- Part 2: LEDT (both definitions), RLE (with and without the direct sound; question 6: the convention for tau_0), the paper's six examples.

### W11.2g part 2 (LEDT) and W11.2e the report section (0.85.0)
- `measure::getLateralDecay` (LEDT.m: the direct sound removed by a window, not by zeroing the first sample): both definitions; two independent
  tails of 2 s with 20 ms pre-delay give 2.12 s by both (counted from the direct sound: T60 + 6 x pre-delay, and 2 s x 0.35 / 0.33).
- The measurement unit "reverb" (`measure::measureReverbResponse` on the device: an impulse, 8 s): a reverb is there if the echo density
  reaches 0.95 within 1 s of the onset (an FDN without diffusers mixes late: the reference 0.34 s; a delay's echo train never) and T30 > 0.1 s.
  Rows: T30 (EDT, T20), T30 per octave, C80, pre-delay, DRR, mixing time, late kurtosis, repetition, coloration, IACC early and late,
  coherence of the tail, LEDT. The summary item "reverb" and the Developer page's column.
- **The decay of a plugin with its dry signal**: at their defaults the real reverbs mix the dry signal 12 ... 20 dB above the tail; the EDC from
  the direct sound then falls by that much in one sample (EDT 0.000 s) and the ranges of ISO 3382-1 (made for rooms) see only a few dB of the
  tail (Dragonfly Plate: T20 1.39 s, T30 0.56 s). The decay times are now taken from the reverberant part (the direct path cut out before the
  band filters, the curves from the onset), the clarity, D50 and Ts keep the direct path. Afterwards EDT, T20 and T30 agree.
- **PluginLab Reference Reverb** (plugins/reference/ReferenceReverb): the FDN with T60, pre-delay, LFO rate and depth, mix. Through the host:
  T30 1.50 s in every octave, the pre-delay 10 ms + 21.5 ms.
- Real reverbs at their defaults (`~/AudioDev/W11Plugins/reverb_reports/`):

| plugin | T30 | per octave 125 Hz ... 8 kHz | pre-delay | DRR | mixing | late kurtosis | IACC late |
|---|---|---|---|---|---|---|---|
| Dragonfly Hall | 1.31 s | 1.65 ... 0.74 s (darker with frequency) | 15.6 ms | 12.4 dB | 10 ms | 3.0 | -0.05 |
| Dragonfly Plate | 0.40 s | 0.40 ... 0.39 s (flat) | 56.0 ms | 18.1 dB | 24 ms | 0.8 | 0.20 |
| TAL-Reverb-4 | 9.7 s | 10.1 ... 9.5 s | 42.5 ms | 15.7 dB | 341 ms | 0.9 | 0.02 |
| Vital Reverb | 1.21 s | 1.34 ... 1.07 s | 12.9 ms | 11.8 dB | 90 ms | 0.1 | 0.08 |
| Room Reverb | 0.99 s | 1.06 ... 0.98 s | 5.8 ms | 20.6 dB | 46 ms | 6.1 | 0.33 |
| Reference Reverb | 1.50 s | 1.50 ... 1.50 s | 31.5 ms | 14.4 dB | 342 ms | 0.2 | 0.03 |

  Room Reverb's late kurtosis (6.1) and late IACC (0.33) stand out: a sparser, less decorrelated tail; Dragonfly Hall's kurtosis of 3 too.
- Not yet: the views of section 6 (EDC per band, decay over frequency, EDR, echogram, echo density, SDM, ...) in the reports; the 100 % wet
  guidance of 6b in the products.

### W11.2f comparison and matching (0.86.0, done)
- `measure::compareReverbs`: per octave and third octave the T30 difference of B against A in % with the verdict (the same <= 5 %, audibly
  close <= 10 %, different), the differences of EDT, C80 (not judged until its JND is checked), pre-delay, DRR, mixing time, late kurtosis,
  late IACC. `engine::compareReverbResponses` (Markdown) and `PluginLabHost --compare-reverbs <A> <B> <report>`.
- The reverb domain of the matcher (`match::ReverbProbe`, `makeReverbObjective`): an impulse, the decay analysis (octaves), the features in
  JND units (T30 per octave and EDT as ln(T) / ln(1.05), the pre-delay per ms, C80 per dB), one tolerance for all: 1 JND.
- Tests: FDN 1.6 s against 1.5 s: +6.7 % (within 1.5 points per octave), audibly close in all 7 octaves; 1.52 s: the same; the report of the
  plugin at 1.5 s and 2.0 s; **the Reference Reverb matched to itself**: from 1.5 s / 10 ms to 2.194 s / 34.91 ms (master 2.2 s / 35 ms) in
  7 renders, every feature within 0.1 JND.
- Two real reverbs compared (`~/AudioDev/W11Plugins/reverb_reports/compare_DragonflyHall_Vital.md`): Dragonfly Hall against Vital Reverb at
  their defaults: T30 -8 % (audibly close); per octave 1 the same, 3 close, 3 different: Vital shorter in the bass (-19 % at 125 Hz) and much
  longer at 8 kHz (+43 %: Dragonfly darkens its tail, Vital hardly); mixing 10 ms against 90 ms, late kurtosis 3.0 against 0.1.
- Not yet: the tail's spectral balance as a feature, a reverb page in the products (the Matcher's domain switch).

### The views of section 6 (0.87.0, done)
- `ui::renderChart` (lines or stems, linear or log x, markers, legend) and `ui::renderHeatmap` (plasma, log or linear rows, colour bar), the
  look of the response plot and the spectrogram. `ui::writeReverbViews`: twelve PNGs next to a report, with the Markdown that shows them:
  EDC per octave with the T30 lines, T30 and EDT per third octave, the EDR, the echogram of the first 100 ms with the pre-delay, the echo
  density profile with the mixing time, Griesinger's echo count, the short-time histogram and the kurtosis, the normalized autocorrelation,
  the coherence, the IAD per octave over time, the Spatial Distribution Meter (every block re its loudest direction: TimePhaseScope.m's
  bNormalize; silent blocks stay dark). The heat maps end at the pre-delay plus 1.2 times the longest octave's T30.
- `--measure` and `--fingerprint` append them when the reverb unit found a reverb ("Reverb views").

## 9. Questions for the author
1. **Test set**: which free reverbs (an FDN algorithm, a plate, a spring, a convolution reverb with a known IR)?
Answer: I will collect a list of free reverbs. We should have at least A Moorer based reverb (Freeverb is a good candidate) in our own referenc plugin). Perhaps we will find source code for Dattoros tank and the coupled allpass from William Gardner. For convolution we can download some IRs from open source libraries. The spring reverb is a special case, since it is strongly dispersive and has a chirp in the impulse response. We will have to find a free example of that as well.
2. **Bands**: octaves (ISO 3382-1 default) or third octaves for the reports?
Answer: Both
3. **100 % wet**: should the rig set the mix to 100 % (from the mapping) before measuring, or measure as set and separate the direct path?
Answer: This is something where we have to guide the user. For us it would be better, if the user always use 100% wet and we have our own mixer for the direct path (listening options, no one wants to listen to 100% wet signals).
4. **JND scale**: 5 % for T30 as "the same", the range from the literature in the report: right?
Lets start with that-
5. **Convolution reverbs**: is it interesting to recover the loaded IR and compare it to the file (a null test against the file)?
   NO

6. **RLE for stereo reverbs**: the angle comes from the inter-channel lag with tau_0 = 0.7 ms (the ears' maximal delay); for a plugin's two
   outputs that is a convention, not a direction. Keep it as in the paper (comparable with the 2006 results), or scale tau_0 to the largest
   lag the plugin produces?
   I think we should keep it as in the paper, since it is a convention and comparable with the 2006 results.
7. **Order of the stereo tools**: is the Spatial Distribution Meter (the most telling picture) the first stereo tool to port, before IACC, LEDT
   and RLE?
8. **The IEC 61260 tolerance table** (new, 2026-10-22): the author's `getSpec_IEC1260.m` holds the class 0/1/2 masks of IEC 1260:1995 and says
   "Using for scientific purpose only, no publication allowed (Implementation of standard)". May the table go into the repository (a test of the
   band filters against the class 1 mask), or stay out? For now the filters are tested by their exact design values (0 dB at the centre,
   -3.01 dB at both edges), without the table.
   Can you find this list in a public available document or website? If yes, we can use it. If not, we should not include it in the repository.

## 10. References
- D. Extra, U. Simmer, S. Fischer, J. Bitzer, "Artificial reverberation: comparing algorithms by using monaural analysis tools," AES 121st
  Convention, San Francisco, 2006 (EDC, EDR, T60/EDT, echo density, short-time histogram, kurtosis, autocorrelation, C80, time variance; the
  author is a co-author).
- J. Bitzer, D. Extra, "Artificial reverberation: comparing algorithms by using binaural analysis tools," AES 121st Convention, San Francisco,
  2006 (IACC, IAD, LEDT, RLE, the time-angle phase scope).
- J. Bitzer, "Spatial Distribution Meter: a new method to display spatial impression over time," AES 122nd Convention, Vienna, 2007.
- D. Griesinger, "Spaciousness and envelopment in musical acoustics," AES 101st Convention, 1996, preprint 4401; "Objective measures of
  spaciousness and envelopment," AES 16th International Conference, 1999.
- J. A. Moorer, "About this reverberation business," Computer Music Journal 3(2), 1979, pp. 13-28.
- M. Blau, "Correlation of apparent source width with objective measures in synthetic sound fields," Acta Acustica united with Acustica 90,
  2004, pp. 720-730; U. Trautmann, "Die messtechnische Erfassung der Räumlichkeit in Sälen für musikalische Darbietungen," PhD thesis, TU Dresden,
  1986 (the room level RL).
- M. R. Schroeder, "Natural sounding artificial reverberation," JAES 10(3), 1962, pp. 219-223.
- J. Dattorro, "Effect design, part 1: reverberator and other filters," JAES 45(9), 1997, pp. 660-684.
- W. G. Gardner, "The virtual acoustic room," Master's thesis, MIT Media Lab, 1992.
- P. P. Vaidyanathan, S. K. Mitra, Y. Neuvo, "A new approach to the realization of low-sensitivity IIR digital filters," IEEE Trans. ASSP 34(2),
  1986 (doubly complementary filters from coupled all-passes: the structure of the author's FB_Init).
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


## Comments
* often mix is a  parameter, so we should measure the direct to reverb ratio (DRR) and the direct path delay (DPD) as well, and report them.

## more comments

Several years ago I programmed a toolbox in matlab to measure mono and stereo reverb algorithms. I Also had a new display to show the room dispersion over time. 
You can find the paper here: [stereo paper](../../../../Research/Paper/OldFiles/AES_121_Stereo/AES121_BitzerExtra.pdf)

[mono paper](../../../../Research/Paper/OldFiles/AES121_Mono/paper121.pdf)
You can find the code here:

/home/bitzer/Research/Paper/OldFiles/AES_121_Stereo/matlab/Tools/

and

/home/bitzer/SoftDev/MatlabTools/AudioInspector/RIRTools/


Especially Room Level Early was a good candidate for quality evaluation.
Check the code for mistakes and errors, its quit old.

You are allowed to look in these two sub-directories for other material that could help, e.g. figures to be used in a manual for Jade Benchmark

And something for the one-third band split: /home/bitzer/SoftDev/MatlabTools/matlabOldThings/MatlabTools/Filterbank/

## possible test candidates

https://www.kvraudio.com/product/main-st-63-spring-by-main-st-audio-labs/downloads
(open source)

https://www.kvraudio.com/product/voyage-voyage-by-musical-entropy/downloads

https://mikey.audio/shu

https://www.kvraudio.com/product/vitalreverb-by-manaswolrd/downloads
(open source)

https://duskaudio.com/plugins/duskverb/

https://github.com/lucianoiam/castello
(open source)

https://socalabs.com/effects/simpleverb/

https://www.kvraudio.com/product/vitaliumverb-by-billy-messenger/downloads
open source

https://github.com/giulioz/smol-rack

https://www.kvraudio.com/product/reev-r-by-tilr/downloads

https://superflydsp.com/index.php/flying-reverb/

https://www.kvraudio.com/product/fdn64reverb-by-uhhyou/downloads

https://www.elephantdsp.com/products/room-reverb/

https://www.kvraudio.com/product/l4reverb-by-uhhyou/downloads

https://www.kvraudio.com/product/latticereverb-by-uhhyou/downloads

https://www.kvraudio.com/product/mverb2020-by-socalabs/downloads

https://github.com/michaelwillis/dragonfly-reverb/releases/tag/3.2.10

https://tal-software.com/products/tal-reverb-4

https://www.kvraudio.com/product/hybridreverb2-by-christian-bor/downloads

https://tal-software.com/products/tal-reverb

https://rekkerd.org/martin-eastwood-audio-free-plugins/

https://freeverb3-vst.sourceforge.io/

