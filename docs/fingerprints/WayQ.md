# Fingerprint: WayQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/WayQ-Linux-v1.0.0/linux/WayQ.vst3`
- format: VST3, manufacturer: Way.Net, version: 1.0.0
- measured: 2026-10-08 17:53
- channels: mono no, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| main-bus layouts accepted | stereo | measured with 2 channel(s) |
| side-chain input (more than one input bus) | no |  |
| MIDI | none |  |
| channels independent (one input driven, the other silent) | yes | L to R: silent, R to L: -186.0 dB re the driven channel (-192.8 dBFS) |
| parameters / changing the audio | 29 / 21 |  |
| latency at 48 kHz, reported / measured (samples) | **0 / 0** | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 1 |
| reported latency = measured at all rates | **no** | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 1 |
| output before the peak of the impulse response | yes |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | **3 of 4 ways** | new instance per render, after prepare every parameter first set to another value, then the target |
| time-invariant (the same noise twice through one instance) | yes | identical |
| settles within 0.25 s after a parameter change | yes | -165.3 dB / -172.0 dBFS |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | yes | continuous parameters: yes, switches and choices: yes |
| digital silence in gives digital silence out | yes |  |
| the same when the host renders offline (offline flag) | yes | identical |
| the same at real-time pace (message loop running) | yes | identical |
| a parameter change reaches the audio, fast and at real-time pace | alike | fast: 21.3333 ms, real-time pace: 21.3333 ms |

Bold (orange on the Developer page): worth a look (see the findings and the details below).

How to read the differences: every difference is given as relative / absolute: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- Latency at 96000 Hz: the plugin reports 0 samples, measured 1 samples
- Delivery 'one instance, parameters set after prepare (stream)' fails: the same settings gave other output (A again: -88.1 dB / -113.7 dBFS (channel 2)). 

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
- coupling L to R: silent, R to L: -186.0 dB re the driven channel (-192.8 dBFS): channels independent yes

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 29 parameters. Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a width or mid/side control reacts only to this one). The scan started from the defaults.

| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |
|---|---|---|---|---|---|---|---|---|---|
| 0 | HP Freq A | 20.0000000 | 20.0000000 | 20000.0000000 | continuous | yes | -2.2 dB / -27.9 dBFS | -2.2 dB / -27.9 dBFS | defaults |
| 1 | HP Q A | 0.3000000 | 0.7070000 | 10.0000000 | continuous | yes | -23.6 dB / -49.3 dBFS | -23.6 dB / -49.3 dBFS | defaults |
| 2 | Mid Freq A | 20.0000000 | 632.4559937 | 20000.0000000 | continuous | yes | -14.9 dB / -21.5 dBFS | -14.9 dB / -21.5 dBFS | defaults, the others at 0.75 |
| 3 | Mid Gain A | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | -13.0 dB / -38.7 dBFS | -13.0 dB / -38.7 dBFS | defaults |
| 4 | Mid Q A | 0.3000000 | 0.7000000 | 17.3099995 | continuous | yes | -32.7 dB / -39.4 dBFS | -32.7 dB / -39.4 dBFS | defaults, the others at 0.75 |
| 5 | LP Freq A | 20.0000000 | 20000.0000000 | 20000.0000000 | continuous | yes | 0.1 dB / -25.6 dBFS | 0.1 dB / -25.6 dBFS | defaults |
| 6 | LP Q A | 0.3000000 | 0.7070000 | 10.0000000 | continuous | yes | 2.8 dB / -22.9 dBFS | 2.8 dB / -22.9 dBFS | defaults |
| 7 | Delta A | 0 | 0 | 2 | continuous | yes | 6.0 dB / -19.7 dBFS | 6.0 dB / -19.7 dBFS | defaults |
| 8 | Bass Tilt Freq A | 20.0000000 | 249.9999542 | 20000.0000000 | continuous | yes | no | no |  |
| 9 | Bass Tilt Gain A | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no | no |  |
| 10 | Treb Tilt Freq A | 20.0000000 | 4000.0000000 | 20000.0000000 | continuous | yes | no | no |  |
| 11 | Treb Tilt Gain A | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no | no |  |
| 12 | EQ Mode X A | Off | Off | On | switch | yes | -2.6 dB / -28.3 dBFS | -2.6 dB / -28.3 dBFS | defaults |
| 13 | HP Freq B | 20.0000000 | 20.0000000 | 20000.0000000 | continuous | yes | -2.2 dB / -27.9 dBFS (channel 2) | -2.3 dB / -27.9 dBFS (channel 2) | defaults |
| 14 | HP Q B | 0.3000000 | 0.7070000 | 10.0000000 | continuous | yes | -21.5 dB / -47.2 dBFS (channel 2) | -17.3 dB / -42.9 dBFS (channel 2) | defaults |
| 15 | Mid Freq B | 20.0000000 | 632.4559937 | 20000.0000000 | continuous | yes | -14.9 dB / -21.5 dBFS (channel 2) | -15.1 dB / -21.7 dBFS (channel 2) | defaults, the others at 0.75 |
| 16 | Mid Gain B | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | -13.0 dB / -38.7 dBFS (channel 2) | -13.0 dB / -38.6 dBFS (channel 2) | defaults |
| 17 | Mid Q B | 0.3000000 | 0.7000000 | 17.3099995 | continuous | yes | -32.7 dB / -39.4 dBFS (channel 2) | -32.9 dB / -39.5 dBFS (channel 2) | defaults, the others at 0.75 |
| 18 | LP Freq B | 20.0000000 | 20000.0000000 | 20000.0000000 | continuous | yes | 0.1 dB / -25.6 dBFS (channel 2) | 0.1 dB / -25.5 dBFS (channel 2) | defaults |
| 19 | LP Q B | 0.3000000 | 0.7070000 | 10.0000000 | continuous | yes | 2.8 dB / -22.9 dBFS (channel 2) | 2.7 dB / -23.0 dBFS (channel 2) | defaults |
| 20 | Delta B | 0 | 0 | 2 | continuous | yes | 6.0 dB / -19.7 dBFS (channel 2) | 6.0 dB / -19.6 dBFS (channel 2) | defaults |
| 21 | Bass Tilt Freq B | 20.0000000 | 249.9999542 | 20000.0000000 | continuous | yes | no | no |  |
| 22 | Bass Tilt Gain B | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no | no |  |
| 23 | Treb Tilt Freq B | 20.0000000 | 4000.0000000 | 20000.0000000 | continuous | yes | no | no |  |
| 24 | Treb Tilt Gain B | -12.0000000 | 0.0000000 | 12.0000000 | continuous | yes | no | no |  |
| 25 | EQ Mode X B | Off | Off | On | switch | yes | -2.6 dB / -28.3 dBFS (channel 2) | -2.7 dB / -28.3 dBFS (channel 2) | defaults |
| 26 | Bypass | Off | Off | On | switch | yes | -2.6 dB / -28.3 dBFS (channel 2) | -2.6 dB / -28.3 dBFS | defaults |
| 27 | M/S Mode | Off | Off | On | switch | yes | 0.0 dB / -25.7 dBFS (channel 2) | -3.0 dB / -28.7 dBFS | defaults |
| 28 | Mix | 0.0000000 | 1.0000000 | 1.0000000 | continuous | yes | -1.9 dB / -27.6 dBFS | -1.9 dB / -27.6 dBFS | defaults |

