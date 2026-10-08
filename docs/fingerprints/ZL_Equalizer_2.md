# Fingerprint: ZL Equalizer 2

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ZL.Equalizer.2-1.4.1-Linux-x86-64/VST3/ZL Equalizer 2.vst3`
- format: VST3, manufacturer: ZL, version: 1.4.1
- measured: 2026-10-08 17:54
- channels: mono yes, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| main-bus layouts accepted | mono, stereo | measured with 2 channel(s) |
| side-chain input (more than one input bus) | yes | Aux (Stereo) |
| MIDI | none |  |
| channels independent (one input driven, the other silent) | yes | L to R: silent, R to L: silent |
| parameters / changing the audio | 610 / 5 |  |
| latency at 48 kHz, reported / measured (samples) | 0 / 0 | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| reported latency = measured at all rates | yes | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| output before the peak of the impulse response | no |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | none | time-varying: the repeatability of A cannot be judged |
| time-invariant (the same noise twice through one instance) | no (time-varying) | -23.3 dB / -33.7 dBFS |
| settles within 0.25 s after a parameter change | no | -3.6 dB / -13.9 dBFS |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes | expected for a time-varying plugin |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | no | continuous parameters: no, switches and choices: no; expected for a time-varying plugin |
| digital silence in gives digital silence out | yes |  |
| the same when the host renders offline (offline flag) | yes | identical |
| the same at real-time pace (message loop running) | yes | identical |
| a parameter change reaches the audio, fast and at real-time pace | alike | fast: not within the render, real-time pace: not within the render |

Bold (orange on the Developer page): worth a look (see the findings and the details below).

How to read the differences: every difference is given as relative / absolute: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- The plugin is time-varying: the same noise twice through one instance, with 0.50 s of silence between, gives different output (-23.3 dB / -33.7 dBFS): an LFO, a random element, dither, a noise generator or a slow envelope. Repeatability, determinism, recovery and silence cannot be judged as for a time-invariant plugin.
- Delivery 'one instance, parameters set after prepare (stream)' fails: the same settings gave other output (A again: -14.1 dB / -38.9 dBFS (channel 2)). the settings that were delivered first differ from the same settings reached by a change (A: -15.5 dB / -38.9 dBFS (channel 2), B: identical): the first delivery was lost.  (expected for a time-varying plugin)
- Delivery 'new instance per render, parameters set before prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -15.5 dB / -38.9 dBFS (channel 2), B: identical): the first delivery was lost.  (expected for a time-varying plugin)
- Delivery 'new instance per render, parameters set after prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -15.5 dB / -38.9 dBFS (channel 2), B: identical): the first delivery was lost.  (expected for a time-varying plugin)
- Delivery 'new instance per render, after prepare every parameter first set to another value, then the target' fails: the settings that were delivered first differ from the same settings reached by a change (A: -15.5 dB / -38.9 dBFS (channel 2), B: -38.5 dB / -58.2 dBFS): the first delivery was lost.  (expected for a time-varying plugin)
- After jumps of the continuous parameters and the switches and choices to both ends the output does not come back to what it was (expected for a time-varying plugin)

## Channels and buses
The buses of the plugin as it is created, the main-bus layouts it accepts (other buses switched off where possible), MIDI, and the coupling of the channels at the setting B: one input channel gets noise, the other silence; the output of the silent channel relative to the output of the driven one (below -100 dB = independent channels). The measurement runs with 2 channel(s); other channels of the plugin get silence.

| bus | name | default layout |
|---|---|---|
| input 0 | Input | Stereo |
| input 1 | Aux | Stereo |
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

- side chain: yes; MIDI in: no, out: no; instrument: no
- coupling L to R: silent, R to L: silent: channels independent yes

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 610 parameters, the first 64 examined. Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a width or mid/side control reacts only to this one). The scan started from the defaults.

| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Filter Structure | Minimum Phase | Minimum Phase | Zero Phase | 6 | yes | 3.0 dB / -21.8 dBFS | 3.0 dB / -21.8 dBFS | defaults |
| 1 | External Side | Off | Off | On | switch | yes | no | no |  |
| 2 | Bypass | On | On | Bypass | switch | yes | -7.1 dB / -26.9 dBFS | -7.1 dB / -26.9 dBFS (channel 2) | defaults, the others at 0.75 |
| 3 | Output Gain | -30.00 | 0.00 | 30.00 | continuous | yes | -1.7 dB / -26.5 dBFS | -1.7 dB / -26.5 dBFS | defaults |
| 4 | Gain Scale | -100.0 | 100.0 | 200.0 | continuous | yes | no | no |  |
| 5 | Auto Gain | Off | Off | On | switch | yes | no | no |  |
| 6 | Static Gain | Off | Off | On | switch | yes | no | no |  |
| 7 | Phase Flip | Off | Off | On | switch | yes | 6.0 dB / -18.8 dBFS | 6.0 dB / -18.8 dBFS | defaults |
| 8 | Lookahead | 0.00 | 0.00 | 20.00 | continuous | yes | 3.1 dB / -21.7 dBFS | 3.1 dB / -21.7 dBFS | defaults |
| 9 | Filter Status0 | Off | Off | On | 3 | yes | no | no |  |
| 10 | Filter Type0 | Peak | Peak | Flat Gain | 11 | yes | no | no |  |
| 11 | Order0 | 6 dB/oct | 12 dB/oct | 96 dB/oct | 7 | yes | no | no |  |
| 12 | LRMode0 | Stereo | Stereo | Side | 5 | yes | no | no |  |
| 13 | Freq0 | 10.0 | 1000.0 | 160000.0 | continuous | yes | no | no |  |
| 14 | Gain0 | -30.00 | 0.00 | 30.00 | continuous | yes | no | no |  |
| 15 | Target Gain0 | -30.00 | 0.00 | 30.00 | continuous | yes | no | no |  |
| 16 | Q0 | 0.025 | 0.707 | 25.000 | continuous | yes | no | no |  |
| 17 | Dynamic ON0 | Off | Off | On | switch | yes | no | no |  |
| 18 | Dynamic Learn0 | Off | Off | On | switch | yes | no | no |  |
| 19 | Dynamic Bypass0 | Off | Off | On | switch | yes | no | no |  |
| 20 | Dynamic Relative0 | Off | Off | On | switch | yes | no | no |  |
| 21 | Side Swap0 | Off | Off | On | switch | yes | no | no |  |
| 22 | Side Link0 | Off | Off | On | switch | yes | no | no |  |
| 23 | Threshold (dB)0 | -80.0 | -40.0 | 0.0 | continuous | yes | no | no |  |
| 24 | Knee Width0 | 0.00 | 8.00 | 32.00 | continuous | yes | no | no |  |
| 25 | Attack0 | 0.00 | 100.00 | 1000.00 | continuous | yes | no | no |  |
| 26 | Release0 | 0.00 | 500.00 | 5000.00 | continuous | yes | no | no |  |
| 27 | Dynamic RMS Length0 | 0.0 | 0.0 | 40.0 | continuous | yes | no | no |  |
| 28 | Dynamic RMS Mix0 | 0.0 | 50.0 | 100.0 | continuous | yes | no | no |  |
| 29 | Dynamic Smooth0 | 0.0 | 100.0 | 100.0 | continuous | yes | no | no |  |
| 30 | Side Filter Type0 | BP | BP | AL | 4 | yes | no | no |  |
| 31 | Side Order0 | 6 dB/oct | 12 dB/oct | 96 dB/oct | 7 | yes | no | no |  |
| 32 | Side Freq0 | 10.0 | 1000.0 | 160000.0 | continuous | yes | no | no |  |
| 33 | Side Q0 | 0.025 | 0.707 | 25.000 | continuous | yes | no | no |  |
| 34 | Filter Status1 | Off | Off | On | 3 | yes | no | no |  |
| 35 | Filter Type1 | Peak | Peak | Flat Gain | 11 | yes | no | no |  |
| 36 | Order1 | 6 dB/oct | 12 dB/oct | 96 dB/oct | 7 | yes | no | no |  |
| 37 | LRMode1 | Stereo | Stereo | Side | 5 | yes | no | no |  |
| 38 | Freq1 | 10.0 | 1000.0 | 160000.0 | continuous | yes | no | no |  |
| 39 | Gain1 | -30.00 | 0.00 | 30.00 | continuous | yes | no | no |  |
| 40 | Target Gain1 | -30.00 | 0.00 | 30.00 | continuous | yes | no | no |  |
| 41 | Q1 | 0.025 | 0.707 | 25.000 | continuous | yes | no | no |  |
| 42 | Dynamic ON1 | Off | Off | On | switch | yes | no | no |  |
| 43 | Dynamic Learn1 | Off | Off | On | switch | yes | no | no |  |
| 44 | Dynamic Bypass1 | Off | Off | On | switch | yes | no | no |  |
| 45 | Dynamic Relative1 | Off | Off | On | switch | yes | no | no |  |
| 46 | Side Swap1 | Off | Off | On | switch | yes | no | no |  |
| 47 | Side Link1 | Off | Off | On | switch | yes | no | no |  |
| 48 | Threshold (dB)1 | -80.0 | -40.0 | 0.0 | continuous | yes | no | no |  |
| 49 | Knee Width1 | 0.00 | 8.00 | 32.00 | continuous | yes | no | no |  |
| 50 | Attack1 | 0.00 | 100.00 | 1000.00 | continuous | yes | no | no |  |
| 51 | Release1 | 0.00 | 500.00 | 5000.00 | continuous | yes | no | no |  |
| 52 | Dynamic RMS Length1 | 0.0 | 0.0 | 40.0 | continuous | yes | no | no |  |
| 53 | Dynamic RMS Mix1 | 0.0 | 50.0 | 100.0 | continuous | yes | no | no |  |
| 54 | Dynamic Smooth1 | 0.0 | 100.0 | 100.0 | continuous | yes | no | no |  |
| 55 | Side Filter Type1 | BP | BP | AL | 4 | yes | no | no |  |
| 56 | Side Order1 | 6 dB/oct | 12 dB/oct | 96 dB/oct | 7 | yes | no | no |  |
| 57 | Side Freq1 | 10.0 | 1000.0 | 160000.0 | continuous | yes | no | no |  |
| 58 | Side Q1 | 0.025 | 0.707 | 25.000 | continuous | yes | no | no |  |
| 59 | Filter Status2 | Off | Off | On | 3 | yes | no | no |  |
| 60 | Filter Type2 | Peak | Peak | Flat Gain | 11 | yes | no | no |  |
| 61 | Order2 | 6 dB/oct | 12 dB/oct | 96 dB/oct | 7 | yes | no | no |  |
| 62 | LRMode2 | Stereo | Stereo | Side | 5 | yes | no | no |  |
| 63 | Freq2 | 10.0 | 1000.0 | 160000.0 | continuous | yes | no | no |  |

## The settings A and B
A = the defaults. B = the parameters that change the audio at 0.75 of their range (switches left out, starting from the defaults); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters where B differs from A are listed.

| no. | name | A (normalised) | A | B (normalised) | B |
|---|---|---|---|---|---|
| 0 | Filter Structure | 0.000 | Minimum Phase | 0.750 | Mixed Phase |
| 3 | Output Gain | 0.500 | 0.00 | 0.750 | 15.00 |
| 8 | Lookahead | 0.000 | 0.00 | 0.750 | 10.41 |

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
| one instance, parameters set after prepare (stream) | no | yes | no | FAILS | identical ; -14.1 dB / -38.9 dBFS (channel 2) | 6.3 dB / -18.5 dBFS (channel 2) | -15.5 dB / -38.9 dBFS (channel 2) ; identical |
| new instance per render, parameters set before prepare | yes | yes | no | FAILS | identical ; identical | 6.3 dB / -18.5 dBFS (channel 2) | -15.5 dB / -38.9 dBFS (channel 2) ; identical |
| new instance per render, parameters set after prepare | yes | yes | no | FAILS | identical ; identical | 6.3 dB / -18.5 dBFS (channel 2) | -15.5 dB / -38.9 dBFS (channel 2) ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | no | FAILS | identical ; identical | 6.2 dB / -18.6 dBFS (channel 2) | -15.5 dB / -38.9 dBFS (channel 2) ; -38.5 dB / -58.2 dBFS |

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
- time-invariant (one instance: settled for 2.00 s, noise, 0.50 s silence, the same noise again; the two outputs the same): no (-23.3 dB / -33.7 dBFS)
- settles within 0.25 s after a parameter change (the output then against the output after 2.00 s): no (-3.6 dB / -13.9 dBFS)
- deterministic (two instances, the same noise, bit exact): yes
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters and switches/choices in separate runs): continuous no, switches and choices no
- digital silence in gives digital silence out: yes

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

