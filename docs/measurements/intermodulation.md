# Intermodulation distortion

Unit of W7.6. Code: `src/measure/Intermodulation.cpp` (`measureDifferenceFrequency`, `measureModulation`), the FFT `getSpectrum` in
`src/measure/Analyzer.cpp`; tests `tests/MeasureIntermodulationTests.cpp`. Conventions: [README](README.md); harmonics: [thd-and-thdn.md](thd-and-thdn.md).

## Purpose
What a non-linearity does to two tones at once. Music is never one sine. Two tones produce sum and difference products that are not harmonics of
either tone and do not blend with the music:
- two close high tones produce a difference tone far below them (a 2 kHz tone from 18 and 20 kHz). This product falls where the ear is most sensitive,
  even when the tones themselves are barely audible, and THD at 1 kHz cannot show it;
- a strong bass tone modulates a high tone (sidebands at $f_2 \pm f_1$), which is how the bass "roughens" the treble through a saturating plugin.

## Standard and sources
- **AES17-2015, 6.3.5 Difference-frequency distortion** (formerly "intermodulation distortion ratio (close-tone)"): two tones of equal level 2 kHz
  apart, the upper at 20 kHz (or at the upper band edge if it is lower); the peak of the sum equals the peak of a sine at the maximum input level, so
  each tone is 0.5 of it. Frequency-domain band-pass filters (5.2.10) 500 Hz wide at 2 kHz (second-order product), 16 kHz (lower third-order product),
  18 kHz (lower fundamental) and 22 kHz (upper third-order product). The ratio is the rms sum of the three products relative to the lower fundamental,
  in dB. AES17 notes that the standard band-pass (5.2.9) is not selective enough, and that the bands may be rms-summed from the FFT of one acquisition.
- **AES17-2015, 6.3.6 Modulation distortion** (formerly "intermodulation distortion ratio (spread-tone)"): 41 Hz and 7993 Hz, the low tone four times
  the high tone in level (0.8 and 0.2 of the maximum input level); bands no wider than 40 Hz at 7952 Hz and 8034 Hz (the modulation sidebands) and at
  7993 Hz (the upper fundamental). The ratio is the rms sum of the two sidebands relative to the upper fundamental.
- **AES17-2015, 5.2.10 and annex B** (frequency-domain and window-width filters): a band-pass made by summing bins; B.5 "the noise problem": the bands
  need a fixed width in Hz, otherwise a noise-limited result depends on the FFT length; B.5.2 recommends +-250 Hz for the close-tone IMD.
- Related older two-tone tests, for orientation only (not used here): CCIF/ITU difference tone (19 + 20 kHz) and SMPTE RP120 (60 Hz + 7 kHz, 4:1). The
  signal generator `makeTwoTone` offers both stimuli. AES17's tone pairs avoid frequencies with simple common divisors.

## Stimuli
`signals::makeTwoTone`: $x[n] = a_1 \sin(2\pi f_1 n/f_s) + a_2 \sin(2\pi f_2 n/f_s)$, the peak of the sum $a_1 + a_2$ at `levelDbfs` (0 dBFS by default,
the maximum input level of a plugin). Both tones are moved to the nearest bin of the analysis window (65536 samples at 48 kHz, bin width 0.73 Hz):
- difference-frequency: 17999.6 Hz and 19999.5 Hz, 0.5 each;
- modulation: 41.0 Hz and 7992.8 Hz, 0.8 and 0.2.

0.5 s of settling before the window.

## Routine and analysis
1. Render the two-tone signal; spectrum of the window (FFT, rectangular window: every tone and every product lies on a bin, so no window function is needed).
2. **Band level** (5.2.10): $L_\text{band} = \sqrt{\sum_{|k\,\Delta f - f_c| \le B/2} 2|X_k|^2/N^2}$, the rms of all bins within +-B/2 of the centre. The
   centres are the products of the coherent tones (e.g. $f_2 - f_1$ = 1999.95 Hz).
3. **Difference-frequency distortion**: $\mathrm{DFD} = 20\log_{10}\left(\sqrt{L_{f_2-f_1}^2 + L_{2f_1-f_2}^2 + L_{2f_2-f_1}^2}\,/\,L_{f_1}\right)$, B = 500 Hz.
   Each product is also reported relative to $L_{f_1}$. At 44.1 kHz the 22 kHz band is cut at Nyquist; below 44 kHz the upper third-order product does
   not exist (reported as NaN).
4. **Modulation distortion**: $\mathrm{MD} = 20\log_{10}\left(\sqrt{L_{f_2-f_1}^2 + L_{f_2+f_1}^2}\,/\,L_{f_2}\right)$, B = 40 Hz. Also reported: the
   sidebands $f_2 \pm k f_1$ for k = 1, 2, 3 and the rms sum of all of them ("all sidebands").

## Band of validity and limits
- **Which orders each test sees.** For $y = \sum c_n x^n$, the product $f_2 - f_1$ comes from the even orders (2, 4, ...); $2f_1 - f_2$ and $2f_2 - f_1$
  come from the odd orders (3, 5, ...). DFD sees both. **AES17's modulation distortion uses only the first sidebands $f_2 \pm f_1$, so it sees only even
  orders**: a purely odd (symmetric) curve, such as $x + 0.05x^3$, a symmetric clipper or tanh, has MD at the floor. Its modulation shows at
  $f_2 \pm 2f_1$ (-32.8 dB here), which is why the unit also reports the higher sidebands.
- The bands collect everything inside them: noise, the device's own idle tones, and aliases that happen to fall there. With a fixed bandwidth in Hz the
  noise contribution does not depend on the window length (annex B.5; checked below). A noise-limited result is a statement about the noise in
  3 x 500 Hz, not about intermodulation.
