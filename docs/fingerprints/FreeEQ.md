# Fingerprint: Free EQ

- file: `/tmp/claude-1000/-home-bitzer-AudioDev/cb378125-017c-4b6c-a2ae-35e52500030b/scratchpad/venn/FreeEQ.vst3`
- format: VST3, manufacturer: Venn Audio, version: 1.5.7
- measured: 2026-10-06 10:07
- channels: mono yes, stereo yes

## Summary
| test | result | detail |
|---|---|---|
| loads and runs with mono or stereo | yes |  |
| main-bus layouts accepted | mono, stereo | measured with 2 channel(s) |
| side-chain input (more than one input bus) | no |  |
| MIDI | none |  |
| channels independent (one input driven, the other silent) | yes | L to R: silent, R to L: silent |
| parameters / changing the audio | 37 / 25 |  |
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
| mono | yes |
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
Noise (peak 0.10) through the plugin with each parameter at 0.25 and 0.75 of its range, against the plugin at the base setting named in the last column; the larger change is shown. "no": below -80 dB in both passes. 37 parameters. Two test signals: the same noise on all channels (L = R) and different noise on the channels (L != R, only with more than one channel; a width or mid/side control reacts only to this one). The scan started from every switch away from its default and every choice one step on (nothing changed the audio at the defaults).

| no. | name | min | default | max | steps | automatable | changes (L = R) | changes (L != R) | measured with |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Band 1 Enabled | Off | Off | On | switch | yes | no | no |  |
| 1 | Band 1 Frequency | -1.000 | -0.682 | 1.000 | continuous | yes | 7.7 dB / 5.4 dBFS | 7.7 dB / 5.4 dBFS | switches flipped, the others at 0.75 |
| 2 | Band 1 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | 4.1 dB / 1.8 dBFS | 4.5 dB / 2.3 dBFS (channel 2) | switches flipped, the others at 0.75 |
| 3 | Band 1 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | -9.9 dB / -34.7 dBFS | -9.9 dB / -34.7 dBFS | switches flipped |
| 4 | Band 1 Type | 1 | 5 | 7 | continuous | yes | 16.0 dB / 13.7 dBFS | 16.0 dB / 13.7 dBFS | switches flipped, the others at 0.75 |
| 5 | Band 2 Enabled | Off | Off | On | switch | yes | no | no |  |
| 6 | Band 2 Frequency | -1.000 | -0.334 | 1.000 | continuous | yes | 7.6 dB / 5.3 dBFS | 7.6 dB / 5.3 dBFS | switches flipped, the others at 0.75 |
| 7 | Band 2 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | 4.9 dB / 2.6 dBFS | 4.9 dB / 2.6 dBFS | switches flipped, the others at 0.75 |
| 8 | Band 2 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | -5.9 dB / -30.7 dBFS | -5.3 dB / -30.1 dBFS (channel 2) | switches flipped |
| 9 | Band 2 Type | 1 | 5 | 7 | continuous | yes | 15.3 dB / 13.0 dBFS | 15.3 dB / 13.0 dBFS | switches flipped, the others at 0.75 |
| 10 | Band 3 Enabled | Off | Off | On | switch | yes | no | no |  |
| 11 | Band 3 Frequency | -1.000 | -0.132 | 1.000 | continuous | yes | 7.0 dB / 4.7 dBFS | 7.0 dB / 4.7 dBFS | switches flipped, the others at 0.75 |
| 12 | Band 3 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | 5.9 dB / 3.6 dBFS | 5.9 dB / 3.6 dBFS | switches flipped, the others at 0.75 |
| 13 | Band 3 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | -2.4 dB / -27.2 dBFS | -2.2 dB / -27.0 dBFS (channel 2) | switches flipped |
| 14 | Band 3 Type | 1 | 5 | 7 | continuous | yes | 14.8 dB / 12.5 dBFS | 14.8 dB / 12.5 dBFS | switches flipped, the others at 0.75 |
| 15 | Band 4 Enabled | Off | Off | On | switch | yes | no | no |  |
| 16 | Band 4 Frequency | -1.000 | 0.132 | 1.000 | continuous | yes | 4.4 dB / 2.1 dBFS | 4.4 dB / 2.1 dBFS | switches flipped, the others at 0.75 |
| 17 | Band 4 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | 9.4 dB / 7.1 dBFS | 9.4 dB / 7.1 dBFS | switches flipped, the others at 0.75 |
| 18 | Band 4 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | 1.4 dB / -23.4 dBFS | 1.4 dB / -23.4 dBFS (channel 2) | switches flipped |
| 19 | Band 4 Type | 1 | 5 | 7 | continuous | yes | 13.7 dB / 11.4 dBFS | 13.7 dB / 11.4 dBFS | switches flipped, the others at 0.75 |
| 20 | Band 5 Enabled | Off | Off | On | switch | yes | no | no |  |
| 21 | Band 5 Frequency | -1.000 | 0.598 | 1.000 | continuous | yes | 0.6 dB / -1.7 dBFS | 0.6 dB / -1.6 dBFS (channel 2) | switches flipped, the others at 0.75 |
| 22 | Band 5 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | 6.6 dB / 4.3 dBFS | 6.7 dB / 4.5 dBFS (channel 2) | switches flipped, the others at 0.75 |
| 23 | Band 5 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | 7.6 dB / -17.3 dBFS | 7.6 dB / -17.3 dBFS | switches flipped |
| 24 | Band 5 Type | 1 | 5 | 7 | continuous | yes | 10.3 dB / 8.0 dBFS | 10.6 dB / 8.3 dBFS (channel 2) | switches flipped, the others at 0.75 |
| 25 | Band 6 Enabled | Off | Off | On | switch | yes | no | no |  |
| 26 | Band 6 Frequency | -1.000 | 0.800 | 1.000 | continuous | yes | 3.5 dB / 1.2 dBFS | 3.5 dB / 1.2 dBFS | switches flipped, the others at 0.75 |
| 27 | Band 6 Resonance | 0.000 | 0.500 | 1.000 | continuous | yes | 5.2 dB / 2.9 dBFS | 5.2 dB / 2.9 dBFS | switches flipped, the others at 0.75 |
| 28 | Band 6 Gain | -35.00 | 0.00 | 35.00 | continuous | yes | 9.2 dB / -15.6 dBFS | 9.3 dB / -15.5 dBFS (channel 2) | switches flipped |
| 29 | Band 6 Type | 1 | 5 | 7 | continuous | yes | 12.0 dB / 9.7 dBFS | 12.1 dB / 9.9 dBFS (channel 2) | switches flipped, the others at 0.75 |
| 30 | Band 1 Slope | 0 | 1 | 3 | continuous | yes | no | no |  |
| 31 | Band 2 Slope | 0 | 1 | 3 | continuous | yes | no | no |  |
| 32 | Band 3 Slope | 0 | 1 | 3 | continuous | yes | no | no |  |
| 33 | Band 4 Slope | 0 | 1 | 3 | continuous | yes | no | no |  |
| 34 | Band 5 Slope | 0 | 1 | 3 | continuous | yes | no | no |  |
| 35 | Band 6 Slope | 0 | 1 | 3 | continuous | yes | no | no |  |
| 36 | Bypass | Off | Off | On | switch | yes | -0.3 dB / -2.6 dBFS | -0.3 dB / -2.5 dBFS (channel 2) | switches flipped, the others at 0.75 |

