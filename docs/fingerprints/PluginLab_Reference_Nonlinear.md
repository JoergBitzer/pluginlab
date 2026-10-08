# Fingerprint: PluginLab Reference Nonlinear

- file: `/home/bitzer/AudioDev/pluginlab/build/reference_plugins/PluginLabReferenceNonlinear.vst3`
- format: VST3, manufacturer: Jade_Hochschule, version: 0.22.0
- measured: 2026-10-08 17:53
- channels: mono yes, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| main-bus layouts accepted | mono, stereo | measured with 2 channel(s) |
| side-chain input (more than one input bus) | no |  |
| MIDI | none |  |
| channels independent (one input driven, the other silent) | no | L to R: -31.7 dB re the driven channel (-34.2 dBFS), R to L: -29.8 dB re the driven channel (-32.2 dBFS) |
| parameters / changing the audio | 13 / 11 |  |
| latency at 48 kHz, reported / measured (samples) | 0 / 0 | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| reported latency = measured at all rates | yes | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| output before the peak of the impulse response | no |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | 4 of 4 ways | time-varying: the repeatability of A cannot be judged |
| time-invariant (the same noise twice through one instance) | no (time-varying) | -28.5 dB / -30.9 dBFS (channel 2) |
| settles within 0.25 s after a parameter change | no | -30.2 dB / -32.6 dBFS |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes | expected for a time-varying plugin |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | no | continuous parameters: no, switches and choices: no; expected for a time-varying plugin |
| digital silence in gives digital silence out | no | peak -22.8 dBFS; expected for a time-varying plugin |
| the same when the host renders offline (offline flag) | yes | identical |
| the same at real-time pace (message loop running) | yes | identical |
| a parameter change reaches the audio, fast and at real-time pace | alike | fast: not within the render, real-time pace: not within the render |

Bold (orange on the Developer page): worth a look (see the findings and the details below).

How to read the differences: every difference is given as relative / absolute: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- The plugin is time-varying: the same noise twice through one instance, with 0.50 s of silence between, gives different output (-28.5 dB / -30.9 dBFS (channel 2)): an LFO, a random element, dither, a noise generator or a slow envelope. Repeatability, determinism, recovery and silence cannot be judged as for a time-invariant plugin.
- After jumps of the continuous parameters and the switches and choices to both ends the output does not come back to what it was (expected for a time-varying plugin)
- Digital silence in does not give digital silence out (peak -22.8 dBFS) (expected for a time-varying plugin)

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
- coupling L to R: -31.7 dB re the driven channel (-34.2 dBFS), R to L: -29.8 dB re the driven channel (-32.2 dBFS): channels independent no

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 13 parameters. Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a width or mid/side control reacts only to this one). The scan started from the defaults.

| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Curve | Off | Off | Soft clip (tanh) | 4 | yes | -42.2 dB / -67.0 dBFS | -42.2 dB / -67.0 dBFS | defaults |
| 1 | Drive | -24.0 dB | 0.0 dB | 24.0 dB | continuous | yes | 9.5 dB / -15.3 dBFS | 9.5 dB / -15.3 dBFS (channel 2) | defaults |
| 2 | a2 | -1.000 | 0.100 | 1.000 | continuous | yes | no | no |  |
| 3 | a3 | -1.000 | 0.050 | 1.000 | continuous | yes | no | no |  |
| 4 | Threshold | -40.0 dBFS | -6.0 dBFS | 0.0 dBFS | continuous | yes | -1.4 dB / -3.8 dBFS (channel 2) | -1.4 dB / -3.7 dBFS (channel 2) | defaults, the others at 0.75 |
| 5 | Output | -24.0 dB | 0.0 dB | 24.0 dB | continuous | yes | 9.5 dB / -15.3 dBFS | 9.5 dB / -15.3 dBFS (channel 2) | defaults |
| 6 | Quantizer | Off | Off | On, TPDF dither | 3 | yes | -71.3 dB / -96.1 dBFS (channel 2) | -71.4 dB / -96.2 dBFS | defaults |
| 7 | Bits | 2 | 16 | 24 | continuous | yes | -44.4 dB / -46.8 dBFS (channel 2) | -44.4 dB / -46.8 dBFS (channel 2) | defaults, the others at 0.75 |
| 8 | Noise | Off | Off | Pink | 3 | yes | -33.9 dB / -58.7 dBFS | -33.9 dB / -58.7 dBFS | defaults |
| 9 | Noise level | -140.0 dBFS | -60.0 dBFS | 0.0 dBFS | continuous | yes | -33.8 dB / -36.2 dBFS | -33.8 dB / -36.2 dBFS | defaults, the others at 0.75 |
| 10 | Hum | Off | Off | 60 Hz | 3 | yes | -38.1 dB / -62.9 dBFS | -38.1 dB / -62.9 dBFS | defaults |
| 11 | Hum level | -140.0 dBFS | -60.0 dBFS | 0.0 dBFS | continuous | yes | -38.0 dB / -40.5 dBFS (channel 2) | -38.0 dB / -40.5 dBFS | defaults, the others at 0.75 |
| 12 | Bypass | Off | Off | On | switch | yes | -0.7 dB / -3.1 dBFS | -0.7 dB / -3.0 dBFS (channel 2) | defaults, the others at 0.75 |

