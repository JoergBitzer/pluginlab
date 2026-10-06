# Fingerprint: 4K EQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/4k-eq-linux/VST3/4K EQ.vst3`
- format: VST3, manufacturer: Dusk Audio, version: 1.0.12
- measured: 2026-10-06 10:07
- channels: mono yes, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| main-bus layouts accepted | mono, stereo | measured with 2 channel(s) |
| side-chain input (more than one input bus) | no |  |
| MIDI | none |  |
| channels independent (one input driven, the other silent) | no | L to R: -60.0 dB re the driven channel (-78.3 dBFS), R to L: -60.0 dB re the driven channel (-78.3 dBFS) |
| parameters / changing the audio | 26 / 26 |  |
| latency at 48 kHz, reported / measured (samples) | 49 / 49 | 44.1 kHz: 49 / 49, 48.0 kHz: 49 / 49, 96.0 kHz: 49 / 49 |
| reported latency = measured at all rates | yes | 44.1 kHz: 49 / 49, 48.0 kHz: 49 / 49, 96.0 kHz: 49 / 49 |
| output before the peak of the impulse response | yes |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | **2 of 4 ways** | new instance per render, after prepare every parameter first set to another value, then the target |
| time-invariant (the same noise twice through one instance) | yes | -85.5 dB / -103.8 dBFS (channel 2) |
| settles within 0.25 s after a parameter change | yes | -80.3 dB / -98.6 dBFS |
| block size independent (steady state) | **no** | largest at 32: -66.5 dB / -84.7 dBFS |
| deterministic (two instances, bit exact) | **no** |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | **no** | continuous parameters: no, switches and choices: no |
| digital silence in gives digital silence out | **no** | peak -96.2 dBFS |

Bold: worth a look (see the findings and the details below).

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- Delivery 'new instance per render, parameters set before prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: identical, B: -13.9 dB / -32.2 dBFS (channel 2)): the first delivery was lost. 
- Delivery 'new instance per render, parameters set after prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: identical, B: -79.2 dB / -97.5 dBFS): the first delivery was lost. 
- The output depends on the block size also after 1.00 s (largest at block size 32: -66.5 dB / -84.7 dBFS)
- Two instances with the same input give different output
- After jumps of the continuous parameters and the switches and choices to both ends the output does not come back to what it was
- Digital silence in does not give digital silence out (peak -96.2 dBFS)

## Channels and buses
The buses of the plugin as it is created, the main-bus layouts it accepts (other buses switched off where possible), MIDI, and the coupling of the channels at the setting B: one input channel gets noise, the other silence; the output of the silent channel relative to the output of the driven one (below -100 dB = independent channels). The measurement runs with 2 channel(s); other channels of the plugin get silence.

| bus | name | default layout |
|---|---|---|
| input 0 | Input | Stereo |
| output 0 | Output | Stereo |

| layout | accepted |
|---|---|
| mono | yes |
| stereo | yes |
| mono in, stereo out | no |
| LCR | no |
| quad | no |
| 5.1 | no |
| 7.1 | no |
| ambisonics 1st order | no |

- side chain: no; MIDI in: no, out: no; instrument: no
- coupling L to R: -60.0 dB re the driven channel (-78.3 dBFS), R to L: -60.0 dB re the driven channel (-78.3 dBFS): channels independent no

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 26 parameters. Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a width or mid/side control reacts only to this one). The scan started from the defaults.

| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |
|---|---|---|---|---|---|---|---|---|---|
| 0 | HPF Frequency | 20 | 20 | 500 | continuous | yes | -70.1 dB / -88.4 dBFS (channel 2) | -61.5 dB / -79.8 dBFS | defaults, the others at 0.75 |
| 1 | HPF Enabled | Off | Off | On | switch | yes | -21.0 dB / -46.0 dBFS | -20.7 dB / -45.6 dBFS (channel 2) | defaults |
| 2 | LPF Frequency | 3000 | 20000 | 20000 | continuous | yes | -66.5 dB / -84.9 dBFS | -65.8 dB / -84.1 dBFS (channel 2) | defaults, the others at 0.75 |
| 3 | LPF Enabled | Off | Off | On | switch | yes | -1.6 dB / -26.5 dBFS | -1.5 dB / -26.5 dBFS (channel 2) | defaults |
| 4 | LF Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -71.0 dB / -89.4 dBFS | -70.2 dB / -88.5 dBFS | defaults, the others at 0.75 |
| 5 | LF Frequency | 30 | 100 | 480 | continuous | yes | -67.1 dB / -85.4 dBFS | -62.2 dB / -80.5 dBFS (channel 2) | defaults, the others at 0.75 |
| 6 | LF Bell Mode | Off | Off | On | switch | yes | -63.3 dB / -81.7 dBFS | -64.8 dB / -83.1 dBFS | defaults, the others at 0.75 |
| 7 | LM Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -70.7 dB / -89.0 dBFS (channel 2) | -59.6 dB / -77.9 dBFS (channel 2) | defaults, the others at 0.75 |
| 8 | LM Frequency | 200 | 600 | 2500 | continuous | yes | -71.8 dB / -90.1 dBFS (channel 2) | -69.0 dB / -87.3 dBFS (channel 2) | defaults, the others at 0.75 |
| 9 | LM Q | 0.40 | 0.70 | 4.00 | continuous | yes | -62.9 dB / -81.3 dBFS | -62.2 dB / -80.5 dBFS | defaults, the others at 0.75 |
| 10 | HM Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -65.2 dB / -83.6 dBFS | -73.0 dB / -91.3 dBFS | defaults, the others at 0.75 |
| 11 | HM Frequency | 600 | 2000 | 7000 | continuous | yes | -69.7 dB / -88.1 dBFS (channel 2) | -66.1 dB / -84.4 dBFS | defaults, the others at 0.75 |
| 12 | HM Q | 0.40 | 0.70 | 4.00 | continuous | yes | -67.5 dB / -85.8 dBFS | -62.8 dB / -81.2 dBFS | defaults, the others at 0.75 |
| 13 | HF Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -72.8 dB / -91.1 dBFS | -63.6 dB / -81.9 dBFS (channel 2) | defaults, the others at 0.75 |
| 14 | HF Frequency | 1500 | 8000 | 16000 | continuous | yes | -64.1 dB / -82.4 dBFS | -64.5 dB / -82.8 dBFS (channel 2) | defaults, the others at 0.75 |
| 15 | HF Bell Mode | Off | Off | On | switch | yes | -65.6 dB / -83.9 dBFS (channel 2) | -69.3 dB / -87.6 dBFS (channel 2) | defaults, the others at 0.75 |
| 16 | EQ Type | Brown | Brown | Black | switch | yes | -43.8 dB / -68.7 dBFS | -43.8 dB / -68.8 dBFS | defaults |
| 17 | Bypass | Off | Off | On | switch | yes | 3.0 dB / -21.9 dBFS | 3.1 dB / -21.9 dBFS (channel 2) | defaults |
| 18 | Input Gain | -12.0 | 0.0 | 12.0 | continuous | yes | -0.0 dB / -25.0 dBFS | -0.0 dB / -25.0 dBFS | defaults |
| 19 | Output Gain | -12.0 | 0.0 | 12.0 | continuous | yes | -77.2 dB / -95.5 dBFS | -65.6 dB / -84.0 dBFS | defaults, the others at 0.75 |
| 20 | Saturation | 0 | 0 | 100 | continuous | yes | -63.3 dB / -81.7 dBFS | -67.3 dB / -85.6 dBFS (channel 2) | defaults, the others at 0.75 |
| 21 | Oversampling | 2x | 2x | 4x | switch | yes | -72.5 dB / -90.8 dBFS (channel 2) | -65.8 dB / -84.1 dBFS (channel 2) | defaults, the others at 0.75 |
| 22 | M/S Mode | Off | Off | On | switch | yes | -60.0 dB / -85.0 dBFS | -60.0 dB / -84.9 dBFS | defaults |
| 23 | Spectrum Pre/Post | Off | Off | On | switch | yes | -68.4 dB / -86.8 dBFS | -67.0 dB / -85.3 dBFS (channel 2) | defaults, the others at 0.75 |
| 24 | Auto Gain Compensation | Off | On | On | switch | yes | -19.1 dB / -37.4 dBFS (channel 2) | -19.1 dB / -37.4 dBFS (channel 2) | defaults, the others at 0.75 |
| 25 | Program | Default | Default | Master Bus Sweetening | 15 | yes | -9.4 dB / -34.3 dBFS | -9.4 dB / -34.3 dBFS | defaults |

## The settings A and B
A = the defaults. B = the parameters that change the audio at 0.75 of their range (switches left out, starting from the defaults); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters where B differs from A are listed.

| no. | name | A (normalised) | A | B (normalised) | B |
|---|---|---|---|---|---|
| 18 | Input Gain | 0.500 | 0.0 | 0.750 | 6.0 |
| 19 | Output Gain | 0.500 | 0.0 | 0.750 | 6.0 |
| 20 | Saturation | 0.000 | 0 | 0.750 | 75 |
| 25 | Program | 0.000 | Default | 0.750 | Bass Guitar Polish |

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
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 1.5 dB / -23.5 dBFS (channel 2) | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | no | FAILS | identical ; identical | 0.0 dB / -24.9 dBFS (channel 2) | identical ; -13.9 dB / -32.2 dBFS (channel 2) |
| new instance per render, parameters set after prepare | yes | yes | no | FAILS | identical ; identical | 1.5 dB / -23.5 dBFS (channel 2) | identical ; -79.2 dB / -97.5 dBFS |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 1.5 dB / -23.5 dBFS (channel 2) | identical ; -85.2 dB / -103.5 dBFS (channel 2) |

Most careful way that works: new instance per render, after prepare every parameter first set to another value, then the target.

## Block sizes
1.00 s of noise through the plugin (setting B) per block size, against block size 512. Steady state: the last 0.10 s (decides; below -100 dB = independent). Whole: the full render (a plugin that smooths its parameters per block differs here, but not in the steady state).

| block size | steady state | whole |
|---|---|---|
| 32 | -66.5 dB / -84.7 dBFS | -66.5 dB / -84.7 dBFS |
| 64 | -75.6 dB / -93.8 dBFS | -75.6 dB / -93.9 dBFS |
| 128 | -73.4 dB / -91.6 dBFS | -73.4 dB / -91.7 dBFS (channel 2) |
| 256 | -66.7 dB / -84.9 dBFS | -66.7 dB / -84.9 dBFS |
| 1024 | -70.4 dB / -88.6 dBFS | -70.3 dB / -88.6 dBFS |
| 2048 | -76.2 dB / -94.4 dBFS | -75.9 dB / -94.2 dBFS (channel 2) |
| 509 | -72.9 dB / -91.1 dBFS | -72.8 dB / -91.1 dBFS (channel 2) |

## Other
- time-invariant (one instance: settled for 2.00 s, noise, 0.50 s silence, the same noise again; the two outputs the same): yes (-85.5 dB / -103.8 dBFS (channel 2))
- settles within 0.25 s after a parameter change (the output then against the output after 2.00 s): yes (-80.3 dB / -98.6 dBFS)
- deterministic (two instances, the same noise, bit exact): no
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters and switches/choices in separate runs): continuous no, switches and choices no
- digital silence in gives digital silence out: no (peak -96.2 dBFS)

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