## The settings A and B
A = the defaults. B = the parameters that change the audio at 0.75 of their range (switches left out, starting from every switch away from its default and every choice one step on (nothing changed the audio at the defaults)); B is used by the delivery, block size, determinism, time invariance, jump, silence and coupling tests. Only the parameters where B differs from A are listed.

| no. | name | A (normalised) | A | B (normalised) | B |
|---|---|---|---|---|---|
| 0 | Band 1 Enabled | 0.000 | Off | 1.000 | On |
| 3 | Band 1 Gain | 0.500 | 0.00 | 0.750 | 17.50 |
| 4 | Band 1 Type | 0.667 | 5 | 0.750 | 6 |
| 5 | Band 2 Enabled | 0.000 | Off | 1.000 | On |
| 6 | Band 2 Frequency | 0.333 | -0.334 | 0.750 | 0.500 |
| 7 | Band 2 Resonance | 0.500 | 0.500 | 0.750 | 0.750 |
| 8 | Band 2 Gain | 0.500 | 0.00 | 0.750 | 17.50 |
| 9 | Band 2 Type | 0.667 | 5 | 0.750 | 6 |
| 10 | Band 3 Enabled | 0.000 | Off | 1.000 | On |
| 11 | Band 3 Frequency | 0.434 | -0.132 | 0.750 | 0.500 |
| 12 | Band 3 Resonance | 0.500 | 0.500 | 0.750 | 0.750 |
| 13 | Band 3 Gain | 0.500 | 0.00 | 0.750 | 17.50 |
| 14 | Band 3 Type | 0.667 | 5 | 0.750 | 6 |
| 15 | Band 4 Enabled | 0.000 | Off | 1.000 | On |
| 16 | Band 4 Frequency | 0.566 | 0.132 | 0.750 | 0.500 |
| 17 | Band 4 Resonance | 0.500 | 0.500 | 0.750 | 0.750 |
| 18 | Band 4 Gain | 0.500 | 0.00 | 0.750 | 17.50 |
| 19 | Band 4 Type | 0.667 | 5 | 0.750 | 6 |
| 20 | Band 5 Enabled | 0.000 | Off | 1.000 | On |
| 21 | Band 5 Frequency | 0.799 | 0.598 | 0.750 | 0.500 |
| 22 | Band 5 Resonance | 0.500 | 0.500 | 0.750 | 0.750 |
| 23 | Band 5 Gain | 0.500 | 0.00 | 0.750 | 17.50 |
| 24 | Band 5 Type | 0.667 | 5 | 0.750 | 6 |
| 25 | Band 6 Enabled | 0.000 | Off | 1.000 | On |
| 26 | Band 6 Frequency | 0.900 | 0.800 | 0.750 | 0.500 |
| 27 | Band 6 Resonance | 0.500 | 0.500 | 0.750 | 0.750 |
| 28 | Band 6 Gain | 0.500 | 0.00 | 0.750 | 17.50 |
| 29 | Band 6 Type | 0.667 | 5 | 0.750 | 6 |

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
| one instance, parameters set after prepare (stream) | yes | yes | yes | ok | identical ; identical | 137.2 dB / 112.4 dBFS (channel 2) | identical ; identical |
| new instance per render, parameters set before prepare | yes | yes | yes | ok | identical ; identical | 137.2 dB / 112.4 dBFS (channel 2) | identical ; identical |
| new instance per render, parameters set after prepare | yes | yes | yes | ok | identical ; identical | 137.2 dB / 112.4 dBFS (channel 2) | identical ; identical |
| new instance per render, after prepare every parameter first set to another value, then the target | yes | yes | yes | ok | identical ; identical | 137.2 dB / 112.4 dBFS (channel 2) | identical ; identical |

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

