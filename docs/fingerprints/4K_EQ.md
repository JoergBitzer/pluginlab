# Fingerprint: 4K EQ

- file: `/home/bitzer/AudioDev/measurement_tool/plugins/ext/4k-eq-linux/VST3/4K EQ.vst3`
- format: VST3, manufacturer: Dusk Audio, version: 1.0.12
- measured: 2026-10-08 17:51
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
| delivery of parameters (A, A, B, A): ways that work | **1 of 4 ways** | one instance, parameters set after prepare (stream) |
| time-invariant (the same noise twice through one instance) | yes | -85.5 dB / -103.9 dBFS |
| settles within 0.25 s after a parameter change | **no** | -62.6 dB / -80.9 dBFS |
| block size independent (steady state) | **no** | largest at 256: -64.9 dB / -83.1 dBFS |
| deterministic (two instances, bit exact) | **no** |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | **no** | continuous parameters: no, switches and choices: no |
| digital silence in gives digital silence out | **no** | peak -96.9 dBFS |
| the same when the host renders offline (offline flag) | yes | -66.9 dB / -85.1 dBFS |
| the same at real-time pace (message loop running) | yes | -77.9 dB / -96.2 dBFS |
| a parameter change reaches the audio, fast and at real-time pace | alike | fast: 149.333 ms, real-time pace: 149.333 ms |

Bold (orange on the Developer page): worth a look (see the findings and the details below).

How to read the differences: every difference is given as relative / absolute: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- After a parameter change the plugin needs longer than 0.25 s to settle: the output then differs from the output after 2.00 s (-62.6 dB / -80.9 dBFS), a slow parameter smoothing or envelope. Tests that follow a change (delivery, recovery) can fail because of it; a larger settleSeconds in the settings shows whether they then pass.
- Delivery 'new instance per render, parameters set before prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: identical, B: -14.0 dB / -32.3 dBFS (channel 2)): the first delivery was lost.  (possibly because of the slow settling)
- Delivery 'new instance per render, parameters set after prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: identical, B: -66.4 dB / -84.8 dBFS): the first delivery was lost.  (possibly because of the slow settling)
- Delivery 'new instance per render, after prepare every parameter first set to another value, then the target' fails: the settings that were delivered first differ from the same settings reached by a change (A: identical, B: -63.0 dB / -81.3 dBFS (channel 2)): the first delivery was lost.  (possibly because of the slow settling)
- The output depends on the block size also after 1.00 s (largest at block size 256: -64.9 dB / -83.1 dBFS)
- Two instances with the same input give different output (possibly because of the slow settling)
- After jumps of the continuous parameters and the switches and choices to both ends the output does not come back to what it was (possibly because of the slow settling)
- Digital silence in does not give digital silence out (peak -96.9 dBFS) (possibly because of the slow settling)

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
| 0 | HPF Frequency | 20 | 20 | 500 | continuous | yes | -66.8 dB / -85.1 dBFS | -60.4 dB / -78.7 dBFS | defaults, the others at 0.75 |
| 1 | HPF Enabled | Off | Off | On | switch | yes | -21.0 dB / -46.0 dBFS | -20.7 dB / -45.6 dBFS (channel 2) | defaults |
| 2 | LPF Frequency | 3000 | 20000 | 20000 | continuous | yes | -66.7 dB / -85.0 dBFS (channel 2) | -66.8 dB / -85.1 dBFS | defaults, the others at 0.75 |
| 3 | LPF Enabled | Off | Off | On | switch | yes | -1.6 dB / -26.5 dBFS | -1.5 dB / -26.5 dBFS (channel 2) | defaults |
| 4 | LF Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -65.3 dB / -83.6 dBFS (channel 2) | -61.2 dB / -79.5 dBFS (channel 2) | defaults, the others at 0.75 |
| 5 | LF Frequency | 30 | 100 | 480 | continuous | yes | -70.5 dB / -88.8 dBFS | -70.3 dB / -88.6 dBFS (channel 2) | defaults, the others at 0.75 |
| 6 | LF Bell Mode | Off | Off | On | switch | yes | -73.3 dB / -91.6 dBFS | -74.0 dB / -92.4 dBFS | defaults, the others at 0.75 |
| 7 | LM Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -66.6 dB / -84.9 dBFS | -60.6 dB / -78.9 dBFS (channel 2) | defaults, the others at 0.75 |
| 8 | LM Frequency | 200 | 600 | 2500 | continuous | yes | -66.8 dB / -85.1 dBFS (channel 2) | -70.8 dB / -89.2 dBFS | defaults, the others at 0.75 |
| 9 | LM Q | 0.40 | 0.70 | 4.00 | continuous | yes | -66.9 dB / -85.2 dBFS | -63.8 dB / -82.1 dBFS | defaults, the others at 0.75 |
| 10 | HM Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -64.1 dB / -82.4 dBFS (channel 2) | -62.7 dB / -81.0 dBFS | defaults, the others at 0.75 |
| 11 | HM Frequency | 600 | 2000 | 7000 | continuous | yes | -71.1 dB / -89.4 dBFS | -68.7 dB / -87.0 dBFS (channel 2) | defaults, the others at 0.75 |
| 12 | HM Q | 0.40 | 0.70 | 4.00 | continuous | yes | -64.9 dB / -83.2 dBFS (channel 2) | -60.4 dB / -78.7 dBFS | defaults, the others at 0.75 |
| 13 | HF Gain | -20.0 | 0.0 | 20.0 | continuous | yes | -65.7 dB / -84.0 dBFS (channel 2) | -65.0 dB / -83.4 dBFS | defaults, the others at 0.75 |
| 14 | HF Frequency | 1500 | 8000 | 16000 | continuous | yes | -65.2 dB / -83.6 dBFS (channel 2) | -63.6 dB / -81.9 dBFS | defaults, the others at 0.75 |
| 15 | HF Bell Mode | Off | Off | On | switch | yes | -74.9 dB / -93.3 dBFS | -64.6 dB / -83.0 dBFS | defaults, the others at 0.75 |
| 16 | EQ Type | Brown | Brown | Black | switch | yes | -43.8 dB / -68.7 dBFS | -43.8 dB / -68.8 dBFS | defaults |
| 17 | Bypass | Off | Off | On | switch | yes | 3.0 dB / -21.9 dBFS | 3.1 dB / -21.9 dBFS (channel 2) | defaults |
| 18 | Input Gain | -12.0 | 0.0 | 12.0 | continuous | yes | -0.0 dB / -25.0 dBFS | -0.0 dB / -25.0 dBFS | defaults |
| 19 | Output Gain | -12.0 | 0.0 | 12.0 | continuous | yes | -72.6 dB / -90.9 dBFS (channel 2) | -62.6 dB / -80.9 dBFS | defaults, the others at 0.75 |
| 20 | Saturation | 0 | 0 | 100 | continuous | yes | -66.7 dB / -85.0 dBFS | -60.1 dB / -78.5 dBFS | defaults, the others at 0.75 |
| 21 | Oversampling | 2x | 2x | 4x | switch | yes | -69.1 dB / -87.4 dBFS | -60.7 dB / -79.0 dBFS (channel 2) | defaults, the others at 0.75 |
| 22 | M/S Mode | Off | Off | On | switch | yes | -60.0 dB / -85.0 dBFS | -60.0 dB / -84.9 dBFS | defaults |
| 23 | Spectrum Pre/Post | Off | Off | On | switch | yes | -65.0 dB / -83.4 dBFS (channel 2) | -67.4 dB / -85.7 dBFS (channel 2) | defaults, the others at 0.75 |
| 24 | Auto Gain Compensation | Off | On | On | switch | yes | -19.1 dB / -37.4 dBFS | -19.1 dB / -37.4 dBFS (channel 2) | defaults, the others at 0.75 |
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
| new instance per render, parameters set before prepare | yes | yes | no | FAILS | identical ; identical | 0.0 dB / -24.9 dBFS (channel 2) | identical ; -14.0 dB / -32.3 dBFS (channel 2) |
| new instance per render, parameters set after prepare | yes | yes | no | FAILS | identical ; identical | 1.5 dB / -23.5 dBFS (channel 2) | identical ; -66.4 dB / -84.8 dBFS |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | no | FAILS | identical ; identical | 1.5 dB / -23.5 dBFS (channel 2) | identical ; -63.0 dB / -81.3 dBFS (channel 2) |

