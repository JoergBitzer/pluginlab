# W11.6: dynamic EQ (plan)

Status: **plan v1** (2026-10-19, for review; questions for the author in section 8). The author: dynamic EQ is a class of its own, after
dynamics (answer to `W11-dynamics-plan.md`, question 5). It joins the two strongest parts of pluginlab: the EQ band model (W8) and the dynamics
measurements (W11.3).

## 1. What W11.6 has to answer
- **Musician / engineer**: which bands react, at what level, how much do they cut or boost, how fast; what is the EQ curve at rest and at full
  action; is it a de-esser, a resonance tamer, an upward expander?
- **Developer**: is the band's shape kept while its gain moves (a bell that changes only its gain, or one that also moves or widens), is the
  band's detector the band itself or an external key, are attack and release the same for cut and boost?

## 2. The model
A dynamic EQ band is an EQ band (type, frequency, Q: the W8 band model) whose **gain is a dynamics processor's output**: the gain follows the
level in the band's detector (usually the band-passed input) through a static curve (threshold, ratio, range: the maximum cut or boost) and the
two-stage smoothing of Simmer, Schmidt, Bitzer (2006). So: band model x static curve x time behaviour.

## 3. Measurements
### 3.1 At rest and at full action
- Below the threshold: an LTI EQ; the band fit of W8 (type, frequency, gain, Q) gives the curve at rest.
- Driven far above the threshold (a loud tone at the band's frequency, or noise): the curve at full action, fitted the same way; the range is the
  gain difference. A band whose frequency or Q changes between rest and action is reported as such.

### 3.2 The static curve of a band
A tone at the band's frequency stepped in level (as W11.3 4.1); the gain of a **quiet probe tone** at the same band (offset by a few Hz, so
that it can be read separately) against the loud tone's level: the band's gain over the detector level: threshold, ratio, range, knee.

### 3.3 The shape while it acts: the dynamic transfer function
The two-tone test of W11.3 4.4 with the probe swept over frequency while the loud tone drives the band at several levels: a family of curves
that shows the band's shape at each amount of action; fitted with the band model at each level (does only the gain move?).

### 3.4 Time behaviour
The three-phase signal of W11.3 4.3 at the band's frequency, the probe's gain over time: attack, release, hold, the model variant; for cut and
for boost separately (upward and downward bands).

### 3.5 Detector
Which signal drives the band: a tone outside the band (does the band react?), the band-passed input or the full input, an external side
chain; the detector's own filter (its centre and width) from the level a tone needs at each frequency to reach the threshold.

### 3.6 With music
The band gains over time on a music sample (one row per band), the EQ curve at chosen moments (a "frozen" curve from the gains and the band
model), the spectrum before and after.

## 4. Comparing two dynamic EQs
The curves at rest and at full action, each band's static curve and time behaviour, the band shapes while acting, the band gains on music.
Matching later (an EQ match at rest is possible with W9 already).

## 5. Visualising
- **Rest and action curves** of the EQ (the band model's magnitude), with the range between them shaded.
- **Family of curves** of the dynamic transfer function at several drive levels.
- **Static curve of a band** (gain over detector level).
- **Band gains on music** over time; an animation or a series of frozen EQ curves.

## 6. Reference plugin
**Reference Dynamic EQ**: 1 ... 4 bands (bell, shelves), each with frequency, Q, threshold, ratio, range (cut and boost), attack, release, the
detector band-passed or full-band or external; the band model of the Reference EQ (RBJ) and the dynamics of the Reference Compressor (W11.3).

## 7. Work packages
| Step | Content | Done when |
|---|---|---|
| W11.6a | rest and full-action curves with the band fit, the range | the reference's bands at rest and at full action within 0.1 dB, 1 % |
| W11.6b | the static curve of a band (probe method) and the dynamic transfer function | the reference's threshold, ratio and range recovered; shape constancy shown |
| W11.6c | time behaviour per band, cut and boost; the detector | the reference's time constants within 2 % |
| W11.6d | band gains on music, report section, views of 5 | readable reports for the reference and two real dynamic EQs |

## 8. Questions for the author
1. Test set: which free dynamic EQs (and de-essers, which are single-band dynamic EQs)?
2. De-essers: part of this class (proposal: yes, as the one-band case)?
3. Should W11.6 come before W11.5 (it reuses the EQ band fit, which is the most mature part of pluginlab)?

## 9. References
- U. Simmer, D. Schmidt, J. Bitzer, "Parameter estimation of dynamic range compressors: models, procedures and test signals," AES 120th
  Convention, Paris, 2006.
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital dynamic range compressor design - a tutorial and analysis," JAES 60(6), 2012.
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (the band model of the Reference EQ).
- The band model and fit of W8 (docs/eqmodel/), the matcher of W9 (docs/matching/).
