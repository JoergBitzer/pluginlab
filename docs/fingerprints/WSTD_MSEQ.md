# Fingerprint: WSTD MSEQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/wstd/wstd-mseq-v1.0.1/WSTD_MSEQ.vst3`
- format: VST3, manufacturer: Wasted Audio, version: 1.0.1
- measured: 2026-10-08 17:53
- channels: mono no, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| main-bus layouts accepted | stereo | measured with 2 channel(s) |
| side-chain input (more than one input bus) | no |  |
| MIDI | none |  |
| channels independent (one input driven, the other silent) | yes | L to R: silent, R to L: silent |
| parameters / changing the audio | 10 / 8 |  |
| latency at 48 kHz, reported / measured (samples) | **0 / 1** | 44.1 kHz: 0 / 1, 48.0 kHz: 0 / 1, 96.0 kHz: 0 / 1 |
| reported latency = measured at all rates | **no** | 44.1 kHz: 0 / 1, 48.0 kHz: 0 / 1, 96.0 kHz: 0 / 1 |
| output before the peak of the impulse response | no |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | 4 of 4 ways | new instance per render, after prepare every parameter first set to another value, then the target |
| time-invariant (the same noise twice through one instance) | yes | identical |
| settles within 0.25 s after a parameter change | yes | identical |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | yes | continuous parameters: yes |
| digital silence in gives digital silence out | yes |  |
| the same when the host renders offline (offline flag) | yes | identical |
| the same at real-time pace (message loop running) | yes | identical |
| a parameter change reaches the audio, fast and at real-time pace | alike | fast: 21.3333 ms, real-time pace: 21.3333 ms |

Bold (orange on the Developer page): worth a look (see the findings and the details below).

How to read the differences: every difference is given as relative / absolute: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- Latency at 44100 Hz: the plugin reports 0 samples, measured 1 samples
- Latency at 48000 Hz: the plugin reports 0 samples, measured 1 samples
- Latency at 96000 Hz: the plugin reports 0 samples, measured 1 samples

## Channels and buses
The buses of the plugin as it is created, the main-bus layouts it accepts (other buses switched off where possible), MIDI, and the coupling of the channels at the setting B: one input channel gets noise, the other silence; the output of the silent channel relative to the output of the driven one (below -100 dB = independent channels). The measurement runs with 2 channel(s); other channels of the plugin get silence.

| bus | name | default layout |
|---|---|---|
| input 0 | Audio Input | Stereo |
| output 0 | Audio Output | Stereo |

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
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 10 parameters. Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a width or mid/side control reacts only to this one). The scan started from the defaults.

| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Buffer Size | 0 | 0 | 32768 | 32768 | no | no | no |  |
| 1 | Sample Rate | 0 | 0 | 384000 | continuous | no | no | no |  |
| 2 | m High | -inf | 0.000000 | 15.000000 | continuous | yes | 1.7 dB / -23.1 dBFS | -1.3 dB / -26.1 dBFS | defaults |
| 3 | m Low | -inf | 0.000000 | 15.000000 | continuous | yes | -12.8 dB / -37.7 dBFS | -16.1 dB / -41.0 dBFS | defaults |
| 4 | m Mid | -inf | 0.000000 | 15.000000 | continuous | yes | -3.9 dB / -28.7 dBFS | -7.0 dB / -31.8 dBFS | defaults |
| 5 | m Mid Freq | 313.299988 | 1337.000000 | 5705.600098 | continuous | yes | -7.2 dB / -32.1 dBFS | -10.3 dB / -35.1 dBFS | defaults |
| 6 | s High | -inf | 0.000000 | 15.000000 | continuous | yes | no | -1.2 dB / -26.0 dBFS | defaults |
| 7 | s Low | -inf | 0.000000 | 15.000000 | continuous | yes | no | -15.5 dB / -40.4 dBFS | defaults |
| 8 | s Mid | -inf | 0.000000 | 15.000000 | continuous | yes | no | -6.8 dB / -31.7 dBFS | defaults |
| 9 | s Mid Freq | 313.299988 | 1337.000000 | 5705.600098 | continuous | yes | no | -10.2 dB / -35.0 dBFS | defaults |

## The settings A and B
A = the defaults. B = the parameters that change the audio at 0.75 of their range (switches left out, starting from the defaults); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters where B differs from A are listed.

| no. | name | A (normalised) | A | B (normalised) | B |
|---|---|---|---|---|---|
| 2 | m High | 0.500 | 0.000000 | 0.750 | 7.500000 |
| 3 | m Low | 0.500 | 0.000000 | 0.750 | 7.500000 |
| 4 | m Mid | 0.500 | 0.000000 | 0.750 | 7.500000 |
| 5 | m Mid Freq | 0.190 | 1337.000000 | 0.750 | 4357.525391 |
| 6 | s High | 0.500 | 0.000000 | 0.750 | 7.500000 |
| 7 | s Low | 0.500 | 0.000000 | 0.750 | 7.500000 |
| 8 | s Mid | 0.500 | 0.000000 | 0.750 | 7.500000 |
| 9 | s Mid Freq | 0.190 | 1337.000000 | 0.750 | 4357.525391 |

## Latency at three sample rates
An impulse (1.0 on all channels) after 4096 samples of silence, the plugin at its default parameters, 1.00 s watched. Measured = position of the largest output sample after the impulse. Reported = getLatencySamples() right after prepareToPlay and after the audio. Output before the peak: the largest output between the impulse and the peak, relative to the peak (a filter with pre-ringing, a look-ahead). Output before the impulse: signal the plugin makes of its own.

| rate | reported after prepare | reported after audio | measured | output before the peak | output before the impulse |
|---|---|---|---|---|---|
| 44100 Hz | 0 | 0 | 1 | none | none |
| 48000 Hz | 0 | 0 | 1 | none | none |
| 96000 Hz | 0 | 0 | 1 | none | none |

## Delivery of parameters (A, A, B, A)
Four ways of giving the plugin its parameters, each with the settings A (defaults), A, B (the parameters that change the audio at 0.75), A. Repeatable: A, A, A the same (below -80 dB). Reacts: B differs from A (above -60 dB). As after a change: A and B equal the outputs of the same settings reached by a change of the parameters in the first way.

| way | repeatable | reacts | as after a change | result | A again (2nd / 3rd) | B against A | A, B against the references |
|---|---|---|---|---|---|---|---|
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 5.6 dB / -19.3 dBFS | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 5.6 dB / -19.3 dBFS | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 5.6 dB / -19.3 dBFS | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 5.6 dB / -19.3 dBFS | identical ; identical |

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
- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters and switches/choices in separate runs): continuous yes
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

