# Mapping

Unit of W8.5. Code: `src/engine/KnobLaw.cpp` (knob laws),
`src/engine/Mapping.cpp` (proposal, file, setting for a band); the host's
`--map`; tests `tests/MappingTests.cpp` and the mapping check in
`tests/ParameterSurveyTests.cpp`. Files: `mappings/` (our own plugins).
Plan: `docs/design/W8-plan.md`, W8.5.

## Purpose
The survey (W8.4, [parameter-survey.md](parameter-survey.md)) says what each
parameter does. The mapping writes it down so that a program can use it:
"parameter 3 is the frequency of band 1, its knob runs logarithmically from
20 Hz to 20 kHz; it acts only when the band's gain is not 0". The prototype
needed an hour of manual exploration per plugin for its mapping files; here
the survey proposes the mapping and a person confirms it.

## Knob laws
A law maps the normalised value v (0 ... 1, what the host sends) to a value x.

| Law | x(v) | Typical |
|---|---|---|
| linear | min + (max - min) v | gain in dB, Q |
| log | min (max / min)^v | frequency |
| power | min + (max - min) v^(1/skew) | JUCE's NormalisableRange with a skew factor |
| table | one value per step | a discrete knob (order 1 ... 8, a stepped frequency) |

Every parameter gets two laws:
- **as shown**: from the texts the plugin shows for v (101 values of a
  continuous knob, every step of a discrete one; the first number of the
  text, "k" = 1000). No render is needed.
- **as measured**: from the survey's points (the W8.3 fit: the dominant band's
  frequency, gain, Q or order; the level at 1 kHz; the delay).

The fit: least squares for linear and log; for power the skew on a log grid
(0.05 ... 20) refined by golden section, the ends by least squares. The
simplest law within 10 % of the best error wins (linear, then log, then
power). Myth and reality (W8.6) is the difference of the two laws.

## The mapping
| Field | Content |
|---|---|
| plugin | name, manufacturer, version, format, identifier (format:name:unique id, the same on every machine) |
| sampleRates, date, proposedBy | how the proposal was made |
| confirmedBy | empty: a proposal; a name: checked by a person (or, for our own plugins, against their code by the tests) |
| bands | the number of bands found |
| parameters | per parameter: index, name, role (W8.4), quantity (frequency, gain, q, order, type, family, band on/off, output level, latency, other, none), band, requires (other parameters' values it needs to act), the two laws, the steps of a switch (text, what was measured, band type and family) |

Bands: the band parameters whose bands lie within a third of an octave of each
other form one band (a frequency knob at its default value, the band of a gain
knob, the band that a type switch changes); the bands are numbered by
frequency. Switches: a switch whose steps change the band type is a type
switch, one that changes only the design family a family switch, one that
turns the band on and off a band switch.

## A setting for a band
`makeSettingForBand(mapping, base, band, wanted)` sets the band switch to "on",
the type switch to the step with the wanted type, the family switch to the
step with the wanted family, the frequency, gain and Q (or order) through the
inverse of their measured laws, and then what these parameters need to act
(unless the mapping set it itself). What it cannot set is listed.

## Where the files are
- Our own plugins (reference and test plugins): `mappings/`, in the
  repository. The survey tests compare every new proposal with these files;
  `PLUGINLAB_WRITE_MAPPINGS=<folder>` writes the proposals for a review.
- All other plugins (also the author's two test objects): the user's settings
  folder, `mappings/` (Linux `~/.config/pluginlab/mappings`; overridden by
  `PLUGINLAB_SETTINGS_FOLDER`). In the host: the Developer page, button
  "Survey + mapping" (a process of its own, minutes per plugin, with a
  progress bar: parameter i of n and the elapsed time; "View mapping"
  shows the report, the column "Mapping" the state; `--developer <plugin file>
  --mapping [--view]` does the same from the command line). Without the
  window: `PluginLabHost --map <plugin file> <report>`. Both survey the plugin,
  write the report (survey and mapping, Markdown) and the proposal there. A confirmed file is never overwritten: the new proposal goes
  next to it (`.proposal.json`) and the report lists the differences.
- Confirming (0.55.0, W8.8): in the host, the window of "View mapping" has
  "Open mapping file" (the JSON in the system's editor, for corrections; a newer
  `.proposal.json` next to a confirmed mapping is opened first) and "Confirm
  mapping ..." (asks for the name, remembered for the next time, and writes
  "name (date)" into `confirmedBy`; a confirmed proposal replaces the old
  mapping). The column "Mapping" shows "confirmed by ..." or "proposal (not
  confirmed)". Without the host: edit the JSON and write the name into
  `confirmedBy`.

## The report
The survey and mapping report (`--map`, the Developer page) is built of
tables (0.49.0):

