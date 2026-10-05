# Fingerprint: PeakEqualizer

- file: `/home/bitzer/.vst3/PeakEqualizer.vst3`
- format: VST3, manufacturer: Jade_Hochschule, version: 0.1.0
- channels: mono yes, stereo yes

## Findings
- Latency at 44100 Hz: the plugin reports 0 samples, measured 88 samples
- Latency at 48000 Hz: the plugin reports 0 samples, measured 96 samples
- Latency at 96000 Hz: the plugin reports 0 samples, measured 192 samples

## Parameters
| # | name | min | default | max | steps | automatable | changes the audio (dB) |
|---|---|---|---|---|---|---|---|
| 0 | Gain | -24.0000000000000000000000000000 | 0.000000000000000000000000000000 | 24.00000000000000000000000000000 | 2147483647 | yes | -5.6 |
| 1 | Q | 0.089999996125698089599609375000 | 1.000000000000000000000000000000 | 10.00000000000000000000000000000 | 2147483647 | yes | -7.1 |
| 2 | Freq | 50.00000000000000000000000000000 | 1000.000000000000000000000000000 | 15000.00000000000000000000000000 | 2147483647 | yes | -1.1 |
| 3 | Bypass | Off | Off | On | 2 | yes | 3.0 |

## Sample rates (the parameters that change the audio at 0.75)
| rate | latency reported | latency measured | feature frequency | feature gain |
|---|---|---|---|---|
| 44100 Hz | 0 | 88 | 3591.06 Hz | 11.99 dB |
| 48000 Hz | 0 | 96 | 3591.06 Hz | 11.99 dB |
| 96000 Hz | 0 | 192 | 3591.06 Hz | 11.99 dB |

Largest difference of the response between the rates (100 Hz - 19.8 kHz): 0.22 dB.
Feature frequency at the highest rate over the lowest: 1.000 (the rates: 2.177).

## Response at the setting B (dB; every ~10th point of the axis)
| frequency | 44100 Hz | 48000 Hz | 96000 Hz |
|---|---|---|---|
| 100 Hz | 0.00 | 0.00 | 0.00 |
| 151.649 Hz | 0.00 | 0.00 | 0.00 |
| 229.974 Hz | 0.01 | 0.01 | 0.01 |
| 348.754 Hz | 0.01 | 0.01 | 0.02 |
| 528.882 Hz | 0.03 | 0.03 | 0.04 |
| 802.045 Hz | 0.08 | 0.08 | 0.09 |
| 1216.29 Hz | 0.22 | 0.22 | 0.23 |
| 1844.5 Hz | 0.67 | 0.68 | 0.70 |
| 2797.17 Hz | 3.45 | 3.48 | 3.58 |
| 4241.88 Hz | 5.55 | 5.59 | 5.76 |
| 6432.77 Hz | 0.80 | 0.82 | 0.92 |
| 9755.23 Hz | 0.19 | 0.21 | 0.26 |
| 14793.7 Hz | 0.04 | 0.05 | 0.09 |

## Delivery of parameters (A, A, B, A)
| way | repeatable | reacts | as after a change | result | differences |
|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A -5.2 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A -5.2 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A -5.2 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A -5.2 dB, A and B against the references -200.0 / -200.0 dB |

Most careful way that works: new instance per render, after prepare every parameter first set to another value, then the target.

## Block sizes (difference to block size 512)
| block size | difference |
|---|---|
| 32 | -200.0 dB |
| 64 | -200.0 dB |
| 128 | -200.0 dB |
| 256 | -200.0 dB |
| 1024 | -200.0 dB |
| 2048 | -200.0 dB |
| 509 | -200.0 dB |

## Other
- deterministic: yes
- output stays finite: yes
- recovers from parameter jumps: yes
- digital silence in gives digital silence out: yes

