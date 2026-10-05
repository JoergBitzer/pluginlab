# Fingerprint: EQoder

- file: `/home/bitzer/.vst3/EQoder.vst3`
- format: VST3, manufacturer: Jade Hochschule, version: 0.0.1
- channels: mono yes, stereo yes

## Findings
- Latency at 44100 Hz: the plugin reports 0 samples, measured 87 samples
- Latency at 48000 Hz: the plugin reports 0 samples, measured 95 samples
- Latency at 96000 Hz: the plugin reports 0 samples, measured 191 samples

## Parameters
| # | name | min | default | max | steps | automatable | changes the audio (dB) |
|---|---|---|---|---|---|---|---|
| 0 | Number of FilterUnits | 1.000000000000000000000000000000 | 6.000000000000000000000000000000 | 24.00000000000000000000000000000 | 2147483647 | yes | no |
| 1 | Number of Filters | 1.000000000000000000000000000000 | 7.000000000000000000000000000000 | 20.00000000000000000000000000000 | 2147483647 | yes | no |
| 2 | Gain at f0 | 0.000000000000000000000000000000 | 6.000000000000000000000000000000 | 20.00000000000000000000000000000 | 2147483647 | yes | no |
| 3 | Gain at last | 0.000000000000000000000000000000 | 6.000000000000000000000000000000 | 20.00000000000000000000000000000 | 2147483647 | yes | no |
| 4 | Form of Gains | 0.000000000000000000000000000000 | 1.000000000000000000000000000000 | 2.000000000000000000000000000000 | 2147483647 | yes | no |
| 5 | Q | 0.300000000000000044408920985006 | 20.00000000000000000000000000000 | 40.00000000000000000000000000000 | 2147483647 | yes | no |
| 6 | BWSpread | 0.300000000000000044408920985006 | 1.000000000000000000000000000000 | 4.000000000000000000000000000000 | 2147483647 | yes | no |
| 7 | FreqSpread | -2.06250000000000000000000000000 | 0.000000000000000000000000000000 | 2.125000000000000000000000000000 | 2147483647 | yes | no |
| 8 | OutGain | -89.7500000000000000000000000000 | 0.000000000000000000000000000000 | 10.00000000000000000000000000000 | 2147483647 | yes | -0.0 |
| 9 | SwitchParallel | 0.000000000000000000000000000000 | 0.000000000000000000000000000000 | 1.000000000000000000000000000000 | 2147483647 | yes | no |
| 10 | EnvAttack | 0.100000000000000005551115123125 | 150.0000000000000000000000000000 | 10000.00000000000000000000000000 | 2147483647 | yes | no |
| 11 | EnvDecay | 10.00000000000000000000000000000 | 150.0000000000000000000000000000 | 10000.00000000000000000000000000 | 2147483647 | yes | no |
| 12 | EnvRelease | 10.00000000000000000000000000000 | 150.0000000000000000000000000000 | 10000.00000000000000000000000000 | 2147483647 | yes | no |
| 13 | EnvDelay | 0.000000000000000000000000000000 | 0.000000000000000000000000000000 | 250.0000000000000000000000000000 | 2147483647 | yes | no |
| 14 | EnvHold | 0.000000000000000000000000000000 | 0.000000000000000000000000000000 | 250.0000000000000000000000000000 | 2147483647 | yes | no |
| 15 | EnvSustainLevel | 0.000000000000000000000000000000 | 1.000000000000000000000000000000 | 1.000000000000000000000000000000 | 2147483647 | yes | no |
| 16 | EnvLevel | 0.000000000000000000000000000000 | 1.000000000000000000000000000000 | 1.000000000000000000000000000000 | 2147483647 | yes | no |
| 17 | EnvInvert | Off | Off | On | 2 | yes | no |
| 18 | Bypass | Off | Off | On | 2 | yes | 3.0 |
| 19 | MIDI CC 0|0 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 20 | MIDI CC 0|1 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 21 | MIDI CC 0|2 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 22 | MIDI CC 0|3 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 23 | MIDI CC 0|4 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 24 | MIDI CC 0|5 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 25 | MIDI CC 0|6 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 26 | MIDI CC 0|7 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 27 | MIDI CC 0|8 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 28 | MIDI CC 0|9 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 29 | MIDI CC 0|10 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 30 | MIDI CC 0|11 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 31 | MIDI CC 0|12 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 32 | MIDI CC 0|13 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 33 | MIDI CC 0|14 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 34 | MIDI CC 0|15 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 35 | MIDI CC 0|16 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 36 | MIDI CC 0|17 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 37 | MIDI CC 0|18 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 38 | MIDI CC 0|19 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 39 | MIDI CC 0|20 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 40 | MIDI CC 0|21 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 41 | MIDI CC 0|22 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 42 | MIDI CC 0|23 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 43 | MIDI CC 0|24 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 44 | MIDI CC 0|25 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 45 | MIDI CC 0|26 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 46 | MIDI CC 0|27 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 47 | MIDI CC 0|28 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 48 | MIDI CC 0|29 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 49 | MIDI CC 0|30 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 50 | MIDI CC 0|31 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 51 | MIDI CC 0|32 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 52 | MIDI CC 0|33 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 53 | MIDI CC 0|34 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 54 | MIDI CC 0|35 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 55 | MIDI CC 0|36 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 56 | MIDI CC 0|37 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 57 | MIDI CC 0|38 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 58 | MIDI CC 0|39 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 59 | MIDI CC 0|40 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 60 | MIDI CC 0|41 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 61 | MIDI CC 0|42 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 62 | MIDI CC 0|43 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |
| 63 | MIDI CC 0|44 | 0.0000 | 0.0000 | 1.0000 | 2147483647 | no | no |