## The settings A and B
A = the defaults. B = the parameters that change the audio at 0.75 of their range (switches left out, starting from the defaults); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters where B differs from A are listed.

| no. | name | A (normalised) | A | B (normalised) | B |
|---|---|---|---|---|---|
| 0 | HP Freq A | 0.000 | 20.0000000 | 0.750 | 4723.5708008 |
| 1 | HP Q A | 0.042 | 0.7070000 | 0.750 | 7.5749998 |
| 2 | Mid Freq A | 0.500 | 632.4559937 | 0.750 | 4723.5708008 |
| 3 | Mid Gain A | 0.500 | 0.0000000 | 0.750 | 6.0000000 |
| 4 | Mid Q A | 0.024 | 0.7000000 | 0.750 | 13.0574999 |
| 5 | LP Freq A | 1.000 | 20000.0000000 | 0.750 | 4723.5708008 |
| 6 | LP Q A | 0.042 | 0.7070000 | 0.750 | 7.5749998 |
| 7 | Delta A | 0.000 | 0 | 0.750 | 2 |
| 13 | HP Freq B | 0.000 | 20.0000000 | 0.750 | 4723.5708008 |
| 14 | HP Q B | 0.042 | 0.7070000 | 0.750 | 7.5749998 |
| 15 | Mid Freq B | 0.500 | 632.4559937 | 0.750 | 4723.5708008 |
| 16 | Mid Gain B | 0.500 | 0.0000000 | 0.750 | 6.0000000 |
| 17 | Mid Q B | 0.024 | 0.7000000 | 0.750 | 13.0574999 |
| 18 | LP Freq B | 1.000 | 20000.0000000 | 0.750 | 4723.5708008 |
| 19 | LP Q B | 0.042 | 0.7070000 | 0.750 | 7.5749998 |
| 20 | Delta B | 0.000 | 0 | 0.750 | 2 |
| 28 | Mix | 1.000 | 1.0000000 | 0.750 | 0.7500000 |

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
| one instance, parameters set after prepare (stream) | no | yes | yes | FAILS | -76.4 dB / -102.1 dBFS (channel 2) ; -88.1 dB / -113.7 dBFS (channel 2) | 19.0 dB / -6.7 dBFS | -88.1 dB / -113.7 dBFS (channel 2) ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 19.0 dB / -6.7 dBFS | -88.1 dB / -113.7 dBFS (channel 2) ; -168.6 dB / -175.3 dBFS |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 19.0 dB / -6.7 dBFS | -88.1 dB / -113.7 dBFS (channel 2) ; -168.6 dB / -175.3 dBFS |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 19.0 dB / -6.7 dBFS | -88.1 dB / -113.7 dBFS (channel 2) ; -163.6 dB / -170.4 dBFS |

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
| 509 | identical | -184.7 dB / -191.2 dBFS (channel 2) |

## Other
- time-invariant (one instance: settled for 2.00 s, noise, 0.50 s silence, the same noise again; the two outputs the same): yes (identical)
- settles within 0.25 s after a parameter change (the output then against the output after 2.00 s): yes (-165.3 dB / -172.0 dBFS)
- deterministic (two instances, the same noise, bit exact): yes
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters and switches/choices in separate runs): continuous yes, switches and choices yes
- digital silence in gives digital silence out: yes

## Real-time behaviour
All other measurements render as fast as possible, in real-time mode (offline flag off) and without letting the message thread run. Here the setting B with 2.0 s of noise is rendered again (fresh instances, as above): with the offline flag, and at real-time pace (after every block the message loop runs until the wall clock has caught up with the audio, so that timers and asynchronous updates of the plugin run as in a DAW). Different = more than 10 dB above the difference of two fast renders (identical) and above -80 dB.

- offline flag on against off: identical - the same: yes
- real-time pace against fast: identical - the same: yes
- a parameter change in the middle of the noise (all parameters A -> B), until every block equals the output of B rendered at real-time pace (below -60 dB): fast: 21.3333 ms, real-time pace: 21.3333 ms

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

