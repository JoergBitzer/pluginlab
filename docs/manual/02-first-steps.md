# First steps: does the demo EQ sound like mine?

The page **Easy** answers the musician's question in five steps. It is the first page of the plugin and the last tab of the application.

## 1. My EQ
Load your EQ and the EQs you want to compare (page **Plugins**, or "Load plugins ..." on Easy; up to four: yours and three others). Choose
yours under "My EQ", open it with "Show its editor" and set it as it sounds good to you. It is the reference: the others are made to sound
like it.

## 2. Compare with
The other loaded plugins, at most three, are listed here.

## 3. Make them the same
One button. Jade Benchmark sets the knobs of the other EQs so that their frequency response comes as close to yours as possible: the band
knobs of each plugin's mapping (frequency, gain, Q), or, for a plugin without a mapping, every continuous knob. It takes about 15 s per EQ
and runs in a process of its own (a plugin that crashes does not take the DAW or the application down). The found settings are applied at
once.

## 4. Listen
A row per EQ and one "without EQ". The number button makes it the one you hear (a short crossfade, no gap). Each row says how close it came:

| Words | Largest difference | Meaning |
|---|---|---|
| the same | up to 0.25 dB | the tolerance of the match |
| audibly close | up to 1 dB | about the smallest level difference one hears in a third-octave band |
| different | above 1 dB | a different sound; the row says the main reason (a knob at the end of its range, another filter design, a missing band, ...) |

The **level slider** of each row keeps the loudness the same while you switch: the match sets it, you can move it. Compare sound, not
loudness (louder nearly always sounds "better"). In the plugin your DAW plays the track; in the application "Play" plays the audio files
of the page EQ Compare.

## 5. Keep it
"Save as presets" stores the settings of the other EQs as presets of those plugins in the settings folder.

## What it does not tell
A match of the magnitude response is not a null: two EQs can have the same frequency response and still differ in phase (minimum against
linear phase) or in their distortion. The page **Expert** measures that (null test, THD, phase), the page **EQ Compare** shows the curves
while you turn the knobs. "The same" is a statement about the frequency response at this setting, not about the plugin in general.
