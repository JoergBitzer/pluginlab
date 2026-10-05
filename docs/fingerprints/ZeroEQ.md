# Fingerprint: ZeroEQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ZeroEQ_1.0.7_Linux_VST3_LV2_CLAP_Standalone/VST3/ZeroEQ.vst3`
- format: VST3, manufacturer: Jun Murakami, version: 1.0.7
- channels: mono yes, stereo yes

## Findings
- The response differs by up to 42.95 dB between the sample rates (the usual cramping near the Nyquist frequency of a bilinear design is below about 1 dB up to 10 kHz)

## Parameters
| # | name | min | default | max | steps | automatable | changes the audio (dB) |
|---|---|---|---|---|---|---|---|
| 0 | Bypass | Off | Off | On | 2 | yes | 14.0 |
| 1 | Output Gain | -24.0 | 0.0 | 24.0 | 2147483647 | yes | 9.5 |
| 2 | Analyzer | Off | Pre+Post | Pre+Post | 4 | yes | no |
| 3 | Bottom Panel Open | Off | On | On | 2 | no | no |
| 4 | EQ dB Range | +/-3 dB | +/-12 dB | +/-32 dB | 5 | no | no |
| 5 | Band 1 On | Off | Off | On | 2 | yes | -21.5 |
| 6 | Band 1 Type | Bell | HighPass | Notch | 6 | yes | no |
| 7 | Band 1 Freq | 20.0000000 | 30.0000000 | 20000.0000000 | 2147483647 | yes | no |
| 8 | Band 1 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | no |
| 9 | Band 1 Q | 0.1000000 | 0.7070000 | 18.0000000 | 2147483647 | yes | no |
| 10 | Band 1 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | no |
| 11 | Band 2 On | Off | Off | On | 2 | yes | -18.1 |
| 12 | Band 2 Type | Bell | HighPass | Notch | 6 | yes | no |
| 13 | Band 2 Freq | 20.0000000 | 60.0000000 | 20000.0000000 | 2147483647 | yes | no |
| 14 | Band 2 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | no |
| 15 | Band 2 Q | 0.1000000 | 0.7070000 | 18.0000000 | 2147483647 | yes | no |
| 16 | Band 2 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | no |
| 17 | Band 3 On | Off | On | On | 2 | yes | 4.6 |
| 18 | Band 3 Type | Bell | LowShelf | Notch | 6 | yes | 0.0 |
| 19 | Band 3 Freq | 20.0000000 | 119.9999924 | 20000.0000000 | 2147483647 | yes | 4.6 |
| 20 | Band 3 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | -9.1 |
| 21 | Band 3 Q | 0.1000000 | 0.7070000 | 18.0000000 | 2147483647 | yes | no |
| 22 | Band 3 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | 25.1 |
| 23 | Band 4 On | Off | On | On | 2 | yes | -6.1 |
| 24 | Band 4 Type | Bell | Bell | Notch | 6 | yes | 0.0 |
| 25 | Band 4 Freq | 20.0000000 | 250.0000153 | 20000.0000000 | 2147483647 | yes | 9.3 |
| 26 | Band 4 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | -8.1 |
| 27 | Band 4 Q | 0.1000000 | 1.0000000 | 18.0000000 | 2147483647 | yes | no |
| 28 | Band 4 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | 12.9 |
| 29 | Band 5 On | Off | On | On | 2 | yes | -11.9 |
| 30 | Band 5 Type | Bell | Bell | Notch | 6 | yes | 0.1 |
| 31 | Band 5 Freq | 20.0000000 | 499.9999695 | 20000.0000000 | 2147483647 | yes | 10.5 |
| 32 | Band 5 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | -4.1 |
| 33 | Band 5 Q | 0.1000000 | 1.0000000 | 18.0000000 | 2147483647 | yes | no |
| 34 | Band 5 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -11.8 |
| 35 | Band 6 On | Off | On | On | 2 | yes | -17.8 |
| 36 | Band 6 Type | Bell | Bell | Notch | 6 | yes | 0.2 |
| 37 | Band 6 Freq | 20.0000000 | 1000.0000000 | 20000.0000000 | 2147483647 | yes | 10.8 |
| 38 | Band 6 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | -1.5 |
| 39 | Band 6 Q | 0.1000000 | 1.0000000 | 18.0000000 | 2147483647 | yes | no |
| 40 | Band 6 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -19.6 |
| 41 | Band 7 On | Off | On | On | 2 | yes | -23.8 |
| 42 | Band 7 Type | Bell | Bell | Notch | 6 | yes | 0.3 |
| 43 | Band 7 Freq | 20.0000000 | 2000.0002441 | 20000.0000000 | 2147483647 | yes | 10.9 |
| 44 | Band 7 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | 1.4 |
| 45 | Band 7 Q | 0.1000000 | 1.0000000 | 18.0000000 | 2147483647 | yes | no |
| 46 | Band 7 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -25.6 |
| 47 | Band 8 On | Off | On | On | 2 | yes | -29.9 |
| 48 | Band 8 Type | Bell | Bell | Notch | 6 | yes | 0.6 |
| 49 | Band 8 Freq | 20.0000000 | 3999.9995117 | 20000.0000000 | 2147483647 | yes | 11.0 |
| 50 | Band 8 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | 4.1 |
| 51 | Band 8 Q | 0.1000000 | 1.0000000 | 18.0000000 | 2147483647 | yes | no |
| 52 | Band 8 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -31.8 |
| 53 | Band 9 On | Off | On | On | 2 | yes | -36.6 |
| 54 | Band 9 Type | Bell | Bell | Notch | 6 | yes | 0.8 |
| 55 | Band 9 Freq | 20.0000000 | 7999.9995117 | 20000.0000000 | 2147483647 | yes | 11.0 |
| 56 | Band 9 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | 6.2 |
| 57 | Band 9 Q | 0.1000000 | 1.0000000 | 18.0000000 | 2147483647 | yes | no |
| 58 | Band 9 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -38.5 |
| 59 | Band 10 On | Off | On | On | 2 | yes | -41.4 |
| 60 | Band 10 Type | Bell | HighShelf | Notch | 6 | yes | 0.7 |
| 61 | Band 10 Freq | 20.0000000 | 12000.0009766 | 20000.0000000 | 2147483647 | yes | 11.0 |
| 62 | Band 10 Gain | -32.0 | 0.0 | 32.0 | 2147483647 | yes | 10.5 |
| 63 | Band 10 Q | 0.1000000 | 0.7070000 | 18.0000000 | 2147483647 | yes | no |

