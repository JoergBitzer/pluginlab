# Fingerprint: PeakEqualizer

- file: `/home/bitzer/.vst3/PeakEqualizer.vst3`
- format: VST3, manufacturer: Jade_Hochschule, version: 0.1.0
- measured: 2026-10-05 21:37
- channels: mono yes, stereo yes

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- Latency at 44100 Hz: the plugin reports 0 samples, measured 88 samples
- Latency at 48000 Hz: the plugin reports 0 samples, measured 96 samples
- Latency at 96000 Hz: the plugin reports 0 samples, measured 192 samples

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 4 parameters.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | Gain | -24.0000000000000000000000000000 | 0.000000000000000000000000000000 | 24.00000000000000000000000000000 | continuous | yes | -5.6 dB / -30.4 dBFS | defaults |
| 1 | Q | 0.089999996125698089599609375000 | 1.000000000000000000000000000000 | 10.00000000000000000000000000000 | continuous | yes | -7.1 dB / -30.2 dBFS | the others at 0.75 |
| 2 | Freq | 50.00000000000000000000000000000 | 1000.000000000000000000000000000 | 15000.00000000000000000000000000 | continuous | yes | -1.1 dB / -24.3 dBFS | the others at 0.75 |
| 3 | Bypass | Off | Off | On | switch | yes | 3.0 dB / -21.8 dBFS | defaults |

## Latency at three sample rates
An impulse (1.0 on all channels) after 4096 samples of silence, the plugin at its default parameters, 1.00 s watched. Measured = position of the largest output sample after the impulse. Reported = getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.

| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |
|---|---|---|---|---|---|
| 44100 Hz | 0 | 0 | 88 | none | none |
| 48000 Hz | 0 | 0 | 96 | none | none |
| 96000 Hz | 0 | 0 | 192 | none | none |

## Delivery of parameters (A, A, B, A)
Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at 0.75), A. Repeatable: A, A, A the same (below -80 dB). Reacts: B differs from A (above -60 dB). As after a change: A and B equal the outputs of the same settings reached by a change of the parameters in the first way.

| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |
|---|---|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | -5.2 dB / -30.0 dBFS | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | -5.2 dB / -30.0 dBFS | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | -5.2 dB / -30.0 dBFS | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | -5.2 dB / -30.0 dBFS | identical ; identical |

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

