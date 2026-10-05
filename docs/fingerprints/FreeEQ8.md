# Fingerprint: FreeEQ8

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/FreeEQ8/FreeEQ8-v2.3.1-Linux/FreeEQ8.vst3`
- format: VST3, manufacturer: TizWildinEntertainment, version: 2.3.1
- channels: mono no, stereo yes

## Findings
- The response differs by up to 14.77 dB between the sample rates (the usual cramping near the Nyquist frequency of a bilinear design is below about 1 dB up to 10 kHz)

## Parameters
| # | name | min | default | max | steps | automatable | changes the audio (dB) |
|---|---|---|---|---|---|---|---|
| 0 | Output Gain | -24.00 | 0.00 | 24.00 | 2147483647 | yes | 9.5 |
| 1 | Scale | 0.10 | 1.00 | 2.00 | 2147483647 | yes | no |
| 2 | Adaptive Q | Off | Off | On | 2 | yes | -3.5 |
| 3 | Oversampling | 1x | 1x | 8x | 4 | yes | 3.0 |
| 4 | Processing Mode | Stereo | Stereo | Mid-Side | 2 | yes | no |
| 5 | Linear Phase | Off | Off | On | 2 | yes | -0.0 |
| 6 | Auto Gain | Off | Off | On | 2 | yes | -1.0 |
| 7 | Intent Mode | None | None | Master Polish | 5 | yes | no |
| 8 | Band 1 On | Off | On | On | 2 | yes | 2.9 |
| 9 | Band 1 Solo | Off | Off | On | 2 | yes | -0.2 |
| 10 | Band 1 Type | Bell | Bell | Bandpass | 6 | yes | 0.0 |
| 11 | Band 1 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | 2.9 |
| 12 | Band 1 Channel | Both | Both | R / Side | 3 | yes | 3.3 |
| 13 | Band 1 Link | -- | -- | B | 3 | yes | no |
| 14 | Band 1 Freq | 20.000 | 80.000 | 20000.000 | 2147483647 | yes | 3.3 |
| 15 | Band 1 Q | 0.100 | 1.000 | 24.000 | 2147483647 | yes | 0.2 |
| 16 | Band 1 Gain | -24.00 | 0.00 | 24.00 | 2147483647 | yes | -14.5 |
| 17 | Band 1 Drive | 0.0 | 0.0 | 100.0 | 2147483647 | yes | 15.5 |
| 18 | Band 1 Dyn On | Off | Off | On | 2 | yes | no |
| 19 | Band 1 Threshold | -60.0 | -20.0 | 0.0 | 2147483647 | yes | no |
| 20 | Band 1 Ratio | 1.0 | 4.0 | 20.0 | 2147483647 | yes | no |
| 21 | Band 1 Attack | 0.1 | 10.0 | 100.0 | 2147483647 | yes | no |
| 22 | Band 1 Release | 1 | 100 | 1000 | 2147483647 | yes | no |
| 23 | Band 2 On | Off | On | On | 2 | yes | -4.7 |
| 24 | Band 2 Solo | Off | Off | On | 2 | yes | 0.1 |
| 25 | Band 2 Type | Bell | Bell | Bandpass | 6 | yes | 0.1 |
| 26 | Band 2 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | 1.7 |
| 27 | Band 2 Channel | Both | Both | R / Side | 3 | yes | -3.2 |
| 28 | Band 2 Link | -- | -- | B | 3 | yes | no |
| 29 | Band 2 Freq | 20.000 | 250.000 | 20000.000 | 2147483647 | yes | -3.3 |
| 30 | Band 2 Q | 0.100 | 1.000 | 24.000 | 2147483647 | yes | -2.7 |
| 31 | Band 2 Gain | -24.00 | 0.00 | 24.00 | 2147483647 | yes | -12.0 |
| 32 | Band 2 Drive | 0.0 | 0.0 | 100.0 | 2147483647 | yes | 15.5 |
| 33 | Band 2 Dyn On | Off | Off | On | 2 | yes | no |
| 34 | Band 2 Threshold | -60.0 | -20.0 | 0.0 | 2147483647 | yes | no |
| 35 | Band 2 Ratio | 1.0 | 4.0 | 20.0 | 2147483647 | yes | no |
| 36 | Band 2 Attack | 0.1 | 10.0 | 100.0 | 2147483647 | yes | no |
| 37 | Band 2 Release | 1 | 100 | 1000 | 2147483647 | yes | no |
| 38 | Band 3 On | Off | On | On | 2 | yes | -8.0 |
| 39 | Band 3 Solo | Off | Off | On | 2 | yes | 0.1 |
| 40 | Band 3 Type | Bell | Bell | Bandpass | 6 | yes | 0.1 |
| 41 | Band 3 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | -1.5 |
| 42 | Band 3 Channel | Both | Both | R / Side | 3 | yes | -7.5 |
| 43 | Band 3 Link | -- | -- | B | 3 | yes | no |
| 44 | Band 3 Freq | 20.000 | 500.000 | 20000.000 | 2147483647 | yes | -7.8 |
| 45 | Band 3 Q | 0.100 | 1.000 | 24.000 | 2147483647 | yes | -8.5 |
| 46 | Band 3 Gain | -24.00 | 0.00 | 24.00 | 2147483647 | yes | -8.2 |
| 47 | Band 3 Drive | 0.0 | 0.0 | 100.0 | 2147483647 | yes | 15.5 |
| 48 | Band 3 Dyn On | Off | Off | On | 2 | yes | no |
| 49 | Band 3 Threshold | -60.0 | -20.0 | 0.0 | 2147483647 | yes | no |
| 50 | Band 3 Ratio | 1.0 | 4.0 | 20.0 | 2147483647 | yes | no |
| 51 | Band 3 Attack | 0.1 | 10.0 | 100.0 | 2147483647 | yes | no |
| 52 | Band 3 Release | 1 | 100 | 1000 | 2147483647 | yes | no |
| 53 | Band 4 On | Off | On | On | 2 | yes | -9.5 |
| 54 | Band 4 Solo | Off | Off | On | 2 | yes | 0.1 |
| 55 | Band 4 Type | Bell | Bell | Bandpass | 6 | yes | 0.2 |
| 56 | Band 4 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | -4.9 |
| 57 | Band 4 Channel | Both | Both | R / Side | 3 | yes | -11.8 |
| 58 | Band 4 Link | -- | -- | B | 3 | yes | no |
| 59 | Band 4 Freq | 20.000 | 1000.000 | 20000.000 | 2147483647 | yes | -12.4 |
| 60 | Band 4 Q | 0.100 | 1.000 | 24.000 | 2147483647 | yes | -12.7 |
| 61 | Band 4 Gain | -24.00 | 0.00 | 24.00 | 2147483647 | yes | -5.5 |
| 62 | Band 4 Drive | 0.0 | 0.0 | 100.0 | 2147483647 | yes | 15.5 |
| 63 | Band 4 Dyn On | Off | Off | On | 2 | yes | no |

## Sample rates (the parameters that change the audio at 0.75)
| rate | latency reported | latency measured | feature frequency | feature gain |
|---|---|---|---|---|
| 44100 Hz | 0 | 0 | 19800 Hz | 28.38 dB |
| 48000 Hz | 0 | 0 | 18992.5 Hz | 24.75 dB |
| 96000 Hz | 0 | 0 | 4422.24 Hz | 23.06 dB |

Largest difference of the response between the rates (100 Hz - 19.8 kHz): 14.77 dB.
Feature frequency at the highest rate over the lowest: 0.223 (the rates: 2.177).

## Response at the setting B (dB; every ~10th point of the axis)
| frequency | 44100 Hz | 48000 Hz | 96000 Hz |
|---|---|---|---|
| 100 Hz | 20.53 | 20.53 | 20.52 |
| 151.649 Hz | 20.55 | 20.54 | 20.52 |
| 229.974 Hz | 20.59 | 20.57 | 20.53 |
| 348.754 Hz | 20.68 | 20.65 | 20.55 |
| 528.882 Hz | 20.91 | 20.84 | 20.59 |
| 802.045 Hz | 21.44 | 21.29 | 20.70 |
| 1216.29 Hz | 22.36 | 22.16 | 20.95 |
| 1844.5 Hz | 23.04 | 22.98 | 21.54 |
| 2797.17 Hz | 22.78 | 22.93 | 22.49 |
| 4241.88 Hz | 21.18 | 21.26 | 23.06 |
| 6432.77 Hz | 18.34 | 19.89 | 22.61 |
| 9755.23 Hz | 17.33 | 19.17 | 21.13 |
| 14793.7 Hz | 18.21 | 15.73 | 17.13 |

## Delivery of parameters (A, A, B, A)
| way | repeatable | reacts | as after a change | result | differences |
|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 35.9 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 35.9 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 35.9 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 35.9 dB, A and B against the references -200.0 / -200.0 dB |

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

