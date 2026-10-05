# Fingerprint: Free EQ

- file: `/tmp/claude-1000/-home-bitzer-AudioDev/cb378125-017c-4b6c-a2ae-35e52500030b/scratchpad/venn/FreeEQ.vst3`
- format: VST3, manufacturer: Venn Audio, version: 1.5.7
- measured: 2026-10-05 22:38
- channels: mono yes, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| channel layouts (main bus in = out) | mono, stereo |  |
| parameters / changing the audio | **37 / 0** |  |
| latency at 48 kHz, reported / measured (samples) | 0 / 0 | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| reported latency = measured at all rates | yes | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| output before the peak of the impulse response | no |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | **none** |  |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | yes |  |
| digital silence in gives digital silence out | yes |  |

Bold: worth a look (see the findings and the details below).

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- No parameter changed the audio (an instrument, a pure analyser, parameters that act only together, or parameters that are not read after prepare)

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 37 parameters.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | Band 1 Enabled | Off | Off | On | switch | yes | no |  |
| 1 | Band 1 Frequency | -1.000 | -0.682 | 1.000 | continuous | yes | no |  |
| 2 | Band 1 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | no |  |
| 3 | Band 1 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | no |  |
| 4 | Band 1 Type | 1 | 5 | 7 | continuous | yes | no |  |
| 5 | Band 2 Enabled | Off | Off | On | switch | yes | no |  |
| 6 | Band 2 Frequency | -1.000 | -0.334 | 1.000 | continuous | yes | no |  |
| 7 | Band 2 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | no |  |
| 8 | Band 2 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | no |  |
| 9 | Band 2 Type | 1 | 5 | 7 | continuous | yes | no |  |
| 10 | Band 3 Enabled | Off | Off | On | switch | yes | no |  |
| 11 | Band 3 Frequency | -1.000 | -0.132 | 1.000 | continuous | yes | no |  |
| 12 | Band 3 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | no |  |
| 13 | Band 3 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | no |  |
| 14 | Band 3 Type | 1 | 5 | 7 | continuous | yes | no |  |
| 15 | Band 4 Enabled | Off | Off | On | switch | yes | no |  |
| 16 | Band 4 Frequency | -1.000 | 0.132 | 1.000 | continuous | yes | no |  |
| 17 | Band 4 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | no |  |
| 18 | Band 4 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | no |  |
| 19 | Band 4 Type | 1 | 5 | 7 | continuous | yes | no |  |
| 20 | Band 5 Enabled | Off | Off | On | switch | yes | no |  |
| 21 | Band 5 Frequency | -1.000 | 0.598 | 1.000 | continuous | yes | no |  |
| 22 | Band 5 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | no |  |
| 23 | Band 5 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | no |  |
| 24 | Band 5 Type | 1 | 5 | 7 | continuous | yes | no |  |
| 25 | Band 6 Enabled | Off | Off | On | switch | yes | no |  |
| 26 | Band 6 Frequency | -1.000 | 0.800 | 1.000 | continuous | yes | no |  |
| 27 | Band 6 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | no |  |
| 28 | Band 6 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | no |  |
| 29 | Band 6 Type | 1 | 5 | 7 | continuous | yes | no |  |
| 30 | Band 1 Slope | 0 | 1 | 3 | continuous | yes | no |  |
| 31 | Band 2 Slope | 0 | 1 | 3 | continuous | yes | no |  |
| 32 | Band 3 Slope | 0 | 1 | 3 | continuous | yes | no |  |
| 33 | Band 4 Slope | 0 | 1 | 3 | continuous | yes | no |  |
| 34 | Band 5 Slope | 0 | 1 | 3 | continuous | yes | no |  |
| 35 | Band 6 Slope | 0 | 1 | 3 | continuous | yes | no |  |
| 36 | Bypass | Off | Off | On | switch | yes | no |  |

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
| one instance, parameters set after prepare (stream) | yes | no | yes | FAILS | identical ; identical | identical | identical ; identical |
| new instance per render, parameters set before prepare | yes | no | yes | FAILS | identical ; identical | identical | identical ; identical |
| new instance per render, parameters set after prepare | yes | no | yes | FAILS | identical ; identical | identical | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | no | yes | FAILS | identical ; identical | identical | identical ; identical |

No way of delivering the parameters passed the test.

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