## Sample rates (the parameters that change the audio at 0.75)
| rate | latency reported | latency measured | feature frequency | feature gain |
|---|---|---|---|---|
| 44100 Hz | 0 | 0 | 3591.06 Hz | 335.72 dB |
| 48000 Hz | 0 | 0 | 3591.06 Hz | 335.76 dB |
| 96000 Hz | 0 | 0 | 3591.06 Hz | 335.93 dB |

Largest difference of the response between the rates (100 Hz - 19.8 kHz): 42.95 dB.
Feature frequency at the highest rate over the lowest: 1.000 (the rates: 2.177).

## Response at the setting B (dB; every ~10th point of the axis)
| frequency | 44100 Hz | 48000 Hz | 96000 Hz |
|---|---|---|---|
| 100 Hz | 191.46 | 188.60 | 199.38 |
| 151.649 Hz | 190.88 | 189.49 | 194.58 |
| 229.974 Hz | 196.82 | 189.69 | 185.69 |
| 348.754 Hz | 189.55 | 157.19 | 200.15 |
| 528.882 Hz | 182.52 | 195.36 | 199.48 |
| 802.045 Hz | 188.17 | 194.48 | 200.70 |
| 1216.29 Hz | 188.11 | 176.82 | 203.52 |
| 1844.5 Hz | 196.03 | 198.04 | 208.88 |
| 2797.17 Hz | 195.03 | 193.70 | 205.65 |
| 4241.88 Hz | 199.33 | 199.74 | 207.33 |
| 6432.77 Hz | 179.60 | 187.03 | 194.33 |
| 9755.23 Hz | 173.57 | 168.37 | 182.97 |
| 14793.7 Hz | 176.09 | 181.52 | 173.99 |

## Delivery of parameters (A, A, B, A)
| way | repeatable | reacts | as after a change | result | differences |
|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | A again -200.0 / -91.6 dB, B against A 317.8 dB, A and B against the references -91.6 / -200.0 dB |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 317.8 dB, A and B against the references -91.6 / -200.0 dB |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 317.8 dB, A and B against the references -91.6 / -200.0 dB |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A 317.8 dB, A and B against the references -91.6 / -200.0 dB |

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

