# Fingerprint: EQoder

- file: `/home/bitzer/.vst3/EQoder.vst3`
- format: VST3, manufacturer: Jade Hochschule, version: 0.0.1
- measured: 2026-10-05 22:38
- channels: mono yes, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| channel layouts (main bus in = out) | mono, stereo |  |
| parameters / changing the audio | 2099 / 2 |  |
| latency at 48 kHz, reported / measured (samples) | **0 / 95** | 44.1 kHz: 0 / 87, 48.0 kHz: 0 / 95, 96.0 kHz: 0 / 191 |
| reported latency = measured at all rates | **no** | 44.1 kHz: 0 / 87, 48.0 kHz: 0 / 95, 96.0 kHz: 0 / 191 |
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
- Latency at 44100 Hz: the plugin reports 0 samples, measured 87 samples
- Latency at 48000 Hz: the plugin reports 0 samples, measured 95 samples
- Latency at 96000 Hz: the plugin reports 0 samples, measured 191 samples

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 2099 parameters, the first 64 examined.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | Number of FilterUnits | 1.000000000000000000000000000000 | 6.000000000000000000000000000000 | 24.00000000000000000000000000000 | continuous | yes | no |  |
| 1 | Number of Filters | 1.000000000000000000000000000000 | 7.000000000000000000000000000000 | 20.00000000000000000000000000000 | continuous | yes | no |  |
| 2 | Gain at f0 | 0.000000000000000000000000000000 | 6.000000000000000000000000000000 | 20.00000000000000000000000000000 | continuous | yes | no |  |
| 3 | Gain at last | 0.000000000000000000000000000000 | 6.000000000000000000000000000000 | 20.00000000000000000000000000000 | continuous | yes | no |  |
| 4 | Form of Gains | 0.000000000000000000000000000000 | 1.000000000000000000000000000000 | 2.000000000000000000000000000000 | continuous | yes | no |  |
| 5 | Q | 0.300000000000000044408920985006 | 20.00000000000000000000000000000 | 40.00000000000000000000000000000 | continuous | yes | no |  |
| 6 | BWSpread | 0.300000000000000044408920985006 | 1.000000000000000000000000000000 | 4.000000000000000000000000000000 | continuous | yes | no |  |
| 7 | FreqSpread | -2.06250000000000000000000000000 | 0.000000000000000000000000000000 | 2.125000000000000000000000000000 | continuous | yes | no |  |
| 8 | OutGain | -89.7500000000000000000000000000 | 0.000000000000000000000000000000 | 10.00000000000000000000000000000 | continuous | yes | -0.0 dB / -24.8 dBFS | defaults |
| 9 | SwitchParallel | 0.000000000000000000000000000000 | 0.000000000000000000000000000000 | 1.000000000000000000000000000000 | continuous | yes | no |  |
| 10 | EnvAttack | 0.100000000000000005551115123125 | 150.0000000000000000000000000000 | 10000.00000000000000000000000000 | continuous | yes | no |  |
| 11 | EnvDecay | 10.00000000000000000000000000000 | 150.0000000000000000000000000000 | 10000.00000000000000000000000000 | continuous | yes | no |  |
| 12 | EnvRelease | 10.00000000000000000000000000000 | 150.0000000000000000000000000000 | 10000.00000000000000000000000000 | continuous | yes | no |  |
| 13 | EnvDelay | 0.000000000000000000000000000000 | 0.000000000000000000000000000000 | 250.0000000000000000000000000000 | continuous | yes | no |  |
| 14 | EnvHold | 0.000000000000000000000000000000 | 0.000000000000000000000000000000 | 250.0000000000000000000000000000 | continuous | yes | no |  |
| 15 | EnvSustainLevel | 0.000000000000000000000000000000 | 1.000000000000000000000000000000 | 1.000000000000000000000000000000 | continuous | yes | no |  |
| 16 | EnvLevel | 0.000000000000000000000000000000 | 1.000000000000000000000000000000 | 1.000000000000000000000000000000 | continuous | yes | no |  |
| 17 | EnvInvert | Off | Off | On | switch | yes | no |  |
| 18 | Bypass | Off | Off | On | switch | yes | 3.0 dB / -21.8 dBFS | defaults |
| 19 | MIDI CC 0|0 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 20 | MIDI CC 0|1 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 21 | MIDI CC 0|2 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 22 | MIDI CC 0|3 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 23 | MIDI CC 0|4 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 24 | MIDI CC 0|5 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 25 | MIDI CC 0|6 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 26 | MIDI CC 0|7 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 27 | MIDI CC 0|8 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 28 | MIDI CC 0|9 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 29 | MIDI CC 0|10 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 30 | MIDI CC 0|11 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 31 | MIDI CC 0|12 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 32 | MIDI CC 0|13 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 33 | MIDI CC 0|14 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 34 | MIDI CC 0|15 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 35 | MIDI CC 0|16 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 36 | MIDI CC 0|17 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 37 | MIDI CC 0|18 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 38 | MIDI CC 0|19 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 39 | MIDI CC 0|20 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 40 | MIDI CC 0|21 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 41 | MIDI CC 0|22 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 42 | MIDI CC 0|23 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 43 | MIDI CC 0|24 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 44 | MIDI CC 0|25 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 45 | MIDI CC 0|26 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 46 | MIDI CC 0|27 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 47 | MIDI CC 0|28 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 48 | MIDI CC 0|29 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 49 | MIDI CC 0|30 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 50 | MIDI CC 0|31 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 51 | MIDI CC 0|32 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 52 | MIDI CC 0|33 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 53 | MIDI CC 0|34 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 54 | MIDI CC 0|35 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 55 | MIDI CC 0|36 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 56 | MIDI CC 0|37 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 57 | MIDI CC 0|38 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 58 | MIDI CC 0|39 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 59 | MIDI CC 0|40 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 60 | MIDI CC 0|41 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 61 | MIDI CC 0|42 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 62 | MIDI CC 0|43 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |
| 63 | MIDI CC 0|44 | 0.0000 | 0.0000 | 1.0000 | continuous | no | no |  |

## Latency at three sample rates
An impulse (1.0 on all channels) after 4096 samples of silence, the plugin at its default parameters, 1.00 s watched. Measured = position of the largest output sample after the impulse. Reported = getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.

| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |
|---|---|---|---|---|---|
| 44100 Hz | 0 | 0 | 87 | none | none |
| 48000 Hz | 0 | 0 | 95 | none | none |
| 96000 Hz | 0 | 0 | 191 | none | none |

## Delivery of parameters (A, A, B, A)
Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at 0.75), A. Repeatable: A, A, A the same (below -80 dB). Reacts: B differs from A (above -60 dB). As after a change: A and B equal the outputs of the same settings reached by a change of the parameters in the first way.

| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |
|---|---|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | -1.7 dB / -26.5 dBFS | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | -1.7 dB / -26.5 dBFS | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | -1.7 dB / -26.5 dBFS | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | -1.7 dB / -26.5 dBFS | identical ; identical |

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

