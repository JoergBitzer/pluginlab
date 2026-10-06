# Fingerprint: FreeEQ8

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/FreeEQ8/FreeEQ8-v2.3.1-Linux/FreeEQ8.vst3`
- format: VST3, manufacturer: TizWildinEntertainment, version: 2.3.1
- measured: 2026-10-06 10:07
- channels: mono no, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| main-bus layouts accepted | stereo | measured with 2 channel(s) |
| side-chain input (more than one input bus) | no |  |
| MIDI | none |  |
| channels independent (one input driven, the other silent) | yes | L to R: silent, R to L: silent |
| parameters / changing the audio | 129 / 42 |  |
| latency at 48 kHz, reported / measured (samples) | 0 / 0 | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| reported latency = measured at all rates | yes | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| output before the peak of the impulse response | no |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | 4 of 4 ways | new instance per render, after prepare every parameter first set to another value, then the target |
| time-invariant (the same noise twice through one instance) | yes | identical |
| settles within 0.25 s after a parameter change | yes | identical |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | yes | continuous parameters: yes, switches and choices: yes |
| digital silence in gives digital silence out | yes |  |

Bold: worth a look (see the findings and the details below).

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- nothing unusual found

## Channels and buses
The buses of the plugin as it is created, the main-bus layouts it accepts (other buses switched off where possible), MIDI, and the coupling of the channels at the setting B: one input channel gets noise, the other silence; the output of the silent channel relative to the output of the driven one (below -100 dB = independent channels). The measurement runs with 2 channel(s); other channels of the plugin get silence.

| bus | name | default layout |
|---|---|---|
| input 0 | Input | Stereo |
| output 0 | Output | Stereo |

| layout | accepted |
|---|---|
| mono | no |
| stereo | yes |
| mono in, stereo out | no |
| LCR | no |
| quad | no |
| 5.1 | no |
| 7.1 | no |
| ambisonics 1st order | no |

- side chain: no; MIDI in: no, out: no; instrument: no
- coupling L to R: silent, R to L: silent: channels independent yes

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 129 parameters, the first 64 examined. Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a width or mid/side control reacts only to this one). The scan started from the defaults.

| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Output Gain | -24.00 | 0.00 | 24.00 | continuous | yes | 9.5 dB / -15.3 dBFS | 9.5 dB / -15.3 dBFS | defaults |
| 1 | Scale | 0.10 | 1.00 | 2.00 | continuous | yes | no | no |  |
| 2 | Adaptive Q | Off | Off | On | switch | yes | -3.5 dB / 8.3 dBFS | -2.4 dB / 9.4 dBFS (channel 2) | defaults, the others at 0.75 |
| 3 | Oversampling | 1x | 1x | 8x | 4 | yes | 3.0 dB / -21.8 dBFS | 3.0 dB / -21.8 dBFS | defaults |
| 4 | Processing Mode | Stereo | Stereo | Mid-Side | switch | yes | no | -0.6 dB / 11.2 dBFS | defaults, the others at 0.75 |
| 5 | Linear Phase | Off | Off | On | switch | yes | -0.0 dB / -24.8 dBFS | -0.0 dB / -24.8 dBFS (channel 2) | defaults |
| 6 | Auto Gain | Off | Off | On | switch | yes | -1.0 dB / 10.8 dBFS | -1.0 dB / 10.8 dBFS (channel 2) | defaults, the others at 0.75 |
| 7 | Intent Mode | None | None | Master Polish | 5 | yes | no | no |  |
| 8 | Band 1 On | Off | On | On | switch | yes | 2.9 dB / 14.8 dBFS | 2.9 dB / 14.8 dBFS | defaults, the others at 0.75 |
| 9 | Band 1 Solo | Off | Off | On | switch | yes | -0.2 dB / 11.6 dBFS | -0.2 dB / 11.6 dBFS (channel 2) | defaults, the others at 0.75 |
| 10 | Band 1 Type | Bell | Bell | Bandpass | 6 | yes | 0.0 dB / -24.8 dBFS | 0.0 dB / -24.8 dBFS | defaults |
| 11 | Band 1 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | 2.9 dB / 14.7 dBFS | 2.9 dB / 14.7 dBFS (channel 2) | defaults, the others at 0.75 |
| 12 | Band 1 Channel | Both | Both | R / Side | 3 | yes | 3.3 dB / 15.1 dBFS | 3.3 dB / 15.1 dBFS | defaults, the others at 0.75 |
| 13 | Band 1 Link | -- | -- | B | 3 | yes | no | no |  |
| 14 | Band 1 Freq | 20.000 | 80.000 | 20000.000 | continuous | yes | 3.3 dB / 15.1 dBFS | 3.3 dB / 15.1 dBFS | defaults, the others at 0.75 |
| 15 | Band 1 Q | 0.100 | 1.000 | 24.000 | continuous | yes | 0.2 dB / 12.0 dBFS (channel 2) | 0.5 dB / 12.3 dBFS (channel 2) | defaults, the others at 0.75 |
| 16 | Band 1 Gain | -24.00 | 0.00 | 24.00 | continuous | yes | -14.5 dB / -39.3 dBFS | -14.5 dB / -39.3 dBFS | defaults |
| 17 | Band 1 Drive | 0.0 | 0.0 | 100.0 | continuous | yes | 15.5 dB / -9.3 dBFS | 15.5 dB / -9.3 dBFS (channel 2) | defaults |
| 18 | Band 1 Dyn On | Off | Off | On | switch | yes | no | no |  |
| 19 | Band 1 Threshold | -60.0 | -20.0 | 0.0 | continuous | yes | no | no |  |
| 20 | Band 1 Ratio | 1.0 | 4.0 | 20.0 | continuous | yes | no | no |  |
| 21 | Band 1 Attack | 0.1 | 10.0 | 100.0 | continuous | yes | no | no |  |
| 22 | Band 1 Release | 1 | 100 | 1000 | continuous | yes | no | no |  |
| 23 | Band 2 On | Off | On | On | switch | yes | -4.7 dB / 7.1 dBFS | -4.7 dB / 7.1 dBFS | defaults, the others at 0.75 |
| 24 | Band 2 Solo | Off | Off | On | switch | yes | 0.1 dB / 11.9 dBFS | 0.1 dB / 11.9 dBFS | defaults, the others at 0.75 |
| 25 | Band 2 Type | Bell | Bell | Bandpass | 6 | yes | 0.1 dB / -24.8 dBFS | 0.1 dB / -24.7 dBFS (channel 2) | defaults |
| 26 | Band 2 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | 1.7 dB / 13.5 dBFS | 1.7 dB / 13.5 dBFS | defaults, the others at 0.75 |
| 27 | Band 2 Channel | Both | Both | R / Side | 3 | yes | -3.2 dB / 8.6 dBFS | -3.2 dB / 8.6 dBFS | defaults, the others at 0.75 |
| 28 | Band 2 Link | -- | -- | B | 3 | yes | no | no |  |
| 29 | Band 2 Freq | 20.000 | 250.000 | 20000.000 | continuous | yes | -3.3 dB / 8.5 dBFS | -3.3 dB / 8.5 dBFS | defaults, the others at 0.75 |
| 30 | Band 2 Q | 0.100 | 1.000 | 24.000 | continuous | yes | -2.7 dB / 9.1 dBFS | -0.5 dB / 11.3 dBFS (channel 2) | defaults, the others at 0.75 |
| 31 | Band 2 Gain | -24.00 | 0.00 | 24.00 | continuous | yes | -12.0 dB / -36.9 dBFS | -11.3 dB / -36.1 dBFS (channel 2) | defaults |
| 32 | Band 2 Drive | 0.0 | 0.0 | 100.0 | continuous | yes | 15.5 dB / -9.3 dBFS | 15.5 dB / -9.3 dBFS (channel 2) | defaults |
| 33 | Band 2 Dyn On | Off | Off | On | switch | yes | no | no |  |
| 34 | Band 2 Threshold | -60.0 | -20.0 | 0.0 | continuous | yes | no | no |  |
| 35 | Band 2 Ratio | 1.0 | 4.0 | 20.0 | continuous | yes | no | no |  |
| 36 | Band 2 Attack | 0.1 | 10.0 | 100.0 | continuous | yes | no | no |  |
| 37 | Band 2 Release | 1 | 100 | 1000 | continuous | yes | no | no |  |
| 38 | Band 3 On | Off | On | On | switch | yes | -8.0 dB / 3.8 dBFS | -7.8 dB / 4.0 dBFS (channel 2) | defaults, the others at 0.75 |
| 39 | Band 3 Solo | Off | Off | On | switch | yes | 0.1 dB / 11.9 dBFS | 0.1 dB / 11.9 dBFS | defaults, the others at 0.75 |
| 40 | Band 3 Type | Bell | Bell | Bandpass | 6 | yes | 0.1 dB / -24.7 dBFS | 0.1 dB / -24.7 dBFS | defaults |
| 41 | Band 3 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | -1.5 dB / 10.3 dBFS | -1.5 dB / 10.3 dBFS | defaults, the others at 0.75 |
| 42 | Band 3 Channel | Both | Both | R / Side | 3 | yes | -7.5 dB / 4.3 dBFS | -7.5 dB / 4.3 dBFS | defaults, the others at 0.75 |
| 43 | Band 3 Link | -- | -- | B | 3 | yes | no | no |  |
| 44 | Band 3 Freq | 20.000 | 500.000 | 20000.000 | continuous | yes | -7.8 dB / 4.0 dBFS | -7.8 dB / 4.0 dBFS | defaults, the others at 0.75 |
| 45 | Band 3 Q | 0.100 | 1.000 | 24.000 | continuous | yes | -8.5 dB / 3.3 dBFS | -8.1 dB / 3.7 dBFS (channel 2) | defaults, the others at 0.75 |
| 46 | Band 3 Gain | -24.00 | 0.00 | 24.00 | continuous | yes | -8.2 dB / -33.0 dBFS | -8.1 dB / -32.9 dBFS (channel 2) | defaults |
| 47 | Band 3 Drive | 0.0 | 0.0 | 100.0 | continuous | yes | 15.5 dB / -9.3 dBFS | 15.5 dB / -9.3 dBFS (channel 2) | defaults |
| 48 | Band 3 Dyn On | Off | Off | On | switch | yes | no | no |  |
| 49 | Band 3 Threshold | -60.0 | -20.0 | 0.0 | continuous | yes | no | no |  |
| 50 | Band 3 Ratio | 1.0 | 4.0 | 20.0 | continuous | yes | no | no |  |
| 51 | Band 3 Attack | 0.1 | 10.0 | 100.0 | continuous | yes | no | no |  |
| 52 | Band 3 Release | 1 | 100 | 1000 | continuous | yes | no | no |  |
| 53 | Band 4 On | Off | On | On | switch | yes | -9.5 dB / 2.3 dBFS | -8.9 dB / 2.9 dBFS (channel 2) | defaults, the others at 0.75 |
| 54 | Band 4 Solo | Off | Off | On | switch | yes | 0.1 dB / 11.9 dBFS | 0.1 dB / 11.9 dBFS | defaults, the others at 0.75 |
| 55 | Band 4 Type | Bell | Bell | Bandpass | 6 | yes | 0.2 dB / -24.6 dBFS | 0.2 dB / -24.6 dBFS | defaults |
| 56 | Band 4 Slope | 12 dB | 12 dB | 48 dB | 3 | yes | -4.9 dB / 6.9 dBFS | -4.9 dB / 6.9 dBFS | defaults, the others at 0.75 |
| 57 | Band 4 Channel | Both | Both | R / Side | 3 | yes | -11.8 dB / -0.0 dBFS | -11.8 dB / -0.0 dBFS | defaults, the others at 0.75 |
| 58 | Band 4 Link | -- | -- | B | 3 | yes | no | no |  |
| 59 | Band 4 Freq | 20.000 | 1000.000 | 20000.000 | continuous | yes | -12.4 dB / -0.6 dBFS | -12.4 dB / -0.6 dBFS | defaults, the others at 0.75 |
| 60 | Band 4 Q | 0.100 | 1.000 | 24.000 | continuous | yes | -12.7 dB / -0.9 dBFS | -12.7 dB / -0.9 dBFS | defaults, the others at 0.75 |
| 61 | Band 4 Gain | -24.00 | 0.00 | 24.00 | continuous | yes | -5.5 dB / -30.3 dBFS | -5.5 dB / -30.3 dBFS (channel 2) | defaults |
| 62 | Band 4 Drive | 0.0 | 0.0 | 100.0 | continuous | yes | 15.5 dB / -9.3 dBFS | 15.5 dB / -9.3 dBFS (channel 2) | defaults |
| 63 | Band 4 Dyn On | Off | Off | On | switch | yes | no | no |  |

## The settings A and B
A = the defaults. B = the parameters that change the audio at 0.75 of their range (switches left out, starting from the defaults); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters where B differs from A are listed.

| no. | name | A (normalised) | A | B (normalised) | B |
|---|---|---|---|---|---|
| 0 | Output Gain | 0.500 | 0.00 | 0.750 | 12.00 |
| 3 | Oversampling | 0.000 | 1x | 0.750 | 4x |
| 10 | Band 1 Type | 0.000 | Bell | 0.750 | LowPass |
| 11 | Band 1 Slope | 0.000 | 12 dB | 0.750 | 48 dB |
| 12 | Band 1 Channel | 0.000 | Both | 0.750 | R / Side |
| 14 | Band 1 Freq | 0.055 | 80.000 | 0.750 | 11258.751 |
| 15 | Band 1 Q | 0.194 | 1.000 | 0.750 | 13.544 |
| 16 | Band 1 Gain | 0.500 | 0.00 | 0.750 | 12.00 |
| 17 | Band 1 Drive | 0.000 | 0.0 | 0.750 | 75.0 |
| 25 | Band 2 Type | 0.000 | Bell | 0.750 | LowPass |
| 26 | Band 2 Slope | 0.000 | 12 dB | 0.750 | 48 dB |
| 27 | Band 2 Channel | 0.000 | Both | 0.750 | R / Side |
| 29 | Band 2 Freq | 0.107 | 250.000 | 0.750 | 11258.751 |
| 30 | Band 2 Q | 0.194 | 1.000 | 0.750 | 13.544 |
| 31 | Band 2 Gain | 0.500 | 0.00 | 0.750 | 12.00 |
| 32 | Band 2 Drive | 0.000 | 0.0 | 0.750 | 75.0 |
| 40 | Band 3 Type | 0.000 | Bell | 0.750 | LowPass |
| 41 | Band 3 Slope | 0.000 | 12 dB | 0.750 | 48 dB |
| 42 | Band 3 Channel | 0.000 | Both | 0.750 | R / Side |
| 44 | Band 3 Freq | 0.155 | 500.000 | 0.750 | 11258.751 |
| 45 | Band 3 Q | 0.194 | 1.000 | 0.750 | 13.544 |
| 46 | Band 3 Gain | 0.500 | 0.00 | 0.750 | 12.00 |
| 47 | Band 3 Drive | 0.000 | 0.0 | 0.750 | 75.0 |
| 55 | Band 4 Type | 0.000 | Bell | 0.750 | LowPass |
| 56 | Band 4 Slope | 0.000 | 12 dB | 0.750 | 48 dB |
| 57 | Band 4 Channel | 0.000 | Both | 0.750 | R / Side |
| 59 | Band 4 Freq | 0.221 | 1000.000 | 0.750 | 11258.751 |
| 60 | Band 4 Q | 0.194 | 1.000 | 0.750 | 13.544 |
| 61 | Band 4 Gain | 0.500 | 0.00 | 0.750 | 12.00 |
| 62 | Band 4 Drive | 0.000 | 0.0 | 0.750 | 75.0 |

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
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 35.9 dB / 11.1 dBFS (channel 2) | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 35.9 dB / 11.1 dBFS (channel 2) | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 35.9 dB / 11.1 dBFS (channel 2) | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 35.9 dB / 11.1 dBFS (channel 2) | identical ; identical |

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
- time-invariant (one instance: settled for 2.00 s, noise, 0.50 s silence, the same noise again; the two outputs the same): yes (identical)
- settles within 0.25 s after a parameter change (the output then against the output after 2.00 s): yes (identical)
- deterministic (two instances, the same noise, bit exact): yes
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters and switches/choices in separate runs): continuous yes, switches and choices yes
- digital silence in gives digital silence out: yes

## Settings used
```
{
  "reactsAboveDb": -80.0,
  "differentAboveDb": -60.0,
  "sameBelowDb": -80.0,
  "blockIndependentBelowDb": -100.0,
  "silentReferenceDbfs": -150.0,
  "couplingBelowDb": -100.0,
  "silenceBelowDbfs": -200.0,
  "timeInvarianceGapSeconds": 0.5,
  "maximumPairs": 64,
  "longSettleSeconds": 2.0,
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