Most careful way that works: one instance, parameters set after prepare (stream).

## Block sizes
1.00 s of noise through the plugin (setting B) per block size, against block size 512. Steady state: the last 0.10 s (decides; below -100 dB = independent). Whole: the full render (a plugin that smooths its parameters per block differs here, but not in the steady state).

| block size | steady state | whole |
|---|---|---|
| 32 | -74.0 dB / -92.3 dBFS (channel 2) | -73.9 dB / -92.2 dBFS |
| 64 | -72.3 dB / -90.7 dBFS (channel 2) | -72.2 dB / -90.5 dBFS |
| 128 | -84.8 dB / -103.1 dBFS (channel 2) | -84.7 dB / -103.0 dBFS (channel 2) |
| 256 | -64.9 dB / -83.1 dBFS | -64.9 dB / -83.2 dBFS |
| 1024 | -67.2 dB / -85.4 dBFS | -67.3 dB / -85.5 dBFS |
| 2048 | -68.1 dB / -86.3 dBFS | -68.1 dB / -86.4 dBFS |
| 509 | -70.9 dB / -89.1 dBFS | -70.9 dB / -89.2 dBFS |

## Other
- time-invariant (one instance: settled for 2.00 s, noise, 0.50 s silence, the same noise again; the two outputs the same): yes (-85.5 dB / -103.9 dBFS)
- settles within 0.25 s after a parameter change (the output then against the output after 2.00 s): no (-62.6 dB / -80.9 dBFS)
- deterministic (two instances, the same noise, bit exact): no
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters and switches/choices in separate runs): continuous no, switches and choices no
- digital silence in gives digital silence out: no (peak -96.9 dBFS)

## Real-time behaviour
All other measurements render as fast as possible, in real-time mode (offline flag off) and without letting the message thread run. Here the setting B with 2.0 s of noise is rendered again (fresh instances, as above): with the offline flag, and at real-time pace (after every block the message loop runs until the wall clock has caught up with the audio, so that timers and asynchronous updates of the plugin run as in a DAW). Different = more than 10 dB above the difference of two fast renders (-68.5 dB / -86.8 dBFS) and above -80 dB.

- offline flag on against off: -66.9 dB / -85.1 dBFS - the same: yes
- real-time pace against fast: -77.9 dB / -96.2 dBFS - the same: yes
- a parameter change in the middle of the noise (all parameters A -> B), until every block equals the output of B rendered at real-time pace (below -60 dB): fast: 149.333 ms, real-time pace: 149.333 ms

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

