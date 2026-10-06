# Fingerprint: BL-StereoWidth

- file: `/home/bitzer/.vst3/BL-StereoWidth.vst3`
- format: VST3, manufacturer: BlueLab, version: 6.3.4
- measured: 2026-10-06 10:06
- channels: mono no, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| main-bus layouts accepted | stereo, mono in, stereo out | measured with 2 channel(s) |
| side-chain input (more than one input bus) | no |  |
| MIDI | none |  |
| channels independent (one input driven, the other silent) | no | L to R: -6.7 dB re the driven channel (-29.0 dBFS), R to L: -22.0 dB re the driven channel (-36.7 dBFS) |
| parameters / changing the audio | 15 / 10 |  |
| latency at 48 kHz, reported / measured (samples) | 0 / 0 | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| reported latency = measured at all rates | yes | 44.1 kHz: 0 / 0, 48.0 kHz: 0 / 0, 96.0 kHz: 0 / 0 |
| output before the peak of the impulse response | no |  |
| output before the impulse (signal of its own) | no |  |
| delivery of parameters (A, A, B, A): ways that work | **none** |  |
| time-invariant (the same noise twice through one instance) | yes | identical |
| settles within 0.25 s after a parameter change | **no** | -77.7 dB / -99.9 dBFS |
| block size independent (steady state) | yes | largest at 32: identical |
| deterministic (two instances, bit exact) | yes |  |
| output stays finite after parameter jumps | yes |  |
| recovers from parameter jumps | **no** | continuous parameters: no, switches and choices: no |
| digital silence in gives digital silence out | yes |  |

Bold: worth a look (see the findings and the details below).

How to read the differences: every difference is given as **relative / absolute**: relative = RMS(output - reference) / RMS(reference) in dB (0 dB: the change is as large as the signal, -40 dB: 1 %, +6 dB: twice the signal, as for a polarity inversion); absolute = RMS(output - reference) in dBFS. "identical": bit exact. For a silent reference only the absolute value counts.

## Findings
- After a parameter change the plugin needs longer than 0.25 s to settle: the output then differs from the output after 2.00 s (-77.7 dB / -99.9 dBFS), a slow parameter smoothing or envelope. Tests that follow a change (delivery, recovery) can fail because of it; a larger settleSeconds in the settings shows whether they then pass.
- Delivery 'one instance, parameters set after prepare (stream)' fails: the same settings gave other output (A again: -62.2 dB / -87.0 dBFS). the settings that were delivered first differ from the same settings reached by a change (A: -62.2 dB / -87.0 dBFS, B: identical): the first delivery was lost.  (possibly because of the slow settling)
- Delivery 'new instance per render, parameters set before prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -62.2 dB / -87.0 dBFS, B: -2.3 dB / -24.4 dBFS): the first delivery was lost.  (possibly because of the slow settling)
- Delivery 'new instance per render, parameters set after prepare' fails: the settings that were delivered first differ from the same settings reached by a change (A: -62.2 dB / -87.0 dBFS, B: -147.0 dB / -169.2 dBFS): the first delivery was lost.  (possibly because of the slow settling)
- Delivery 'new instance per render, after prepare every parameter first set to another value, then the target' fails: the settings that were delivered first differ from the same settings reached by a change (A: -64.4 dB / -89.2 dBFS, B: -136.0 dB / -158.2 dBFS): the first delivery was lost.  (possibly because of the slow settling)
- After jumps of the continuous parameters and the switches and choices to both ends the output does not come back to what it was (possibly because of the slow settling)

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
| mono in, stereo out | yes |
| LCR | no |
| quad | no |
| 5.1 | no |
| 7.1 | no |
| ambisonics 1st order | no |

- side chain: no; MIDI in: no, out: no; instrument: no
- coupling L to R: -6.7 dB re the driven channel (-29.0 dBFS), R to L: -22.0 dB re the driven channel (-36.7 dBFS): channels independent no

## Parameters
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 15 parameters. Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a width or mid/side control reacts only to this one). The scan started from the defaults.

| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Bypass | off | off | on | switch | yes | -13.9 dB / -38.7 dBFS | -13.9 dB / -38.7 dBFS | defaults |
| 1 | MonoToStereo | Off | Off | On | switch | yes | 3.1 dB / -21.7 dBFS | 1.8 dB / -23.0 dBFS (channel 2) | defaults |
| 2 | WidthBoost | Off | Off | On | switch | yes | no | 0.6 dB / -21.6 dBFS | defaults, the others at 0.75 |
| 3 | BassToMono | Off | Off | On | switch | yes | no | -27.9 dB / -52.7 dBFS | defaults |
| 4 | BassFocus | 0 | 100 | 6000 | continuous | yes | -2.0 dB / -26.9 dBFS (channel 2) | -2.0 dB / -26.9 dBFS | defaults |
| 5 | Width | -100.00 | 0.00 | 100.00 | continuous | yes | no | -9.0 dB / -33.8 dBFS | defaults |
| 6 | Pan | -100.00 | 0.00 | 100.00 | continuous | yes | -6.8 dB / -31.6 dBFS (channel 2) | -6.8 dB / -31.5 dBFS (channel 2) | defaults |
| 7 | WidthLimit | Off | Off | On | switch | yes | no | -9.6 dB / -34.5 dBFS | defaults |
| 8 | WidthLimitSpeed | 0.00 | 50.00 | 100.00 | continuous | yes | no | no |  |
| 9 | MonoOut | Off | Off | On | switch | yes | no | -2.9 dB / -27.8 dBFS | defaults |
| 10 | OutGain | -12.0 | 0.0 | 12.0 | continuous | yes | -0.0 dB / -24.9 dBFS (channel 2) | -0.0 dB / -24.9 dBFS | defaults |
| 11 | VectorscopeMode0 | 0 | 1 | 1 | switch | yes | no | no |  |
| 12 | VectorscopeMode1 | 0 | 0 | 1 | switch | yes | no | no |  |
| 13 | VectorscopeMode2 | 0 | 0 | 1 | switch | yes | no | no |  |
| 14 | VectorscopeMode3 | 0 | 0 | 1 | switch | yes | no | no |  |

## The settings A and B
A = the defaults. B = the parameters that change the audio at 0.75 of their range (switches left out, starting from the defaults); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters where B differs from A are listed.

| no. | name | A (normalised) | A | B (normalised) | B |
|---|---|---|---|---|---|
| 4 | BassFocus | 0.359 | 100 | 0.750 | 1898 |
| 5 | Width | 0.500 | 0.00 | 0.750 | 50.00 |
| 6 | Pan | 0.500 | 0.00 | 0.750 | 50.00 |
| 10 | OutGain | 0.500 | 0.0 | 0.750 | 6.0 |

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
| one instance, parameters set after prepare (stream) | no | yes | no | FAILS | -145.0 dB / -169.8 dBFS ; -62.2 dB / -87.0 dBFS | 8.5 dB / -16.3 dBFS (channel 2) | -62.2 dB / -87.0 dBFS ; identical |
| new instance per render, parameters set before prepare | yes | yes | no | FAILS | identical ; identical | 7.4 dB / -17.3 dBFS (channel 2) | -62.2 dB / -87.0 dBFS ; -2.3 dB / -24.4 dBFS |
| new instance per render, parameters set after prepare | yes | yes | no | FAILS | identical ; identical | 8.5 dB / -16.3 dBFS (channel 2) | -62.2 dB / -87.0 dBFS ; -147.0 dB / -169.2 dBFS |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | no | FAILS | identical ; identical | 8.5 dB / -16.3 dBFS (channel 2) | -64.4 dB / -89.2 dBFS ; -136.0 dB / -158.2 dBFS |

No way of delivering the parameters passed the test.

## Block sizes
1.00 s of noise through the plugin (setting B) per block size, against block size 512. Steady state: the last 0.10 s (decides; below -100 dB = independent). Whole: the full render (a plugin that smooths its parameters per block differs here, but not in the steady state).

| block size | steady state | whole |
|---|---|---|
| 32 | identical | -78.6 dB / -93.1 dBFS (channel 2) |
| 64 | identical | -79.4 dB / -93.9 dBFS (channel 2) |
| 128 | identical | -80.1 dB / -94.6 dBFS (channel 2) |
| 256 | identical | -81.5 dB / -96.0 dBFS (channel 2) |
| 1024 | identical | -83.6 dB / -98.1 dBFS (channel 2) |
| 2048 | identical | -76.0 dB / -90.5 dBFS (channel 2) |
| 509 | identical | -94.6 dB / -109.1 dBFS (channel 2) |

## Other
- time-invariant (one instance: settled for 2.00 s, noise, 0.50 s silence, the same noise again; the two outputs the same): yes (identical)
- settles within 0.25 s after a parameter change (the output then against the output after 2.00 s): no (-77.7 dB / -99.9 dBFS)
- deterministic (two instances, the same noise, bit exact): yes
- output stays finite (no NaN or infinity in the jump test): yes
- recovers from parameter jumps (the parameters that change the audio to 0 and 1 and back, then the output of setting B again; continuous parameters and switches/choices in separate runs): continuous no, switches and choices no
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

