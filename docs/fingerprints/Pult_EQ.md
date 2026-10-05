# Fingerprint: Pult EQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/Pult.EQ_1_0_0_Linux_vst3/Pult EQ_1_0_0_Linux_vst3/Pult EQ.vst3`
- format: VST3, manufacturer: Consistent Interruption, version: 1.0.0
- measured: 2026-10-05 21:37
- channels: mono no, stereo yes

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- Two instances with the same input give different output
- Digital silence in does not give digital silence out (peak -133.7 dBFS)

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 29 parameters.

| no. | name | min | default | max | steps | automatable | changes the audio | measured with |
|---|---|---|---|---|---|---|---|---|
| 0 | In | -25.0 | 0.0 | 25.0 | continuous | yes | 9.1 dB / -15.6 dBFS | defaults |
| 1 | Stereo/Mid/Side | 0.0 | 1.0 | 3.0 | continuous | yes | -2.9 dB / -6.1 dBFS | the others at 0.75 |
| 2 | Drive | 0.0 | 3.0 | 10.0 | continuous | yes | -35.6 dB / -60.4 dBFS | defaults |
| 3 | Out | -25.0 | 0.0 | 25.0 | continuous | yes | 9.3 dB / -15.4 dBFS | defaults |
| 4 | Low Boost | 0.0 | 0.0 | 10.0 | continuous | yes | -18.4 dB / -43.1 dBFS | defaults |
| 5 | Hight Adjust | 0.0 | 0.0 | 10.0 | continuous | yes | -16.0 dB / -19.2 dBFS | the others at 0.75 |
| 6 | Low Atten | 0.0 | 0.0 | 10.0 | continuous | yes | -15.5 dB / -40.3 dBFS | defaults |
| 7 | Hight Atten | 0.0 | 0.0 | 10.0 | continuous | yes | -4.4 dB / -29.1 dBFS | defaults |
| 8 | Hight Boost | 0.0 | 0.0 | 10.0 | continuous | yes | -0.6 dB / -25.3 dBFS | defaults |
| 9 | Low Frequency | 0.0 | 0.0 | 3.0 | continuous | yes | -65.1 dB / -89.9 dBFS | defaults |
| 10 | Hight Frequency | 0.0 | 0.0 | 6.0 | continuous | yes | -9.1 dB / -12.3 dBFS | the others at 0.75 |
| 11 | Atten Frequency | 0.0 | 0.0 | 2.0 | continuous | yes | -11.8 dB / -15.1 dBFS | the others at 0.75 |
| 12 | Mid Low Boost | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 13 | Mid Hight Adjust | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 14 | Mid Low Atten | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 15 | Mid Hight Atten | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 16 | Mid Hight Boost | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 17 | Mid Low Frequency | 0.0 | 0.0 | 3.0 | continuous | yes | no |  |
| 18 | Mid Hight Frequency | 0.0 | 0.0 | 6.0 | continuous | yes | no |  |
| 19 | Mid Atten Frequency | 0.0 | 0.0 | 2.0 | continuous | yes | no |  |
| 20 | Side Low Boost | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 21 | Side Hight Adjust | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 22 | Side Low Atten | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 23 | Side Hight Atten | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 24 | Side Hight Boost | 0.0 | 0.0 | 10.0 | continuous | yes | no |  |
| 25 | Side Low Frequency | 0.0 | 0.0 | 3.0 | continuous | yes | no |  |
| 26 | Side Hight Frequency | 0.0 | 0.0 | 6.0 | continuous | yes | no |  |
| 27 | Side Hight Atten Frequency | 0.0 | 0.0 | 2.0 | continuous | yes | no |  |
| 28 | Bypass | Off | Off | On | switch | yes | -39.9 dB / -64.6 dBFS | defaults |

## Latency at three sample rates
An impulse (1.0 on all channels) after 4096 samples of silence, the plugin at its default parameters, 1.00 s watched. Measured = position of the largest output sample after the impulse. Reported = getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.

| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |
|---|---|---|---|---|---|
| 44100 Hz | 1 | 1 | 1 | none | none |
| 48000 Hz | 1 | 1 | 1 | none | none |
| 96000 Hz | 1 | 1 | 1 | none | none |

## Delivery of parameters (A, A, B, A)
Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at 0.75), A. Repeatable: A, A, A the same (below -80 dB). Reacts: B differs from A (above -60 dB). As after a change: A and B equal the outputs of the same settings reached by a change of the parameters in the first way.

| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |
|---|---|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | -141.9 dB / -166.7 dBFS ; -142.6 dB / -167.3 dBFS | 22.9 dB / -1.9 dBFS | -142.6 dB / -167.3 dBFS ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | -142.5 dB / -167.3 dBFS ; -142.6 dB / -167.3 dBFS | 22.9 dB / -1.9 dBFS | -142.7 dB / -167.4 dBFS ; -142.5 dB / -143.7 dBFS |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | -142.5 dB / -167.2 dBFS ; -142.5 dB / -167.3 dBFS | 22.9 dB / -1.9 dBFS | -142.5 dB / -167.2 dBFS ; -142.4 dB / -143.7 dBFS |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | -142.6 dB / -167.3 dBFS ; -142.6 dB / -167.4 dBFS | 22.9 dB / -1.9 dBFS | -142.6 dB / -167.4 dBFS ; -142.4 dB / -143.7 dBFS |

Most careful way that works: new instance per render, after prepare every parameter first set to another value, then the target.

## Block sizes
1.00 s of noise through the plugin (setting B) per block size, against block size 512. Steady state: the last 0.10 s (decides; below -100 dB = independent). Whole: the full render (a plugin that smooths its parameters per block differs here, but not in the steady state).

| block size | steady state | whole |
|---|---|---|
| 32 | -142.6 dB / -143.7 dBFS | -142.5 dB / -143.7 dBFS |
| 64 | -142.5 dB / -143.6 dBFS | -142.5 dB / -143.7 dBFS |
| 128 | -142.7 dB / -143.8 dBFS | -142.6 dB / -143.7 dBFS |
| 256 | -142.6 dB / -143.8 dBFS | -142.5 dB / -143.7 dBFS |
| 1024 | -142.5 dB / -143.6 dBFS | -142.5 dB / -143.7 dBFS |
| 2048 | -142.5 dB / -143.6 dBFS | -142.5 dB / -143.7 dBFS |
| 509 | -142.6 dB / -143.7 dBFS | -142.5 dB / -143.7 dBFS |

## Other
- deterministic (two instances, the same noise, bit exact): no
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (every parameter that changes the audio to 0 and 1 and back, then the output of setting B again): yes
- digital silence in gives digital silence out: no (peak -133.7 dBFS)

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