## The settings A and B
A = the defaults. B = the parameters that change the audio at 0.75 of their range (switches left out, starting from the defaults); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters where B differs from A are listed.

| no. | name | A (normalised) | A | B (normalised) | B |
|---|---|---|---|---|---|
| 1 | Drive | 0.500 | 0.0 dB | 0.750 | 12.0 dB |
| 4 | Threshold | 0.850 | -6.0 dBFS | 0.750 | -10.0 dBFS |
| 5 | Output | 0.500 | 0.0 dB | 0.750 | 12.0 dB |
| 6 | Quantizer | 0.000 | Off | 0.750 | On, TPDF dither |
| 7 | Bits | 0.636 | 16 | 0.750 | 19 |
| 8 | Noise | 0.000 | Off | 0.750 | Pink |
| 9 | Noise level | 0.571 | -60.0 dBFS | 0.750 | -35.0 dBFS |
| 10 | Hum | 0.000 | Off | 0.750 | 60 Hz |
| 11 | Hum level | 0.571 | -60.0 dBFS | 0.750 | -35.0 dBFS |

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
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 21.7 dB / -3.0 dBFS (channel 2) | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 21.7 dB / -3.0 dBFS (channel 2) | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 21.7 dB / -3.0 dBFS (channel 2) | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 21.7 dB / -3.0 dBFS (channel 2) | identical ; identical |

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
- time-invariant (one instance: settled for 2.00 s, noise, 0.50 s silence, the same noise again; the two outputs the same): no (-28.5 dB / -30.9 dBFS (channel 2))
- settles within 0.25 s after a parameter change (the output then against the output after 2.00 s): no (-30.2 dB / -32.6 dBFS)
- deterministic (two instances, the same noise, bit exact): yes
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters and switches/choices in separate runs): continuous no, switches and choices no
- digital silence in gives digital silence out: no (peak -22.8 dBFS)

## Real-time behaviour
All other measurements render as fast as possible, in real-time mode (offline flag off) and without letting the message thread run. Here the setting B with 2.0 s of noise is rendered again (fresh instances, as above): with the offline flag, and at real-time pace (after every block the message loop runs until the wall clock has caught up with the audio, so that timers and asynchronous updates of the plugin run as in a DAW). Different = more than 10 dB above the difference of two fast renders (identical) and above -80 dB.

- offline flag on against off: identical - the same: yes
- real-time pace against fast: identical - the same: yes
- a parameter change in the middle of the noise (all parameters A -> B), until every block equals the output of B rendered at real-time pace (below -60 dB): fast: not within the render, real-time pace: not within the render

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
  "maximumJumpedParameters": 16,
  "realTimeTests": true,
  "realTimeSeconds": 2.0,
  "longRealTimeSeconds": 0.0
}
```

