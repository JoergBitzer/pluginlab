# Spectrogram of the sweep

Part of the fingerprint report (0.52.0, on the author's request). Code:
`src/measure/Spectrogram.cpp` (`computeSpectrogram`, `makeSynchronizedSweep`),
`src/ui/SpectrogramImage.cpp` (the picture), `apps/host/HostFingerprint.cpp`
(the section of the report); tests `tests/MeasureSpectrogramTests.cpp`.

## Purpose
One picture that shows what a plugin does to a sweep, before any number: the
sweep as a bright rising line, harmonics as fainter lines above it (2, 3, ...
times its frequency), aliasing as lines that fold back down at Nyquist, noise
as a uniform glow, filters as a line that gets darker, and latency, ringing
or a signal of its own after the end of the sweep.

## The signal
A synchronized exponential sweep (Novak, Lotton, Simon, JAES 63(10), 2015),
20 Hz ... 20 kHz in 3 s, peak -6 dBFS, the same on every input channel,
followed by 0.5 s of silence; through the plugin at its default setting
(the careful delivery of the plugin device, W7.11), 48 kHz; output channel 1
is shown.

## The analysis (as JadeSpectrogram2, without its overlays)
| Step | What |
|---|---|
| window | Blackman-Harris, 4 terms (sidelobes below -92 dB). Hann (-31 dB) is enough for JadeSpectrogram's default range of 100 dB, but at -120 dB its leakage around a strong low-frequency sweep showed as vertical streaks |
| two resolutions | a logarithmic axis needs fine frequency steps at the low end (8192 samples: 5.9 Hz per bin) and fine time steps at the high end, where an exponential sweep is fast (2048 samples: 42.7 ms). Both transforms use the same hop (512 samples, 10.7 ms, 75 % of the short frame) and are centred on the same instants; each bin shows the smaller of the two levels: the long one smears in time, the short one in frequency, and each smear is absent in the other. (A crossover at 400 Hz left a hard edge of the short transform's leakage.) |
| level | dB re a full-scale sine: a sine of amplitude 1 on a bin reads 0 dB (2 abs(X) / sum of the window) |
| frequency axis | logarithmic, 20 Hz ... 20 kHz, labels 1-2-5 per decade; a pixel row that covers several bins shows their maximum (narrow lines stay visible), one that covers less than a bin interpolates between the two bins around its centre |
| time axis | seconds, steps of 0.1, 0.2, 0.5, 1, 2 or 5 s (at most about ten labels); a pixel column shows the maximum of the frames in it |
| colours | plasma (matplotlib, CC0; perceptually uniform, readable with the usual forms of colour blindness), -120 dB (dark violet) ... 0 dB (yellow), colour bar in steps of 20 dB |

The report gives every parameter in a table below the picture. The picture is
a PNG next to the report (`<report>_sweep_spectrogram.png`); the report
window of the Developer page shows it.

## Results
The author's ACM-3SA (an analogue console model, local): the sweep, its third
harmonic about 85 dB down, and that harmonic's aliasing folding back from
Nyquist between 2.6 and 3.1 s. The tests: a sine of amplitude 0.5 on a bin
reads -6.02 dB within 0.05 dB with both windows and with one or two
resolutions; an octave below it the frame is empty (below -86 dB).
