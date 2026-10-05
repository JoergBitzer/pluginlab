# Fingerprint: 4K EQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/4k-eq-linux/VST3/4K EQ.vst3`
- format: VST3, manufacturer: Dusk Audio, version: 1.0.12
- measured: 2026-10-05 22:38
- channels: mono yes, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| channel layouts (main bus in = out) | mono, stereo |  |
| parameters / changing the audio | 26 / 26 |  |
| latency at 48 kHz, reported / measured (samples) | 49 / 49 | 44.1 kHz: 49 / 49, 48.0 kHz: 49 / 49, 96.0 kHz: 49 / 49 |
| reported latency = measured at all rates | yes | 44.1 kHz: 49 / 49, 48.0 kHz: 49 / 49, 96.0 kHz: 49 / 49 |
| output before the peak of the impulse response | yes |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | **1 of 4 ways** | one instance, parameters set after prepare (stream) |
| block size independent (steady state) | **no** | largest at 509: -62.3 dB / -80.5 dBFS |
| deterministic (two instances, bit exact) | **no** |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | **no** |  |
| digital silence in gives digital silence out | **no** | peak -97.4 dBFS |

Bold: worth a look (see the findings and the details below).

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- Delivery 'new instance per render, parameters set before prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: identical, B: -14.0 dB / -32.3 dBFS): the first delivery was lost. 
- Delivery 'new instance per render, parameters set after prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: identical, B: -68.4 dB / -86.7 dBFS): the first delivery was lost. 
- Delivery 'new instance per render, after prepare every parameter first set to another value, then the target' fails: the settings that were delivered first differ from the same settings reached by a change (A: identical, B: -66.1 dB / -84.4 dBFS): the first delivery was lost. 
- The output depends on the block size also after 1.00 s (largest at block size 509: -62.3 dB / -80.5 dBFS)
- Two instances with the same input give different output
- After parameter jumps to both ends the output does not come back to what it was
- Digital silence in does not give digital silence out (peak -97.4 dBFS)

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 26 parameters.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | HPF Frequency | 20 | 20 | 500 | continuous | yes | -64.8 dB / -83.2 dBFS | the others at 0.75 |
| 1 | HPF Enabled | Off | Off | On | switch | yes | -21.0 dB / -46.0 dBFS | defaults |
| 2 | LPF Frequency | 3000 | 20000 | 20000 | continuous | yes | -65.8 dB / -84.1 dBFS | the others at 0.75 |
| 3 | LPF Enabled | Off | Off | On | switch | yes | -1.6 dB / -26.5 dBFS | defaults |
| 4 | LF Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -61.3 dB / -79.6 dBFS | the others at 0.75 |
| 5 | LF Frequency | 30 | 100 | 480 | continuous | yes | -64.4 dB / -82.7 dBFS | the others at 0.75 |
| 6 | LF Bell Mode | Off | Off | On | switch | yes | -63.4 dB / -81.8 dBFS | the others at 0.75 |
| 7 | LM Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -74.0 dB / -92.3 dBFS | the others at 0.75 |
| 8 | LM Frequency | 200 | 600 | 2500 | continuous | yes | -67.4 dB / -85.7 dBFS | the others at 0.75 |
| 9 | LM Q | 0.40 | 0.70 | 4.00 | continuous | yes | -62.3 dB / -80.6 dBFS | the others at 0.75 |
| 10 | HM Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -65.0 dB / -83.3 dBFS | the others at 0.75 |
| 11 | HM Frequency | 600 | 2000 | 7000 | continuous | yes | -67.8 dB / -86.1 dBFS | the others at 0.75 |
| 12 | HM Q | 0.40 | 0.70 | 4.00 | continuous | yes | -64.7 dB / -83.0 dBFS | the others at 0.75 |
| 13 | HF Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -59.3 dB / -77.7 dBFS | the others at 0.75 |
| 14 | HF Frequency | 1500 | 8000 | 16000 | continuous | yes | -63.9 dB / -82.2 dBFS | the others at 0.75 |
| 15 | HF Bell Mode | Off | Off | On | switch | yes | -62.5 dB / -80.9 dBFS | the others at 0.75 |
| 16 | EQ Type | Brown | Brown | Black | switch | yes | -43.8 dB / -68.7 dBFS | defaults |
| 17 | Bypass | Off | Off | On | switch | yes | 3.0 dB / -21.9 dBFS | defaults |
| 18 | Input Gain | -12.0 | 0.0 | 12.0 | continuous | yes | -0.0 dB / -25.0 dBFS | defaults |
| 19 | Output Gain | -12.0 | 0.0 | 12.0 | continuous | yes | -62.7 dB / -81.1 dBFS | the others at 0.75 |
| 20 | Saturation | 0 | 0 | 100 | continuous | yes | -66.5 dB / -84.8 dBFS | the others at 0.75 |
| 21 | Oversampling | 2x | 2x | 4x | switch | yes | -63.6 dB / -81.9 dBFS | the others at 0.75 |
| 22 | M/S Mode | Off | Off | On | switch | yes | -60.0 dB / -85.0 dBFS | defaults |
| 23 | Spectrum Pre/Post | Off | Off | On | switch | yes | -60.0 dB / -78.3 dBFS | the others at 0.75 |
| 24 | Auto Gain Compensation | Off | On | On | switch | yes | -19.1 dB / -37.4 dBFS | the others at 0.75 |
| 25 | Program | Default | Default | Master Bus Sweetening | 15 | yes | -9.4 dB / -34.3 dBFS | defaults |