- Aliases are part of the device's output and are measured where they land (the exact expectation in the tests includes them).
- The ratios are relative to the fundamental **at the output**: a filter after the non-linearity changes them. A low-pass that attenuates 18 kHz more
  than 2 kHz makes the DFD ratio worse, even positive.
- Devices with memory in the non-linearity (compressors) respond to the 41 Hz envelope. That is what modulation distortion was made for, and no closed
  form exists for them.

## Results for known test signals
From `MeasureIntermodulationTests` (2026-10-09, 0.31.0), 48 kHz, 0 dBFS peak unless noted. **Expected values** come from the exact spectrum of the
polynomial: each sine is two complex exponentials on bins $\pm k$, and $x^n$ is a cyclic convolution modulo N, so aliases and coinciding products add
with their phases. A filter after the curve multiplies each bin by its response. The same band sums are applied to it. "-" = no product (the
measured value is the float floor, -160 dB).

**Difference-frequency distortion** (dB re the lower fundamental; expected = measured to 0.001 dB):

| Device | Level | 2nd order $f_2 - f_1$ | lower 3rd $2f_1 - f_2$ | upper 3rd $2f_2 - f_1$ | DFD |
|---|---|---|---|---|---|
| $x + 0.1x^2$ | 0 dBFS | -26.021 | - | - | -26.021 |
| $x + 0.05x^3$ | 0 dBFS | - | -40.801 | -40.801 | -37.791 |
| $x + 0.1x^2 + 0.05x^3 + 0.02x^5$ | 0 dBFS | -26.326 | -37.842 | -37.842 | -25.753 |
| the same | -10 dBFS | -36.046 | -60.231 | -60.231 | -36.013 |
| the same | -20 dBFS | -46.023 | -80.527 | -80.527 | -46.020 |
| the same, then Butterworth low-pass 4th order 10 kHz | 0 dBFS | +13.498 | -26.309 | -77.674 | +13.499 |
| gain 0.5 (linear) | 0 dBFS | | | | -162.0 (floor) |

By hand: the $x^2$ term gives $c_2 a_1 a_2$ at $f_2 - f_1$, relative to $a_1$: $0.1 \cdot 0.5 = 0.05$ = -26.02 dB. The $x^3$ term gives
$\frac34 c_3 a_1^2 a_2$ at $2f_1 - f_2$, relative to the fundamental $a_1 + c_3(\frac34 a_1^3 + \frac32 a_1 a_2^2)$: -40.80 dB. 

**Modulation distortion** (dB re the upper fundamental; lower and upper sidebands are equal for a memoryless curve; expected = measured to 0.001 dB):

| Device | $f_2 \pm 41$ Hz | $f_2 \pm 82$ Hz | $f_2 \pm 123$ Hz | AES17 MD ($\pm 41$ Hz) | all sidebands |
|---|---|---|---|---|---|
| $x + 0.1x^2$ | -21.938 | - | - | -18.928 | -18.928 |
| $x + 0.05x^3$ | - | -32.815 | - | - (floor -162.7) | -29.805 |
| $x + 0.1x^2 + 0.05x^3 + 0.02x^5$ | -22.500 | -29.631 | - | -19.490 | -18.721 |

By hand: the $x^2$ term gives $c_2 a_1 a_2$ at $f_2 \pm f_1$, relative to $a_2$: $0.1 \cdot 0.8 = 0.08$ = -21.94 dB each, -18.93 dB for both.

**Noise** (16-bit quantizer with TPDF dither, total noise $q^2/4$, 0 dBFS): DFD -99.29 dB with a window of 65536 samples and -99.30 dB with 262144
samples; expected from the noise in 3 x 500 Hz: -99.34 dB. The result does not depend on the window length, as annex B.5 requires.

Tolerances in the test: band levels and ratios 0.01 dB (products below -130 dB: below -110 dB measured), fundamental levels 0.001 dB, noise 0.3 dB.
No oracle: the prototype has no intermodulation measurement.

**What the results teach**
- The level dependence tells the order: the second-order product falls 10 dB per 10 dB of level (-26.3, -36.0, -46.0 dB), the third-order products
  20 dB per 10 dB once the fifth order no longer contributes (-60.2 -> -80.5 dB; between 0 and -10 dBFS the $x^5$ term makes it 22.4 dB).
- The quadratic puts its product at 2 kHz, far below the tones; the cubic puts its products at 16 and 22 kHz, right next to them.
- AES17's modulation distortion is blind to symmetric curves; a saturator that is symmetric (tanh, a symmetric clipper) needs the second sidebands or DFD.
- A ratio "above 0 dB" is possible: the product passes a filter that attenuates the tones.

## Implementation
```cpp
pluginlab::measure::DifferenceFrequencySettings dfd;  // sampleRate, upperToneHz (20000), spacingHz (2000), levelDbfs (0, peak of the sum), bandwidthHz (500), settleSeconds, measureSeconds, channels
pluginlab::measure::DifferenceFrequencyResult a = pluginlab::measure::measureDifferenceFrequency(device, dfd);
// a.lowerHz, a.upperHz, a.windowStart, a.windowSamples, a.channels[c]: lowerFundamentalDbfs, secondOrderDb, lowerThirdOrderDb, upperThirdOrderDb, ratioDb, ratioPercent
pluginlab::measure::ModulationSettings md;           // lowToneHz (41), highToneHz (7993), amplitudeRatio (4), levelDbfs (0), bandwidthHz (40), sidebandOrders (3), ...
pluginlab::measure::ModulationResult b = pluginlab::measure::measureModulation(device, md);
// b.channels[c]: upperFundamentalDbfs, lowerSidebandDb[k - 1], upperSidebandDb[k - 1], ratioDb, ratioPercent, allSidebandsDb
```
