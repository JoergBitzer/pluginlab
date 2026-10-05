# Fingerprint: Multi-Q

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/multi-q-linux/VST3/Multi-Q.vst3`
- format: VST3, manufacturer: Dusk Audio, version: 0.10.9
- channels: mono yes, stereo yes

## Findings
- The response differs by up to 3.90 dB between the sample rates (the usual cramping near the Nyquist frequency of a bilinear design is below about 1 dB up to 10 kHz)
- Delivery 'new instance per render, parameters set before prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -102.9 dB, B: -35.9 dB): the first delivery was lost. 
- Delivery 'new instance per render, parameters set after prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -102.9 dB, B: -45.2 dB): the first delivery was lost. 
- Delivery 'new instance per render, after prepare every parameter first set to another value, then the target' fails: the settings that were delivered first differ from the same settings reached by a change (A: -107.3 dB, B: -45.1 dB): the first delivery was lost. 
- The output depends on the block size
- After parameter jumps to both ends the output does not come back to what it was
- Digital silence in does not give digital silence out (peak -149.2 dBFS)

## Parameters
| # | name | min | default | max | steps | automatable | changes the audio (dB) |
|---|---|---|---|---|---|---|---|
| 0 | Band 1 Enabled | Off | Off | On | 2 | yes | -26.9 |
| 1 | Band 1 Frequency | 20.0000000 | 20.0000000 | 20000.0000000 | 2147483647 | yes | no |
| 2 | Band 1 Q | 0.1000000 | 0.7100000 | 100.0000000 | 2147483647 | yes | no |
| 3 | Band 1 Routing | Global | Global | Side | 6 | yes | no |
| 4 | Band 1 Slope | 6 dB/oct | 12 dB/oct | 96 dB/oct | 8 | yes | no |
| 5 | Band 1 Invert | Off | Off | On | 2 | yes | no |
| 6 | Band 1 Phase Invert | Off | Off | On | 2 | yes | no |
| 7 | Band 1 Pan | -1.00 | 0.00 | 1.00 | 2147483647 | yes | no |
| 8 | Band 2 Enabled | Off | On | On | 2 | yes | -13.7 |
| 9 | Band 2 Frequency | 20.0000000 | 100.0000000 | 20000.0000000 | 2147483647 | yes | 0.5 |
| 10 | Band 2 Gain | -24.0 | 0.0 | 24.0 | 2147483647 | yes | -15.0 |
| 11 | Band 2 Q | 0.1000000 | 0.7100000 | 100.0000000 | 2147483647 | yes | -13.7 |
| 12 | Band 2 Shape | Low Shelf | Low Shelf | High Pass | 3 | yes | -18.1 |
| 13 | Band 2 Routing | Global | Global | Side | 6 | yes | no |
| 14 | Band 2 Saturation | Off | Off | FET | 5 | yes | -50.8 |
| 15 | Band 2 Sat Drive | 0.00 | 0.30 | 1.00 | 2147483647 | yes | no |
| 16 | Band 2 Invert | Off | Off | On | 2 | yes | -66.2 |
| 17 | Band 2 Phase Invert | Off | Off | On | 2 | yes | -7.7 |
| 18 | Band 2 Pan | -1.00 | 0.00 | 1.00 | 2147483647 | yes | -19.8 |
| 19 | Band 3 Enabled | Off | On | On | 2 | yes | 10.5 |
| 20 | Band 3 Frequency | 20.0000000 | 200.0000153 | 20000.0000000 | 2147483647 | yes | 8.2 |
| 21 | Band 3 Gain | -24.0 | 0.0 | 24.0 | 2147483647 | yes | -11.3 |
| 22 | Band 3 Q | 0.1000000 | 0.7100000 | 100.0000000 | 2147483647 | yes | -0.4 |
| 23 | Band 3 Shape | Peaking | Peaking | Tilt Shelf | 4 | yes | -0.1 |
| 24 | Band 3 Routing | Global | Global | Side | 6 | yes | no |
| 25 | Band 3 Saturation | Off | Off | FET | 5 | yes | -50.8 |
| 26 | Band 3 Sat Drive | 0.00 | 0.30 | 1.00 | 2147483647 | yes | no |
| 27 | Band 3 Invert | Off | Off | On | 2 | yes | no |
| 28 | Band 3 Phase Invert | Off | Off | On | 2 | yes | 16.5 |
| 29 | Band 3 Pan | -1.00 | 0.00 | 1.00 | 2147483647 | yes | 4.5 |
| 30 | Band 4 Enabled | Off | On | On | 2 | yes | 0.8 |
| 31 | Band 4 Frequency | 20.0000000 | 499.9999695 | 20000.0000000 | 2147483647 | yes | 2.9 |
| 32 | Band 4 Gain | -24.0 | 0.0 | 24.0 | 2147483647 | yes | -6.8 |
| 33 | Band 4 Q | 0.1000000 | 0.7100000 | 100.0000000 | 2147483647 | yes | -0.5 |
| 34 | Band 4 Shape | Peaking | Peaking | Tilt Shelf | 4 | yes | -0.2 |
| 35 | Band 4 Routing | Global | Global | Side | 6 | yes | no |
| 36 | Band 4 Saturation | Off | Off | FET | 5 | yes | -50.8 |
| 37 | Band 4 Sat Drive | 0.00 | 0.30 | 1.00 | 2147483647 | yes | no |
| 38 | Band 4 Invert | Off | Off | On | 2 | yes | no |
| 39 | Band 4 Phase Invert | Off | Off | On | 2 | yes | 6.9 |
| 40 | Band 4 Pan | -1.00 | 0.00 | 1.00 | 2147483647 | yes | -5.2 |
| 41 | Band 5 Enabled | Off | On | On | 2 | yes | -2.8 |
| 42 | Band 5 Frequency | 20.0000000 | 1000.0000000 | 20000.0000000 | 2147483647 | yes | 0.9 |
| 43 | Band 5 Gain | -24.0 | 0.0 | 24.0 | 2147483647 | yes | -4.1 |
| 44 | Band 5 Q | 0.1000000 | 0.7100000 | 100.0000000 | 2147483647 | yes | -0.7 |
| 45 | Band 5 Shape | Peaking | Peaking | Tilt Shelf | 4 | yes | -0.4 |
| 46 | Band 5 Routing | Global | Global | Side | 6 | yes | no |
| 47 | Band 5 Saturation | Off | Off | FET | 5 | yes | -50.8 |
| 48 | Band 5 Sat Drive | 0.00 | 0.30 | 1.00 | 2147483647 | yes | no |
| 49 | Band 5 Invert | Off | Off | On | 2 | yes | no |
| 50 | Band 5 Phase Invert | Off | Off | On | 2 | yes | 3.3 |
| 51 | Band 5 Pan | -1.00 | 0.00 | 1.00 | 2147483647 | yes | -8.8 |
| 52 | Band 6 Enabled | Off | On | On | 2 | yes | 5.1 |
| 53 | Band 6 Frequency | 20.0000000 | 2000.0002441 | 20000.0000000 | 2147483647 | yes | 3.2 |
| 54 | Band 6 Gain | -24.0 | 0.0 | 24.0 | 2147483647 | yes | -1.2 |
| 55 | Band 6 Q | 0.1000000 | 0.7100000 | 100.0000000 | 2147483647 | yes | -0.4 |
| 56 | Band 6 Shape | Peaking | Peaking | Tilt Shelf | 4 | yes | -0.7 |
| 57 | Band 6 Routing | Global | Global | Side | 6 | yes | no |
| 58 | Band 6 Saturation | Off | Off | FET | 5 | yes | -50.8 |
| 59 | Band 6 Sat Drive | 0.00 | 0.30 | 1.00 | 2147483647 | yes | no |
| 60 | Band 6 Invert | Off | Off | On | 2 | yes | no |
| 61 | Band 6 Phase Invert | Off | Off | On | 2 | yes | 11.1 |
| 62 | Band 6 Pan | -1.00 | 0.00 | 1.00 | 2147483647 | yes | -0.9 |
| 63 | Band 7 Enabled | Off | On | On | 2 | yes | no |

## Sample rates (the parameters that change the audio at 0.75)
| rate | latency reported | latency measured | feature frequency | feature gain |
|---|---|---|---|---|
| 44100 Hz | 60 | 60 | 19800 Hz | -24.10 dB |
| 48000 Hz | 60 | 60 | 19800 Hz | -24.10 dB |
| 96000 Hz | 60 | 60 | 19800 Hz | -24.10 dB |

Largest difference of the response between the rates (100 Hz - 19.8 kHz): 3.90 dB.
Feature frequency at the highest rate over the lowest: 1.000 (the rates: 2.177).

## Response at the setting B (dB; every ~10th point of the axis)
| frequency | 44100 Hz | 48000 Hz | 96000 Hz |
|---|---|---|---|
| 100 Hz | -5.66 | -5.73 | -9.56 |
| 151.649 Hz | -21.25 | -21.24 | -20.93 |
| 229.974 Hz | -23.14 | -23.13 | -23.07 |
| 348.754 Hz | -23.72 | -23.72 | -23.71 |
| 528.882 Hz | -23.94 | -23.94 | -23.95 |
| 802.045 Hz | -24.02 | -24.02 | -24.02 |
| 1216.29 Hz | -24.05 | -24.05 | -24.04 |
| 1844.5 Hz | -24.00 | -24.00 | -24.01 |
| 2797.17 Hz | -23.41 | -23.41 | -23.41 |
| 4241.88 Hz | -22.89 | -22.88 | -22.87 |
| 6432.77 Hz | -24.01 | -24.01 | -24.00 |
| 9755.23 Hz | -24.08 | -24.08 | -24.08 |
| 14793.7 Hz | -24.10 | -24.10 | -24.09 |

## Delivery of parameters (A, A, B, A)
| way | repeatable | reacts | as after a change | result | differences |
|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | A again -200.0 / -102.9 dB, B against A -0.6 dB, A and B against the references -102.9 / -200.0 dB |
| new instance per render, parameters set before prepare | yes | yes | no | FAILS | A again -200.0 / -200.0 dB, B against A -0.6 dB, A and B against the references -102.9 / -35.9 dB |
| new instance per render, parameters set after prepare | yes | yes | no | FAILS | A again -200.0 / -200.0 dB, B against A -0.6 dB, A and B against the references -102.9 / -45.2 dB |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | no | FAILS | A again -200.0 / -200.0 dB, B against A -0.6 dB, A and B against the references -107.3 / -45.1 dB |

Most careful way that works: one instance, parameters set after prepare (stream).

## Block sizes (difference to block size 512)
| block size | difference |
|---|---|
| 32 | -63.4 dB |
| 64 | -63.4 dB |
| 128 | -83.5 dB |
| 256 | -83.5 dB |
| 1024 | -82.7 dB |
| 2048 | -80.7 dB |
| 509 | -84.7 dB |

## Other
- deterministic: yes
- output stays finite: yes
- recovers from parameter jumps: no
- digital silence in gives digital silence out: no

