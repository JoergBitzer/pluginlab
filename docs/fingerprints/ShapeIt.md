# Fingerprint: Soundly Shape it

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/shapeit/Shapeit/ShapeIt.vst3`
- format: VST3, manufacturer: Soundly, version: 1.0.13
- measured: 2026-10-05 21:37
- channels: mono yes, stereo yes

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- nothing unusual found

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 67 parameters, the first 64 examined.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | Bypass | Off | Off | On | switch | yes | -2.5 dB / -15.3 dBFS | the others at 0.75 |
| 1 | Gain In | -12.0 | 0.0 | 12.0 | continuous | yes | -0.0 dB / -24.9 dBFS | defaults |
| 2 | Gain Out | -12.0 | 0.0 | 12.0 | continuous | yes | -0.0 dB / -24.9 dBFS | defaults |
| 3 | Mix | 0 | 100 | 100 | continuous | yes | no |  |
| 4 | Phase invert L | Off | Off | On | switch | yes | 6.0 dB / -18.8 dBFS | defaults |
| 5 | Phase invert R | Off | Off | On | switch | yes | no |  |
| 6 | Band 1 Loaded | Off | Off | On | switch | yes | no |  |
| 7 | Band 1 Active | Off | On | On | switch | yes | no |  |
| 8 | Band 1 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 9 | Band 1 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 10 | Band 1 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 11 | Band 1 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 12 | Band 2 Loaded | Off | Off | On | switch | yes | no |  |
| 13 | Band 2 Active | Off | On | On | switch | yes | no |  |
| 14 | Band 2 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 15 | Band 2 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 16 | Band 2 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 17 | Band 2 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 18 | Band 3 Loaded | Off | Off | On | switch | yes | no |  |
| 19 | Band 3 Active | Off | On | On | switch | yes | no |  |
| 20 | Band 3 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 21 | Band 3 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 22 | Band 3 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 23 | Band 3 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 24 | Band 4 Loaded | Off | Off | On | switch | yes | no |  |
| 25 | Band 4 Active | Off | On | On | switch | yes | no |  |
| 26 | Band 4 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 27 | Band 4 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 28 | Band 4 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 29 | Band 4 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 30 | Band 5 Loaded | Off | Off | On | switch | yes | no |  |
| 31 | Band 5 Active | Off | On | On | switch | yes | no |  |
| 32 | Band 5 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 33 | Band 5 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 34 | Band 5 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 35 | Band 5 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 36 | Band 6 Loaded | Off | Off | On | switch | yes | no |  |
| 37 | Band 6 Active | Off | On | On | switch | yes | no |  |
| 38 | Band 6 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 39 | Band 6 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 40 | Band 6 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 41 | Band 6 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 42 | Band 7 Loaded | Off | Off | On | switch | yes | no |  |
| 43 | Band 7 Active | Off | On | On | switch | yes | no |  |
| 44 | Band 7 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 45 | Band 7 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 46 | Band 7 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 47 | Band 7 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 48 | Band 8 Loaded | Off | Off | On | switch | yes | no |  |
| 49 | Band 8 Active | Off | On | On | switch | yes | no |  |
| 50 | Band 8 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 51 | Band 8 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 52 | Band 8 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 53 | Band 8 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 54 | Band 9 Loaded | Off | Off | On | switch | yes | no |  |
| 55 | Band 9 Active | Off | On | On | switch | yes | no |  |
| 56 | Band 9 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 57 | Band 9 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |
| 58 | Band 9 Gain | -24.0 | 0.0 | +24.0 | continuous | yes | no |  |
| 59 | Band 9 Q | 0.10 | 0.70 | 40.0 | continuous | yes | no |  |
| 60 | Band 10 Loaded | Off | Off | On | switch | yes | no |  |
| 61 | Band 10 Active | Off | On | On | switch | yes | no |  |
| 62 | Band 10 Type | Low Cut 48 dB/oct. | Bell | High Cut 48 dB/oct. | 14 | yes | no |  |
| 63 | Band 10 frequency | 15.000 | 1.000k | 24.00k | continuous | yes | no |  |

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
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 9.5 dB / -15.3 dBFS | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 9.5 dB / -15.3 dBFS | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 9.5 dB / -15.3 dBFS | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 9.5 dB / -15.3 dBFS | identical ; identical |

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

