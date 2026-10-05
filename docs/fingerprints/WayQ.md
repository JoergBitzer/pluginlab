# Fingerprint: WayQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/WayQ-Linux-v1.0.0/linux/WayQ.vst3`
- format: VST3, manufacturer: Way.Net, version: 1.0.0
- channels: mono no, stereo yes

## Findings
- Latency at 96000 Hz: the plugin reports 0 samples, measured 1 samples
- The response differs by up to 1.44 dB between the sample rates (the usual cramping near the Nyquist frequency of a bilinear design is below about 1 dB up to 10 kHz)
- Delivery 'one instance, parameters set after prepare (stream)' fails: the same settings gave other output (A again: 4.6 dB). the settings that were delivered first differ from the same settings reached by a change (A: -1.3 dB, B: -200.0 dB): the first delivery was lost. 
- Delivery 'new instance per render, parameters set before prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -1.3 dB, B: -105.9 dB): the first delivery was lost. 
- Delivery 'new instance per render, parameters set after prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -1.3 dB, B: -105.9 dB): the first delivery was lost. 
- Delivery 'new instance per render, after prepare every parameter first set to another value, then the target' fails: the settings that were delivered first differ from the same settings reached by a change (A: -1.3 dB, B: -105.9 dB): the first delivery was lost. 

## Parameters
| # | name | min | default | max | steps | automatable | changes the audio (dB) |
|---|---|---|---|---|---|---|---|
| 0 | HP Freq A | 20.0000000 | 20.0000000 | 20000.0000000 | 2147483647 | yes | -2.2 |
| 1 | HP Q A | 0.3000000 | 0.7070000 | 10.0000000 | 2147483647 | yes | -23.6 |
| 2 | Mid Freq A | 20.0000000 | 632.4559937 | 20000.0000000 | 2147483647 | yes | -14.9 |
| 3 | Mid Gain A | -12.0000000 | 0.0000000 | 12.0000000 | 2147483647 | yes | -13.0 |
| 4 | Mid Q A | 0.3000000 | 0.7000000 | 17.3099995 | 2147483647 | yes | -32.7 |
| 5 | LP Freq A | 20.0000000 | 20000.0000000 | 20000.0000000 | 2147483647 | yes | 0.1 |
| 6 | LP Q A | 0.3000000 | 0.7070000 | 10.0000000 | 2147483647 | yes | 2.8 |
| 7 | Delta A | 0 | 0 | 2 | 2147483647 | yes | 6.0 |
| 8 | Bass Tilt Freq A | 20.0000000 | 249.9999542 | 20000.0000000 | 2147483647 | yes | no |
| 9 | Bass Tilt Gain A | -12.0000000 | 0.0000000 | 12.0000000 | 2147483647 | yes | no |
| 10 | Treb Tilt Freq A | 20.0000000 | 4000.0000000 | 20000.0000000 | 2147483647 | yes | no |
| 11 | Treb Tilt Gain A | -12.0000000 | 0.0000000 | 12.0000000 | 2147483647 | yes | no |
| 12 | EQ Mode X A | Off | Off | On | 2 | yes | -2.6 |
| 13 | HP Freq B | 20.0000000 | 20.0000000 | 20000.0000000 | 2147483647 | yes | no |
| 14 | HP Q B | 0.3000000 | 0.7070000 | 10.0000000 | 2147483647 | yes | no |
| 15 | Mid Freq B | 20.0000000 | 632.4559937 | 20000.0000000 | 2147483647 | yes | no |
| 16 | Mid Gain B | -12.0000000 | 0.0000000 | 12.0000000 | 2147483647 | yes | no |
| 17 | Mid Q B | 0.3000000 | 0.7000000 | 17.3099995 | 2147483647 | yes | no |
| 18 | LP Freq B | 20.0000000 | 20000.0000000 | 20000.0000000 | 2147483647 | yes | no |
| 19 | LP Q B | 0.3000000 | 0.7070000 | 10.0000000 | 2147483647 | yes | no |
| 20 | Delta B | 0 | 0 | 2 | 2147483647 | yes | no |
| 21 | Bass Tilt Freq B | 20.0000000 | 249.9999542 | 20000.0000000 | 2147483647 | yes | no |
| 22 | Bass Tilt Gain B | -12.0000000 | 0.0000000 | 12.0000000 | 2147483647 | yes | no |
| 23 | Treb Tilt Freq B | 20.0000000 | 4000.0000000 | 20000.0000000 | 2147483647 | yes | no |
| 24 | Treb Tilt Gain B | -12.0000000 | 0.0000000 | 12.0000000 | 2147483647 | yes | no |
| 25 | EQ Mode X B | Off | Off | On | 2 | yes | no |
| 26 | Bypass | Off | Off | On | 2 | yes | -2.6 |
| 27 | M/S Mode | Off | Off | On | 2 | yes | 0.0 |
| 28 | Mix | 0.0000000 | 1.0000000 | 1.0000000 | 2147483647 | yes | -1.9 |

