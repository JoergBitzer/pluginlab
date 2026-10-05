# Fingerprint: WSTD MSEQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/wstd/wstd-mseq-v1.0.1/WSTD_MSEQ.vst3`
- format: VST3, manufacturer: Wasted Audio, version: 1.0.1
- channels: mono no, stereo yes

## Findings
- Latency at 44100 Hz: the plugin reports 0 samples, measured 2 samples
- Latency at 48000 Hz: the plugin reports 0 samples, measured 2 samples
- Latency at 96000 Hz: the plugin reports 0 samples, measured 2 samples

## Parameters
| # | name | min | default | max | steps | automatable | changes the audio (dB) |
|---|---|---|---|---|---|---|---|
| 0 | Buffer Size | 0 | 0 | 32768 | 32768 | no | no |
| 1 | Sample Rate | 0 | 0 | 384000 | 2147483647 | no | no |
| 2 | m High | -inf | 0.000000 | 15.000000 | 2147483647 | yes | 1.7 |
| 3 | m Low | -inf | 0.000000 | 15.000000 | 2147483647 | yes | -12.8 |
| 4 | m Mid | -inf | 0.000000 | 15.000000 | 2147483647 | yes | -3.9 |
| 5 | m Mid Freq | 313.299988 | 1337.000000 | 5705.600098 | 2147483647 | yes | -7.2 |
| 6 | s High | -inf | 0.000000 | 15.000000 | 2147483647 | yes | no |
| 7 | s Low | -inf | 0.000000 | 15.000000 | 2147483647 | yes | no |
| 8 | s Mid | -inf | 0.000000 | 15.000000 | 2147483647 | yes | no |
| 9 | s Mid Freq | 313.299988 | 1337.000000 | 5705.600098 | 2147483647 | yes | no |

## Sample rates (the parameters that change the audio at 0.75)
| rate | latency reported | latency measured | feature frequency | feature gain |
|---|---|---|---|---|
| 44100 Hz | 0 | 2 | 4806.29 Hz | 11.44 dB |
| 48000 Hz | 0 | 2 | 4806.29 Hz | 11.43 dB |
| 96000 Hz | 0 | 2 | 4806.29 Hz | 11.37 dB |

Largest difference of the response between the rates (100 Hz - 19.8 kHz): 0.65 dB.
Feature frequency at the highest rate over the lowest: 1.000 (the rates: 2.177).

## Response at the setting B (dB; every ~10th point of the axis)
| frequency | 44100 Hz | 48000 Hz | 96000 Hz |
|---|---|---|---|
| 100 Hz | 7.22 | 7.22 | 7.22 |
| 151.649 Hz | 6.86 | 6.86 | 6.86 |
| 229.974 Hz | 6.07 | 6.07 | 6.06 |
| 348.754 Hz | 4.33 | 4.33 | 4.31 |
| 528.882 Hz | 0.48 | 0.47 | 0.43 |
| 802.045 Hz | -8.88 | -8.84 | -8.71 |
| 1216.29 Hz | -0.88 | -0.83 | -0.66 |
| 1844.5 Hz | 5.97 | 5.99 | 6.05 |
| 2797.17 Hz | 9.70 | 9.69 | 9.68 |
| 4241.88 Hz | 11.35 | 11.33 | 11.27 |
| 6432.77 Hz | 11.06 | 11.06 | 11.06 |
| 9755.23 Hz | 9.61 | 9.68 | 9.91 |
| 14793.7 Hz | 8.15 | 8.27 | 8.76 |

## Delivery of parameters (A, A, B, A)
| way | repeatable | reacts | as after a change | result | differences |
|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 5.6 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 5.6 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 5.6 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 5.6 dB, A and B against the references -200.0 / -200.0 dB |

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