## Latency at three sample rates
An impulse (1.0 on all channels) after 4096 samples of silence, the plugin at its default parameters, 1.00 s watched. Measured = position of the largest output sample after the impulse. Reported = getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.

| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |
|---|---|---|---|---|---|
| 44100 Hz | 49 | 49 | 49 | -34.5 dB | none |
| 48000 Hz | 49 | 49 | 49 | -34.5 dB | none |
| 96000 Hz | 49 | 49 | 49 | -34.1 dB | none |

## Delivery of parameters (A, A, B, A)
Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at 0.75), A. Repeatable: A, A, A the same (below -80 dB). Reacts: B differs from A (above -60 dB). As after a change: A and B equal the outputs of the same settings reached by a change of the parameters in the first way.

| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |
|---|---|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 1.5 dB / -23.5 dBFS | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | no | FAILS | identical ; identical | 0.0 dB / -24.9 dBFS | identical ; -14.0 dB / -32.3 dBFS |
| new instance per render, parameters set after prepare | yes | yes | no | FAILS | identical ; identical | 1.5 dB / -23.5 dBFS | identical ; -68.4 dB / -86.7 dBFS |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | no | FAILS | identical ; identical | 1.5 dB / -23.5 dBFS | identical ; -66.1 dB / -84.4 dBFS |

Most careful way that works: one instance, parameters set after prepare (stream).

## Block sizes
1.00 s of noise through the plugin (setting B) per block size, against block size 512. Steady state: the last 0.10 s (decides; below -100 dB = independent). Whole: the full render (a plugin that smooths its parameters per block differs here, but not in the steady state).

| block size | steady state | whole |
|---|---|---|
| 32 | -76.4 dB / -94.6 dBFS | -76.3 dB / -94.5 dBFS |
| 64 | -71.0 dB / -89.2 dBFS | -71.0 dB / -89.3 dBFS |
| 128 | -79.5 dB / -97.7 dBFS | -79.4 dB / -97.7 dBFS |
| 256 | -66.7 dB / -84.9 dBFS | -66.8 dB / -85.0 dBFS |
| 1024 | -67.2 dB / -85.4 dBFS | -67.2 dB / -85.4 dBFS |
| 2048 | -67.3 dB / -85.5 dBFS | -67.3 dB / -85.6 dBFS |
| 509 | -62.3 dB / -80.5 dBFS | -62.2 dB / -80.5 dBFS |

## Other
- deterministic (two instances, the same noise, bit exact): no
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (every parameter that changes the audio to 0 and 1 and back, then the output of setting B again): no
- digital silence in gives digital silence out: no (peak -97.4 dBFS)

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

