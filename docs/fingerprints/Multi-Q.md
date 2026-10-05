# Fingerprint: Multi-Q

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/multi-q-linux/VST3/Multi-Q.vst3`
- format: VST3, manufacturer: Dusk Audio, version: 0.10.9
- measured: 2026-10-05 22:38
- channels: mono yes, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| channel layouts (main bus in = out) | mono, stereo |  |
| parameters / changing the audio | 191 / 42 |  |
| latency at 48 kHz, reported / measured (samples) | 60 / 60 | 44.1 kHz: 60 / 60, 48.0 kHz: 60 / 60, 96.0 kHz: 60 / 60 |
| reported latency = measured at all rates | yes | 44.1 kHz: 60 / 60, 48.0 kHz: 60 / 60, 96.0 kHz: 60 / 60 |
| output before the peak of the impulse response | no |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | **2 of 4 ways** | new instance per render, parameters set after prepare |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | **no** |  |
| digital silence in gives digital silence out | **no** | peak -149.2 dBFS |

Bold: worth a look (see the findings and the details below).

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- Delivery 'new instance per render, parameters set before prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -102.9 dB / -127.7 dBFS, B: -37.2 dB / -82.0 dBFS): the first delivery was lost. 
- Delivery 'new instance per render, after prepare every parameter first set to another value, then the target' fails: the settings that were delivered first differ from the same settings reached by a change (A: -107.3 dB / -132.2 dBFS, B: -51.9 dB / -96.7 dBFS): the first delivery was lost. 
- After parameter jumps to both ends the output does not come back to what it was
- Digital silence in does not give digital silence out (peak -149.2 dBFS)

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 191 parameters, the first 64 examined.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | Band 1 Enabled | Off | Off | On | switch | yes | -26.9 dB / -51.7 dBFS | defaults |
| 1 | Band 1 Frequency | 20.0000000 | 20.0000000 | 20000.0000000 | continuous | yes | no |  |
| 2 | Band 1 Q | 0.1000000 | 0.7100000 | 100.0000000 | continuous | yes | no |  |
| 3 | Band 1 Routing | Global | Global | Side | 6 | yes | no |  |
| 4 | Band 1 Slope | 6 dB/oct | 12 dB/oct | 96 dB/oct | 8 | yes | no |  |
| 5 | Band 1 Invert | Off | Off | On | switch | yes | no |  |
| 6 | Band 1 Phase Invert | Off | Off | On | switch | yes | no |  |
| 7 | Band 1 Pan | -1.00 | 0.00 | 1.00 | continuous | yes | no |  |
| 8 | Band 2 Enabled | Off | On | On | switch | yes | -13.7 dB / -69.1 dBFS | the others at 0.75 |
| 9 | Band 2 Frequency | 20.0000000 | 100.0000000 | 20000.0000000 | continuous | yes | 0.5 dB / -54.9 dBFS | the others at 0.75 |
| 10 | Band 2 Gain | -24.0 | 0.0 | 24.0 | continuous | yes | -15.0 dB / -39.8 dBFS | defaults |
| 11 | Band 2 Q | 0.1000000 | 0.7100000 | 100.0000000 | continuous | yes | -13.7 dB / -69.0 dBFS | the others at 0.75 |
| 12 | Band 2 Shape | Low Shelf | Low Shelf | High Pass | 3 | yes | -18.1 dB / -42.9 dBFS | defaults |
| 13 | Band 2 Routing | Global | Global | Side | 6 | yes | no |  |
| 14 | Band 2 Saturation | Off | Off | FET | 5 | yes | -50.8 dB / -75.6 dBFS | defaults |
| 15 | Band 2 Sat Drive | 0.00 | 0.30 | 1.00 | continuous | yes | no |  |
| 16 | Band 2 Invert | Off | Off | On | switch | yes | -66.2 dB / -121.5 dBFS | the others at 0.75 |
| 17 | Band 2 Phase Invert | Off | Off | On | switch | yes | -7.7 dB / -63.1 dBFS | the others at 0.75 |
| 18 | Band 2 Pan | -1.00 | 0.00 | 1.00 | continuous | yes | -19.8 dB / -75.1 dBFS | the others at 0.75 |
| 19 | Band 3 Enabled | Off | On | On | switch | yes | 10.5 dB / -44.8 dBFS | the others at 0.75 |
| 20 | Band 3 Frequency | 20.0000000 | 200.0000153 | 20000.0000000 | continuous | yes | 8.2 dB / -47.2 dBFS | the others at 0.75 |
| 21 | Band 3 Gain | -24.0 | 0.0 | 24.0 | continuous | yes | -11.3 dB / -36.1 dBFS | defaults |
| 22 | Band 3 Q | 0.1000000 | 0.7100000 | 100.0000000 | continuous | yes | -0.4 dB / -55.7 dBFS | the others at 0.75 |
| 23 | Band 3 Shape | Peaking | Peaking | Tilt Shelf | 4 | yes | -0.1 dB / -24.9 dBFS | defaults |
| 24 | Band 3 Routing | Global | Global | Side | 6 | yes | no |  |
| 25 | Band 3 Saturation | Off | Off | FET | 5 | yes | -50.8 dB / -75.6 dBFS | defaults |
| 26 | Band 3 Sat Drive | 0.00 | 0.30 | 1.00 | continuous | yes | no |  |
| 27 | Band 3 Invert | Off | Off | On | switch | yes | no |  |
| 28 | Band 3 Phase Invert | Off | Off | On | switch | yes | 16.5 dB / -38.8 dBFS | the others at 0.75 |
| 29 | Band 3 Pan | -1.00 | 0.00 | 1.00 | continuous | yes | 4.5 dB / -50.9 dBFS | the others at 0.75 |
| 30 | Band 4 Enabled | Off | On | On | switch | yes | 0.8 dB / -54.5 dBFS | the others at 0.75 |
| 31 | Band 4 Frequency | 20.0000000 | 499.9999695 | 20000.0000000 | continuous | yes | 2.9 dB / -52.5 dBFS | the others at 0.75 |
| 32 | Band 4 Gain | -24.0 | 0.0 | 24.0 | continuous | yes | -6.8 dB / -31.6 dBFS | defaults |
| 33 | Band 4 Q | 0.1000000 | 0.7100000 | 100.0000000 | continuous | yes | -0.5 dB / -55.9 dBFS | the others at 0.75 |
| 34 | Band 4 Shape | Peaking | Peaking | Tilt Shelf | 4 | yes | -0.2 dB / -25.0 dBFS | defaults |
| 35 | Band 4 Routing | Global | Global | Side | 6 | yes | no |  |
| 36 | Band 4 Saturation | Off | Off | FET | 5 | yes | -50.8 dB / -75.6 dBFS | defaults |
| 37 | Band 4 Sat Drive | 0.00 | 0.30 | 1.00 | continuous | yes | no |  |
| 38 | Band 4 Invert | Off | Off | On | switch | yes | no |  |
| 39 | Band 4 Phase Invert | Off | Off | On | switch | yes | 6.9 dB / -48.5 dBFS | the others at 0.75 |
| 40 | Band 4 Pan | -1.00 | 0.00 | 1.00 | continuous | yes | -5.2 dB / -60.5 dBFS | the others at 0.75 |
| 41 | Band 5 Enabled | Off | On | On | switch | yes | -2.8 dB / -58.1 dBFS | the others at 0.75 |
| 42 | Band 5 Frequency | 20.0000000 | 1000.0000000 | 20000.0000000 | continuous | yes | 0.9 dB / -54.4 dBFS | the others at 0.75 |
| 43 | Band 5 Gain | -24.0 | 0.0 | 24.0 | continuous | yes | -4.1 dB / -28.9 dBFS | defaults |
| 44 | Band 5 Q | 0.1000000 | 0.7100000 | 100.0000000 | continuous | yes | -0.7 dB / -56.0 dBFS | the others at 0.75 |
| 45 | Band 5 Shape | Peaking | Peaking | Tilt Shelf | 4 | yes | -0.4 dB / -25.2 dBFS | defaults |
| 46 | Band 5 Routing | Global | Global | Side | 6 | yes | no |  |
| 47 | Band 5 Saturation | Off | Off | FET | 5 | yes | -50.8 dB / -75.6 dBFS | defaults |
| 48 | Band 5 Sat Drive | 0.00 | 0.30 | 1.00 | continuous | yes | no |  |
| 49 | Band 5 Invert | Off | Off | On | switch | yes | no |  |
| 50 | Band 5 Phase Invert | Off | Off | On | switch | yes | 3.3 dB / -52.1 dBFS | the others at 0.75 |
| 51 | Band 5 Pan | -1.00 | 0.00 | 1.00 | continuous | yes | -8.8 dB / -64.1 dBFS | the others at 0.75 |
| 52 | Band 6 Enabled | Off | On | On | switch | yes | 5.1 dB / -50.3 dBFS | the others at 0.75 |
| 53 | Band 6 Frequency | 20.0000000 | 2000.0002441 | 20000.0000000 | continuous | yes | 3.2 dB / -52.2 dBFS | the others at 0.75 |
| 54 | Band 6 Gain | -24.0 | 0.0 | 24.0 | continuous | yes | -1.2 dB / -26.1 dBFS | defaults |
| 55 | Band 6 Q | 0.1000000 | 0.7100000 | 100.0000000 | continuous | yes | -0.4 dB / -55.8 dBFS | the others at 0.75 |
| 56 | Band 6 Shape | Peaking | Peaking | Tilt Shelf | 4 | yes | -0.7 dB / -25.6 dBFS | defaults |
| 57 | Band 6 Routing | Global | Global | Side | 6 | yes | no |  |
| 58 | Band 6 Saturation | Off | Off | FET | 5 | yes | -50.8 dB / -75.6 dBFS | defaults |
| 59 | Band 6 Sat Drive | 0.00 | 0.30 | 1.00 | continuous | yes | no |  |
| 60 | Band 6 Invert | Off | Off | On | switch | yes | no |  |
| 61 | Band 6 Phase Invert | Off | Off | On | switch | yes | 11.1 dB / -44.3 dBFS | the others at 0.75 |
| 62 | Band 6 Pan | -1.00 | 0.00 | 1.00 | continuous | yes | -0.9 dB / -56.3 dBFS | the others at 0.75 |
| 63 | Band 7 Enabled | Off | On | On | switch | yes | no |  |

## Latency at three sample rates
An impulse (1.0 on all channels) after 4096 samples of silence, the plugin at its default parameters, 1.00 s watched. Measured = position of the largest output sample after the impulse. Reported = getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.

| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |
|---|---|---|---|---|---|
| 44100 Hz | 60 | 60 | 60 | none | none |
| 48000 Hz | 60 | 60 | 60 | none | none |
| 96000 Hz | 60 | 60 | 60 | none | none |

## Delivery of parameters (A, A, B, A)
Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at 0.75), A. Repeatable: A, A, A the same (below -80 dB). Reacts: B differs from A (above -60 dB). As after a change: A and B equal the outputs of the same settings reached by a change of the parameters in the first way.

| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |
|---|---|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; -102.9 dB / -127.7 dBFS | -0.6 dB / -25.4 dBFS | -102.9 dB / -127.7 dBFS ; identical |
| new instance per render, parameters set before prepare | yes | yes | no | FAILS | identical ; identical | -0.6 dB / -25.4 dBFS | -102.9 dB / -127.7 dBFS ; -37.2 dB / -82.0 dBFS |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | -0.6 dB / -25.4 dBFS | -102.9 dB / -127.7 dBFS ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | no | FAILS | identical ; identical | -0.6 dB / -25.4 dBFS | -107.3 dB / -132.2 dBFS ; -51.9 dB / -96.7 dBFS |

Most careful way that works: new instance per render, parameters set after prepare.

## Block sizes
1.00 s of noise through the plugin (setting B) per block size, against block size 512. Steady state: the last 0.10 s (decides; below -100 dB = independent). Whole: the full render (a plugin that smooths its parameters per block differs here, but not in the steady state).

| block size | steady state | whole |
|---|---|---|
| 32 | identical | identical |
| 64 | identical | identical |
| 128 | identical | identical |
| 256 | identical | identical |
| 1024 | identical | identical |
| 2048 | identical | identical |
| 509 | identical | identical |

## Other
- deterministic (two instances, the same noise, bit exact): yes
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (every parameter that changes the audio to 0 and 1 and back, then the output of setting B again): no
- digital silence in gives digital silence out: no (peak -149.2 dBFS)

## Settings used
```
{
  "reactsAboveDb": -80.0,
  "differentAboveDb": -60.0,
  "sameBelowDb": -80.0,
  "blockIndependentBelowDb": -100.0,
  "silentReferenceDbfs": -150.0,
  "noiseLevel": 0.1,
  "settleSeconds": 0.25,
  "impulsePreDelaySamples": 4096,
  "latencyObserveSeconds": 1.0,
  "latencyMinimumPeak": 0.0001,
  "blockRenderSeconds": 1.0,
  "blockCompareSeconds": 0.1,
  "blockSizes": [
    32,
    64,
    128,
    256,
    1024,
    2048,
    509
  ],
  "lowSetting": 0.25,
  "highSetting": 0.75,
  "pokeDistance": 0.4,
  "maximumParameters": 64,
  "maximumJumpedParameters": 16
}
```

