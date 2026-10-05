# Fingerprint: ZeroEQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ZeroEQ_1.0.7_Linux_VST3_LV2_CLAP_Standalone/VST3/ZeroEQ.vst3`
- format: VST3, manufacturer: Jun Murakami, version: 1.0.7
- measured: 2026-10-05 21:37
- channels: mono yes, stereo yes

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- nothing unusual found

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 72 parameters, the first 64 examined.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | Bypass | Off | Off | On | switch | yes | 14.0 dB / -12.6 dBFS | the others at 0.75 |
| 1 | Output Gain | -24.0 | 0.0 | 24.0 | continuous | yes | 9.5 dB / -15.3 dBFS | defaults |
| 2 | Analyzer | Off | Pre+Post | Pre+Post | 4 | yes | no |  |
| 3 | Bottom Panel Open | Off | On | On | switch | no | no |  |
| 4 | EQ dB Range | +/-3 dB | +/-12 dB | +/-32 dB | 5 | no | no |  |
| 5 | Band 1 On | Off | Off | On | switch | yes | -21.5 dB / -46.3 dBFS | defaults |
| 6 | Band 1 Type | Bell | HighPass | Notch | 6 | yes | no |  |
| 7 | Band 1 Freq | 20.0000000 | 30.0000000 | 20000.0000000 | continuous | yes | no |  |
| 8 | Band 1 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | no |  |
| 9 | Band 1 Q | 0.1000000 | 0.7070000 | 18.0000000 | continuous | yes | no |  |
| 10 | Band 1 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | no |  |
| 11 | Band 2 On | Off | Off | On | switch | yes | -18.1 dB / -42.9 dBFS | defaults |
| 12 | Band 2 Type | Bell | HighPass | Notch | 6 | yes | no |  |
| 13 | Band 2 Freq | 20.0000000 | 60.0000000 | 20000.0000000 | continuous | yes | no |  |
| 14 | Band 2 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | no |  |
| 15 | Band 2 Q | 0.1000000 | 0.7070000 | 18.0000000 | continuous | yes | no |  |
| 16 | Band 2 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | no |  |
| 17 | Band 3 On | Off | On | On | switch | yes | 4.6 dB / -22.1 dBFS | the others at 0.75 |
| 18 | Band 3 Type | Bell | LowShelf | Notch | 6 | yes | 0.0 dB / -24.8 dBFS | defaults |
| 19 | Band 3 Freq | 20.0000000 | 119.9999924 | 20000.0000000 | continuous | yes | 4.6 dB / -22.0 dBFS | the others at 0.75 |
| 20 | Band 3 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | -9.1 dB / -33.9 dBFS | defaults |
| 21 | Band 3 Q | 0.1000000 | 0.7070000 | 18.0000000 | continuous | yes | no |  |
| 22 | Band 3 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | 25.1 dB / -1.5 dBFS | the others at 0.75 |
| 23 | Band 4 On | Off | On | On | switch | yes | -6.1 dB / -32.7 dBFS | the others at 0.75 |
| 24 | Band 4 Type | Bell | Bell | Notch | 6 | yes | 0.0 dB / -24.8 dBFS | defaults |
| 25 | Band 4 Freq | 20.0000000 | 250.0000153 | 20000.0000000 | continuous | yes | 9.3 dB / -17.4 dBFS | the others at 0.75 |
| 26 | Band 4 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | -8.1 dB / -32.9 dBFS | defaults |
| 27 | Band 4 Q | 0.1000000 | 1.0000000 | 18.0000000 | continuous | yes | no |  |
| 28 | Band 4 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | 12.9 dB / -13.7 dBFS | the others at 0.75 |
| 29 | Band 5 On | Off | On | On | switch | yes | -11.9 dB / -38.5 dBFS | the others at 0.75 |
| 30 | Band 5 Type | Bell | Bell | Notch | 6 | yes | 0.1 dB / -24.7 dBFS | defaults |
| 31 | Band 5 Freq | 20.0000000 | 499.9999695 | 20000.0000000 | continuous | yes | 10.5 dB / -16.2 dBFS | the others at 0.75 |
| 32 | Band 5 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | -4.1 dB / -29.0 dBFS | defaults |
| 33 | Band 5 Q | 0.1000000 | 1.0000000 | 18.0000000 | continuous | yes | no |  |
| 34 | Band 5 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -11.8 dB / -38.4 dBFS | the others at 0.75 |
| 35 | Band 6 On | Off | On | On | switch | yes | -17.8 dB / -44.4 dBFS | the others at 0.75 |
| 36 | Band 6 Type | Bell | Bell | Notch | 6 | yes | 0.2 dB / -24.6 dBFS | defaults |
| 37 | Band 6 Freq | 20.0000000 | 1000.0000000 | 20000.0000000 | continuous | yes | 10.8 dB / -15.8 dBFS | the others at 0.75 |
| 38 | Band 6 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | -1.5 dB / -26.3 dBFS | defaults |
| 39 | Band 6 Q | 0.1000000 | 1.0000000 | 18.0000000 | continuous | yes | no |  |
| 40 | Band 6 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -19.6 dB / -46.2 dBFS | the others at 0.75 |
| 41 | Band 7 On | Off | On | On | switch | yes | -23.8 dB / -50.4 dBFS | the others at 0.75 |
| 42 | Band 7 Type | Bell | Bell | Notch | 6 | yes | 0.3 dB / -24.5 dBFS | defaults |
| 43 | Band 7 Freq | 20.0000000 | 2000.0002441 | 20000.0000000 | continuous | yes | 10.9 dB / -15.7 dBFS | the others at 0.75 |
| 44 | Band 7 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | 1.4 dB / -23.5 dBFS | defaults |
| 45 | Band 7 Q | 0.1000000 | 1.0000000 | 18.0000000 | continuous | yes | no |  |
| 46 | Band 7 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -25.6 dB / -52.3 dBFS | the others at 0.75 |
| 47 | Band 8 On | Off | On | On | switch | yes | -29.9 dB / -56.6 dBFS | the others at 0.75 |
| 48 | Band 8 Type | Bell | Bell | Notch | 6 | yes | 0.6 dB / -24.2 dBFS | defaults |
| 49 | Band 8 Freq | 20.0000000 | 3999.9995117 | 20000.0000000 | continuous | yes | 11.0 dB / -15.7 dBFS | the others at 0.75 |
| 50 | Band 8 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | 4.1 dB / -20.8 dBFS | defaults |
| 51 | Band 8 Q | 0.1000000 | 1.0000000 | 18.0000000 | continuous | yes | no |  |
| 52 | Band 8 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -31.8 dB / -58.5 dBFS | the others at 0.75 |
| 53 | Band 9 On | Off | On | On | switch | yes | -36.6 dB / -63.3 dBFS | the others at 0.75 |
| 54 | Band 9 Type | Bell | Bell | Notch | 6 | yes | 0.8 dB / -24.0 dBFS | defaults |
| 55 | Band 9 Freq | 20.0000000 | 7999.9995117 | 20000.0000000 | continuous | yes | 11.0 dB / -15.7 dBFS | the others at 0.75 |
| 56 | Band 9 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | 6.2 dB / -18.7 dBFS | defaults |
| 57 | Band 9 Q | 0.1000000 | 1.0000000 | 18.0000000 | continuous | yes | no |  |
| 58 | Band 9 Slope | 6 dB/oct | 18 dB/oct | 48 dB/oct | 6 | yes | -38.5 dB / -65.1 dBFS | the others at 0.75 |
| 59 | Band 10 On | Off | On | On | switch | yes | -41.4 dB / -68.0 dBFS | the others at 0.75 |
| 60 | Band 10 Type | Bell | HighShelf | Notch | 6 | yes | 0.7 dB / -24.2 dBFS | defaults |
| 61 | Band 10 Freq | 20.0000000 | 12000.0009766 | 20000.0000000 | continuous | yes | 11.0 dB / -15.6 dBFS | the others at 0.75 |
| 62 | Band 10 Gain | -32.0 | 0.0 | 32.0 | continuous | yes | 10.5 dB / -14.3 dBFS | defaults |
| 63 | Band 10 Q | 0.1000000 | 0.7070000 | 18.0000000 | continuous | yes | no |  |

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
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 317.8 dB / 293.0 dBFS | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 317.8 dB / 293.0 dBFS | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 317.8 dB / 293.0 dBFS | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 317.8 dB / 293.0 dBFS | identical ; identical |

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

