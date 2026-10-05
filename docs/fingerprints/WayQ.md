# Fingerprint: WayQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/WayQ-Linux-v1.0.0/linux/WayQ.vst3`
- format: VST3, manufacturer: Way.Net, version: 1.0.0
- measured: 2026-10-05 21:37
- channels: mono no, stereo yes

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- Latency at 96000 Hz: the plugin reports 0 samples, measured 1 samples

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 29 parameters.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | HP Freq A | 20.0000000 | 20.0000000 | 20000.0000000 | continuous | yes | -2.2 dB / -27.9 dBFS | defaults |
| 1 | HP Q A | 0.3000000 | 0.7070000 | 10.0000000 | continuous | yes | -23.6 dB / -49.3 dBFS | defaults |
| 2 | Mid Freq A | 20.0000000 | 632.4559937 | 20000.0000000 | continuous | yes | -14.9 dB / -21.5 dBFS | the others at 0.75 |
| 3 | Mid Gain A | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | -13.0 dB / -38.7 dBFS | defaults |
| 4 | Mid Q A | 0.3000000 | 0.7000000 | 17.3099995 | continuous | yes | -32.7 dB / -39.4 dBFS | the others at 0.75 |
| 5 | LP Freq A | 20.0000000 | 20000.0000000 | 20000.0000000 | continuous | yes | 0.1 dB / -25.6 dBFS | defaults |
| 6 | LP Q A | 0.3000000 | 0.7070000 | 10.0000000 | continuous | yes | 2.8 dB / -22.9 dBFS | defaults |
| 7 | Delta A | 0 | 0 | 2 | continuous | yes | 6.0 dB / -19.7 dBFS | defaults |
| 8 | Bass Tilt Freq A | 20.0000000 | 249.9999542 | 20000.0000000 | continuous | yes | no |  |
| 9 | Bass Tilt Gain A | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no |  |
| 10 | Treb Tilt Freq A | 20.0000000 | 4000.0000000 | 20000.0000000 | continuous | yes | no |  |
| 11 | Treb Tilt Gain A | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no |  |
| 12 | EQ Mode X A | Off | Off | On | switch | yes | -2.6 dB / -28.3 dBFS | defaults |
| 13 | HP Freq B | 20.0000000 | 20.0000000 | 20000.0000000 | continuous | yes | no |  |
| 14 | HP Q B | 0.3000000 | 0.7070000 | 10.0000000 | continuous | yes | no |  |
| 15 | Mid Freq B | 20.0000000 | 632.4559937 | 20000.0000000 | continuous | yes | no |  |
| 16 | Mid Gain B | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no |  |
| 17 | Mid Q B | 0.3000000 | 0.7000000 | 17.3099995 | continuous | yes | no |  |
| 18 | LP Freq B | 20.0000000 | 20000.0000000 | 20000.0000000 | continuous | yes | no |  |
| 19 | LP Q B | 0.3000000 | 0.7070000 | 10.0000000 | continuous | yes | no |  |
| 20 | Delta B | 0 | 0 | 2 | continuous | yes | no |  |
| 21 | Bass Tilt Freq B | 20.0000000 | 249.9999542 | 20000.0000000 | continuous | yes | no |  |
| 22 | Bass Tilt Gain B | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no |  |
| 23 | Treb Tilt Freq B | 20.0000000 | 4000.0000000 | 20000.0000000 | continuous | yes | no |  |
| 24 | Treb Tilt Gain B | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no |  |
| 25 | EQ Mode X B | Off | Off | On | switch | yes | no |  |
| 26 | Bypass | Off | Off | On | switch | yes | -2.6 dB / -28.3 dBFS | defaults |
| 27 | M/S Mode | Off | Off | On | switch | yes | 0.0 dB / -25.7 dBFS | defaults |
| 28 | Mix | 0.0000000 | 1.0000000 | 1.0000000 | continuous | yes | -1.9 dB / -27.6 dBFS | defaults |

## Latency at three sample rates
An impulse (1.0 on all channels) after 4096 samples of silence, the plugin at its default parameters, 1.00 s watched. Measured = position of the largest output sample after the impulse. Reported = getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.

| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |
|---|---|---|---|---|---|
| 44100 Hz | 0 | 0 | 0 | none | none |
| 48000 Hz | 0 | 0 | 0 | none | none |
| 96000 Hz | 0 | 0 | 1 | -7.3 dB | none |

## Delivery of parameters (A, A, B, A)
Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at 0.75), A. Repeatable: A, A, A the same (below -80 dB). Reacts: B differs from A (above -60 dB). As after a change: A and B equal the outputs of the same settings reached by a change of the parameters in the first way.

| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |
|---|---|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | -93.6 dB / -119.3 dBFS ; -93.9 dB / -119.6 dBFS | 19.0 dB / -6.7 dBFS | -93.9 dB / -119.6 dBFS ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 19.0 dB / -6.7 dBFS | -93.9 dB / -119.6 dBFS ; -168.6 dB / -175.3 dBFS |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 19.0 dB / -6.7 dBFS | -93.9 dB / -119.6 dBFS ; -168.6 dB / -175.3 dBFS |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 19.0 dB / -6.7 dBFS | -93.8 dB / -119.5 dBFS ; -163.6 dB / -170.4 dBFS |

Most careful way that works: new instance per render, after prepare every parameter first set to another value, then the target.

## Block sizes
1.00 s of noise through the plugin (setting B) per block size, against block size 512. Steady state: the last 0.10 s (decides; below -100 dB = independent). Whole: the full render (a plugin that smooths its parameters per block differs here, but not in the steady state).

| block size | steady state | whole |
|---|---|---|
| 32 | identical | -165.6 dB / -171.8 dBFS |
| 64 | identical | -165.3 dB / -171.6 dBFS |
| 128 | identical | -170.2 dB / -176.4 dBFS |
| 256 | identical | -164.6 dB / -170.8 dBFS |
| 1024 | identical | -164.7 dB / -170.9 dBFS |
| 2048 | identical | -164.1 dB / -170.4 dBFS |
| 509 | identical | -188.1 dB / -194.4 dBFS |

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

