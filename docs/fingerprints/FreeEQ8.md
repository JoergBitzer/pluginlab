# Fingerprint: FreeEQ8

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/FreeEQ8/FreeEQ8-v2.3.1-Linux/FreeEQ8.vst3`
- format: VST3, manufacturer: TizWildinEntertainment, version: 2.3.1
- measured: 2026-10-05 22:38
- channels: mono no, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| channel layouts (main bus in = out) | stereo |  |
| parameters / changing the audio | 129 / 41 |  |
| latency at 48 kHz, reported / measured (samples) | 0 / 0 | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| reported latency = measured at all rates | yes | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| output before the peak of the impulse response | no |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | 4 of 4 ways | new instance per render, after prepare every parameter first set to another value, then the target |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | yes |  |
| digital silence in gives digital silence out | yes |  |

Bold: worth a look (see the findings and the details below).

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- nothing unusual found

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 129 parameters, the first 64 examined.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | Output Gain | -24.00 | 0.00 | 24.00 | continuous | yes | 9.5 dB / -15.3 dBFS | defaults |
| 1 | Scale | 0.10 | 1.00 | 2.00 | continuous | yes | no |  |
| 2 | Adaptive Q | Off | Off | On | switch | yes | -3.5 dB / 8.3 dBFS | the others at 0.75 |
| 3 | Oversampling | 1x | 1x | 8x | 4 | yes | 3.0 dB / -21.8 dBFS | defaults |
| 4 | Processing Mode | Stereo | Stereo | Mid-Side | switch | yes | no |  |
| 5 | Linear Phase | Off | Off | On | switch | yes | -0.0 dB / -24.8 dBFS | defaults |
| 6 | Auto Gain | Off | Off | On | switch | yes | -1.0 dB / 10.8 dBFS | the others at 0.75 |
| 7 | Intent Mode | None | None | Master Polish | 5 | yes | no |  |
| 8 | Band 1 On | Off | On | On | switch | yes | 2.9 dB / 14.8 dBFS | the others at 0.75 |
| 9 | Band 1 Solo | Off | Off | On | switch | yes | -0.2 dB / 11.6 dBFS | the others at 0.75 |
| 10 | Band 1 Type | Bell | Bell | Bandpass | 6 | yes | 0.0 dB / -24.8 dBFS | defaults |
| 11 | Band 1 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | 2.9 dB / 14.7 dBFS | the others at 0.75 |
| 12 | Band 1 Channel | Both | Both | R / Side | 3 | yes | 3.3 dB / 15.1 dBFS | the others at 0.75 |
| 13 | Band 1 Link | -- | -- | B | 3 | yes | no |  |
| 14 | Band 1 Freq | 20.000 | 80.000 | 20000.000 | continuous | yes | 3.3 dB / 15.1 dBFS | the others at 0.75 |
| 15 | Band 1 Q | 0.100 | 1.000 | 24.000 | continuous | yes | 0.2 dB / 12.0 dBFS | the others at 0.75 |
| 16 | Band 1 Gain | -24.00 | 0.00 | 24.00 | continuous | yes | -14.5 dB / -39.3 dBFS | defaults |
| 17 | Band 1 Drive | 0.0 | 0.0 | 100.0 | continuous | yes | 15.5 dB / -9.3 dBFS | defaults |
| 18 | Band 1 Dyn On | Off | Off | On | switch | yes | no |  |
| 19 | Band 1 Threshold | -60.0 | -20.0 | 0.0 | continuous | yes | no |  |
| 20 | Band 1 Ratio | 1.0 | 4.0 | 20.0 | continuous | yes | no |  |
| 21 | Band 1 Attack | 0.1 | 10.0 | 100.0 | continuous | yes | no |  |
| 22 | Band 1 Release | 1 | 100 | 1000 | continuous | yes | no |  |
| 23 | Band 2 On | Off | On | On | switch | yes | -4.7 dB / 7.1 dBFS | the others at 0.75 |
| 24 | Band 2 Solo | Off | Off | On | switch | yes | 0.1 dB / 11.9 dBFS | the others at 0.75 |
| 25 | Band 2 Type | Bell | Bell | Bandpass | 6 | yes | 0.1 dB / -24.8 dBFS | defaults |
| 26 | Band 2 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | 1.7 dB / 13.5 dBFS | the others at 0.75 |
| 27 | Band 2 Channel | Both | Both | R / Side | 3 | yes | -3.2 dB / 8.6 dBFS | the others at 0.75 |
| 28 | Band 2 Link | -- | -- | B | 3 | yes | no |  |
| 29 | Band 2 Freq | 20.000 | 250.000 | 20000.000 | continuous | yes | -3.3 dB / 8.5 dBFS | the others at 0.75 |
| 30 | Band 2 Q | 0.100 | 1.000 | 24.000 | continuous | yes | -2.7 dB / 9.1 dBFS | the others at 0.75 |
| 31 | Band 2 Gain | -24.00 | 0.00 | 24.00 | continuous | yes | -12.0 dB / -36.9 dBFS | defaults |
| 32 | Band 2 Drive | 0.0 | 0.0 | 100.0 | continuous | yes | 15.5 dB / -9.3 dBFS | defaults |
| 33 | Band 2 Dyn On | Off | Off | On | switch | yes | no |  |
| 34 | Band 2 Threshold | -60.0 | -20.0 | 0.0 | continuous | yes | no |  |
| 35 | Band 2 Ratio | 1.0 | 4.0 | 20.0 | continuous | yes | no |  |
| 36 | Band 2 Attack | 0.1 | 10.0 | 100.0 | continuous | yes | no |  |
| 37 | Band 2 Release | 1 | 100 | 1000 | continuous | yes | no |  |
| 38 | Band 3 On | Off | On | On | switch | yes | -8.0 dB / 3.8 dBFS | the others at 0.75 |
| 39 | Band 3 Solo | Off | Off | On | switch | yes | 0.1 dB / 11.9 dBFS | the others at 0.75 |
| 40 | Band 3 Type | Bell | Bell | Bandpass | 6 | yes | 0.1 dB / -24.7 dBFS | defaults |
| 41 | Band 3 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | -1.5 dB / 10.3 dBFS | the others at 0.75 |
| 42 | Band 3 Channel | Both | Both | R / Side | 3 | yes | -7.5 dB / 4.3 dBFS | the others at 0.75 |
| 43 | Band 3 Link | -- | -- | B | 3 | yes | no |  |
| 44 | Band 3 Freq | 20.000 | 500.000 | 20000.000 | continuous | yes | -7.8 dB / 4.0 dBFS | the others at 0.75 |
| 45 | Band 3 Q | 0.100 | 1.000 | 24.000 | continuous | yes | -8.5 dB / 3.3 dBFS | the others at 0.75 |
| 46 | Band 3 Gain | -24.00 | 0.00 | 24.00 | continuous | yes | -8.2 dB / -33.0 dBFS | defaults |
| 47 | Band 3 Drive | 0.0 | 0.0 | 100.0 | continuous | yes | 15.5 dB / -9.3 dBFS | defaults |
| 48 | Band 3 Dyn On | Off | Off | On | switch | yes | no |  |
| 49 | Band 3 Threshold | -60.0 | -20.0 | 0.0 | continuous | yes | no |  |
| 50 | Band 3 Ratio | 1.0 | 4.0 | 20.0 | continuous | yes | no |  |
| 51 | Band 3 Attack | 0.1 | 10.0 | 100.0 | continuous | yes | no |  |
| 52 | Band 3 Release | 1 | 100 | 1000 | continuous | yes | no |  |
| 53 | Band 4 On | Off | On | On | switch | yes | -9.5 dB / 2.3 dBFS | the others at 0.75 |
| 54 | Band 4 Solo | Off | Off | On | switch | yes | 0.1 dB / 11.9 dBFS | the others at 0.75 |
| 55 | Band 4 Type | Bell | Bell | Bandpass | 6 | yes | 0.2 dB / -24.6 dBFS | defaults |
| 56 | Band 4 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | -4.9 dB / 6.9 dBFS | the others at 0.75 |
| 57 | Band 4 Channel | Both | Both | R / Side | 3 | yes | -11.8 dB / -0.0 dBFS | the others at 0.75 |
| 58 | Band 4 Link | -- | -- | B | 3 | yes | no |  |
| 59 | Band 4 Freq | 20.000 | 1000.000 | 20000.000 | continuous | yes | -12.4 dB / -0.6 dBFS | the others at 0.75 |
| 60 | Band 4 Q | 0.100 | 1.000 | 24.000 | continuous | yes | -12.7 dB / -0.9 dBFS | the others at 0.75 |
| 61 | Band 4 Gain | -24.00 | 0.00 | 24.00 | continuous | yes | -5.5 dB / -30.3 dBFS | defaults |
| 62 | Band 4 Drive | 0.0 | 0.0 | 100.0 | continuous | yes | 15.5 dB / -9.3 dBFS | defaults |
| 63 | Band 4 Dyn On | Off | Off | On | switch | yes | no |  |

## Latency at three sample rates
An impulse (1.0 on all channels) after 4096 samples of silence, the plugin at its default parameters, 1.00 s watched. Measured = position of the largest output sample after the impulse. Reported = getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.

| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |
|---|---|---|---|---|---|
| 44100 Hz | 0 | 0 | 0 | none | none |
| 48000 Hz | 0 | 0 | 0 | none | none |
| 96000 Hz | 0 | 0 | 0 | none | none |

## Delivery of parameters (A, A, B, A)
Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at 0.75), A. Repeatable: A, A, A the same (below -80 dB). Reacts: B differs from A (above -60 dB). As after a change: A and B equal the outputs of the same settings reached by a change of the parameters in the first way.

| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |
|---|---|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 35.9 dB / 11.0 dBFS | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 35.9 dB / 11.0 dBFS | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 35.9 dB / 11.0 dBFS | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 35.9 dB / 11.0 dBFS | identical ; identical |

Most careful way that works: new instance per render, after prepare every parameter first set to another value, then the target.

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
- recovers from parameter jumps (every parameter that changes the audio to 0 and 1 and back, then the output of setting B again): yes
- digital silence in gives digital silence out: yes

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