| Section | Content |
|---|---|
| Parameter survey | one row per parameter: range shown, role, measured effect, acts only when, system class at the two ends (LTI, time-variant, non-linear with the kind) |
| Steps of the switches | one row per step: the other parameters set, the step's text, the measured response in the band model |
| Side effects | what a parameter changes besides its role (proportional Q, small bands of the fit left out) |
| Mapping | one row per parameter: band, quantity, law as shown, law as measured, needs |
| Steps of the switches (mapping) | one row per step: measured response, band type, family (what `makeSettingForBand` chooses by) |
| About this report | why the mapping exists, what the survey and the mapping do, the columns, what to do with the proposal |

Frequencies are written without false precision (one decimal below 100 Hz,
whole hertz above).

## Results
From the survey tests and `MappingTests` (2026-10-11, 0.47.0), 48 kHz, Debug.

**Knob laws** (synthetic): linear, log and JUCE's skew 0.3 are recovered
exactly from the survey's 5 points and from 101 sampled texts (largest
relative error below 1e-9; the round trip of the value below 1e-6).

**Proposals for our own plugins** (`mappings/`): every row as the plugin's
code says, without edits.

| Plugin | Proposal (shortened) |
|---|---|
| Reference EQ | 1 band. Algorithm: family switch (RBJ, Orfanidis, Zoelzer and state variable as RBJ, Butterworth and Linkwitz-Riley flat with a peak, linear phase). Type: type switch. Frequency: log, 20.0 ... 20016 Hz as shown (the texts are whole Hz), log 20.0 ... 20003 Hz as measured, needs Gain = 21.6 dB. Gain: linear -24 ... 24 dB. Q: linear 0.1 ... 20 (measured 0.103 ... 20). Order: table 1 ... 8 (a stepped knob found from its texts), needs Algorithm = Butterworth, Type = Low-pass. Bypass: band on/off. |
| Test EQ Switch | 1 band. Gain linear -24 ... 24 dB, needs Enabled = On; Frequency power (skew 0.300) 20 ... 20000 Hz, shown and measured; Q linear 0.1 ... 10; Enabled: band on/off. |
| Test EQ Drive | 1 band as above (Frequency needs Gain); Drive: distortion; Output: output level, linear -12 ... 12 dB; Display: none. |
| Reference Utility | 0 bands. Gain: output level, linear -60 ... 24 dB; Delay: latency, linear 0 ... 4800 samples; Bypass: latency, table 1782, 0 samples (with the delay it needs). |
| Reference Nonlinear | 0 bands. Drive and Output: output level, linear -24 ... 24 dB (with the curve off); the rest: distortion, idle output, switches. |

**The files drive the plugins** (requested band, set from the file, measured
by the swept sine, largest deviation 20 Hz ... 20 kHz where the band is above
-60 dB):

| Plugin | Requested | Deviation |
|---|---|---|
| Reference EQ | RBJ peak 1 kHz +6 dB Q 2 | 0.016 dB |
| Reference EQ | Orfanidis peak 3 kHz -4 dB Q 1 | 0.002 dB |
| Reference EQ | RBJ low shelf 200 Hz +3 dB Q 0.71 | 0.0007 dB |
| Reference EQ | Butterworth low-pass 2 kHz, order 4 | 0.018 dB |
| Test EQ Switch | RBJ peak 500 Hz +8 dB Q 3 (the switch set by the mapping) | 0.001 dB |
| Test EQ Drive | RBJ peak 2 kHz -6 dB Q 1.5 | 0.0002 dB |

The largest deviations come from the measured laws' ends (the Q law's 0.103
instead of 0.1 at v = 0: 0.003 in Q).

**What the mapping taught** (corrections on the way)
- At the edge of the band a fit trades the band's frequency against a small
  extra band (a peak at 28 Hz came out at 27.8 Hz with a shelf at 10 Hz): the
  measured law uses only the points whose fit has the usual number of bands.
- Three points fit a power law exactly: errors below 1e-4 of the range count
  as equal, so the simpler law (log) wins.
- A whole-number knob (order 1 ... 8) that the host is told is continuous is
  found from its texts and surveyed step by step: both laws become tables.
- A notch is also measured as a deep peak: of several steps with the wanted
  band type, the one whose text names it ("Peak") is taken.
- A band that no frequency, gain or Q knob belongs to (Lagrange's droop at
  high frequencies, "a shelf") is a side effect, not an EQ band.

## Limits
- One sample rate (48 kHz) so far; W8.6 checks the laws at 44.1, 48 and 96 kHz.
- Bands are grouped by where they sit at the defaults; two bands of a
  multi-band EQ that start at the same frequency would be merged (the
  confirmation has to split them).
- The measured law is fitted to 5 points of a continuous knob; a law that is
  none of linear, log and power (a table of a real analogue model) is
  approximated.
- Not yet tried on the author's two test objects (local): `PluginLabHost --map`.