## Sample rates (the parameters that change the audio at 0.75)
| rate | latency reported | latency measured | feature frequency | feature gain |
|---|---|---|---|---|
| 44100 Hz | 0 | 87 | 13611.6 Hz | -15.00 dB |
| 48000 Hz | 0 | 95 | 7288.69 Hz | -15.00 dB |
| 96000 Hz | 0 | 191 | 211.598 Hz | -15.00 dB |

Largest difference of the response between the rates (100 Hz - 19.8 kHz): 0.00 dB.
Feature frequency at the highest rate over the lowest: 0.016 (the rates: 2.177).

## Response at the setting B (dB; every ~10th point of the axis)
| frequency | 44100 Hz | 48000 Hz | 96000 Hz |
|---|---|---|---|
| 100 Hz | -15.00 | -15.00 | -15.00 |
| 151.649 Hz | -15.00 | -15.00 | -15.00 |
| 229.974 Hz | -15.00 | -15.00 | -15.00 |
| 348.754 Hz | -15.00 | -15.00 | -15.00 |
| 528.882 Hz | -15.00 | -15.00 | -15.00 |
| 802.045 Hz | -15.00 | -15.00 | -15.00 |
| 1216.29 Hz | -15.00 | -15.00 | -15.00 |
| 1844.5 Hz | -15.00 | -15.00 | -15.00 |
| 2797.17 Hz | -15.00 | -15.00 | -15.00 |
| 4241.88 Hz | -15.00 | -15.00 | -15.00 |
| 6432.77 Hz | -15.00 | -15.00 | -15.00 |
| 9755.23 Hz | -15.00 | -15.00 | -15.00 |
| 14793.7 Hz | -15.00 | -15.00 | -15.00 |

## Delivery of parameters (A, A, B, A)
| way | repeatable | reacts | as after a change | result | differences |
|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A -1.7 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A -1.7 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A -1.7 dB, A and B against the references -200.0 / -200.0 dB |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | A again -200.0 / -200.0 dB, B against A -1.7 dB, A and B against the references -200.0 / -200.0 dB |

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