## Sample rates (the parameters that change the audio at 0.75)
| rate | latency reported | latency measured | feature frequency | feature gain |
|---|---|---|---|---|
| 44100 Hz | 0 | 0 | 4806.29 Hz | 35.32 dB |
| 48000 Hz | 0 | 0 | 4806.29 Hz | 35.33 dB |
| 96000 Hz | 0 | 1 | 4806.29 Hz | 35.39 dB |

Largest difference of the response between the rates (100 Hz - 19.8 kHz): 1.44 dB.
Feature frequency at the highest rate over the lowest: 1.000 (the rates: 2.177).

## Response at the setting B (dB; every ~10th point of the axis)
| frequency | 44100 Hz | 48000 Hz | 96000 Hz |
|---|---|---|---|
| 100 Hz | -9.03 | -9.03 | -9.03 |
| 151.649 Hz | -9.02 | -9.02 | -9.01 |
| 229.974 Hz | -8.98 | -8.98 | -8.98 |
| 348.754 Hz | -8.91 | -8.91 | -8.90 |
| 528.882 Hz | -8.74 | -8.73 | -8.72 |
| 802.045 Hz | -8.34 | -8.33 | -8.29 |
| 1216.29 Hz | -7.39 | -7.37 | -7.29 |
| 1844.5 Hz | -5.07 | -5.03 | -4.85 |
| 2797.17 Hz | 1.07 | 1.17 | 1.56 |
| 4241.88 Hz | 23.50 | 23.64 | 24.18 |
| 6432.77 Hz | 7.71 | 7.96 | 8.98 |
| 9755.23 Hz | -4.08 | -3.80 | -2.65 |
| 14793.7 Hz | -8.00 | -7.76 | -6.67 |

## Delivery of parameters (A, A, B, A)
| way | repeatable | reacts | as after a change | result | differences |
|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | no | yes | no | FAILS | A again -93.6 / 4.6 dB, B against A 19.0 dB, A and B against the references -1.3 / -200.0 dB |
| new instance per render, parameters set before prepare | yes | yes | no | FAILS | A again -200.0 / -200.0 dB, B against A 19.0 dB, A and B against the references -1.3 / -105.9 dB |
| new instance per render, parameters set after prepare | yes | yes | no | FAILS | A again -200.0 / -200.0 dB, B against A 19.0 dB, A and B against the references -1.3 / -105.9 dB |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | no | FAILS | A again -200.0 / -200.0 dB, B against A 19.0 dB, A and B against the references -1.3 / -105.9 dB |

No way of delivering the parameters passed the test.

## Block sizes (difference to block size 512)
| block size | difference |
|---|---|
| 32 | -154.0 dB |
| 64 | -153.1 dB |
| 128 | -153.2 dB |
| 256 | -155.0 dB |
| 1024 | -150.7 dB |
| 2048 | -158.8 dB |
| 509 | -170.9 dB |

## Other
- deterministic: yes
- output stays finite: yes
- recovers from parameter jumps: yes
- digital silence in gives digital silence out: yes

