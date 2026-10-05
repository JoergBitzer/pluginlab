# W5b: revision of the fingerprint report (working plan)

Status: plan, 2026-10-05. Sources: the author's "Comments to fingerprint report" (C1 ... C9, in `docs/reference/`) and the weaknesses and proposals of
`docs/reference/fingerprint-report-explained.md` (sections 6 and 8, cited as W1 ... W18 and P1 ... P8).
Goal: a report that is technical only (nothing that assumes an EQ or any linear, time-invariant behavior), whose numbers can be read without
this document, whose decision thresholds are visible and settable, and whose yes/no results stand in one table, in the report and on the Developer page.

## 1. Decisions to take first

### D1 (C4) Relative difference: which denominator? My argument
Current: `rel = RMS(a - b) / RMS(b)`, b = the reference. Proposal of the author to discuss: `RMS(a - b) / (RMS(a) + RMS(b))`.

- **For the question "are a and b the same?" the choice does not matter.** If a and b are nearly equal, RMS(a) + RMS(b) = 2 RMS(b), so the sum form is
  the reference form minus exactly 6.02 dB. All "same" thresholds (-80 dB, -100 dB) would just move by 6 dB. Nothing is gained, and the numbers get harder to compare
  with the usual null depth.
- **For large differences the two behave differently, and the reference form tells more.** The sum form is bounded: by the triangle inequality
  RMS(a - b) <= RMS(a) + RMS(b), so it never exceeds 0 dB. A gain change of +12 dB gives (3.98 - 1) / 4.98 = -4.4 dB, +24 dB gives -0.7 dB, a polarity inversion 0 dB, a
  silence against noise 0 dB: very different things end up close to 0 dB. The reference form gives +9.5 dB, +23 dB, +6 dB and 0 dB: it keeps the size of the change.
- **Symmetry is the real argument for the sum form.** The reference form depends on which signal is the reference. In our tests there always is a natural reference
  (the defaults, block size 512, the first render, the stream way), so the asymmetry is meaningful, not arbitrary. Where there is none (two instances, determinism) both
  are equally good.
- **The weak spot of the reference form is a silent reference** (RMS(b) = 0, today floored at 1e-12, which gives absurd +200 dB). The absolute difference solves that.

**Proposal:** keep the reference form as the relative difference and call it what it is in audio: "null depth relative to the reference" (dB re reference). Print the
**absolute difference** RMS(a - b) in dBFS next to it in every table (C4). Decisions use the relative value, except when the reference is silent (RMS below -150 dBFS): then the
absolute value decides. If the author still prefers a symmetric measure, the better candidate than the sum is the larger of the two levels, `max(RMS(a), RMS(b))`: it is
symmetric, it is the reference form whenever the reference is the louder one, and it saturates only at +6 dB (polarity inversion), not at 0 dB.

### D2 (C7) Remove the response analysis
All frequency-domain parts go: the response at the setting B, the "feature", the comparison of the rates, the findings "moves with the sample rate" and "differs between the
rates", the 128-point axis and the FFT. They assume a linear, time-invariant plugin and belong to the analyzer (W7). The latency per rate stays. The test EQs stay in the
repository (the delivery tests need `PluginLabTestEqPrepare`; `...Fs` becomes the first test case of the analyzer). The PeakEQ answer stays in the W5 design note as history.
(Consequence: W1, W5, W6, W7 of the weaknesses disappear.)

### D3 (C5) Where the settings live
`~/.config/pluginlab/fingerprint_settings.json` (Windows/macOS: the application data folder like the catalog). Written with the defaults if missing; unknown keys ignored, a
missing key takes the default, a broken file gives the defaults and a warning in the report. Every report prints the values it used (so an old report stays
interpretable when the settings change).

## 2. Work packages (in this order; each with design note entry, code, tests, docs, version)

### R1 Readability basics and settings (C4, C5, C6, P1, P3, P6, W2, W3, W9, W10)
- `FingerprintSettings` (struct + JSON load/save) with all thresholds, signal levels and lengths: reacts -80, different -60, same -80, block-independent -100, silent
  reference -150 dBFS, settle 0.25 s, noise level 0.1, the 0.25/0.75 test positions, poke distance 0.4, the parameter limit 64 (W11), the number of jumped parameters 16, block sizes.
- Every difference printed as "relative (dB re reference) / absolute (dBFS)" (D1).
- `steps`: "continuous" instead of 2147483647, "switch" for 2 (C6).
- Latency: print "nothing came out" when the impulse gives no output (W2); read the reported latency twice, right after `prepareToPlay` and after one second of audio, and
  print both (W3: plugins that report it late).
- "changes the audio": a column "measured with" = "defaults" (pass 1) or "the others at 0.75" (pass 2) (W9, P3).
- Under each table one sentence what it measures and a legend of the dB values (P6); the numbers behind each finding in the finding itself (P8).
- Tests: settings round trip and defaults; report texts; the gain plugin's Bypass shows "with the others at 0.75".

### R2 Delayed impulse and latency (C1, C7, W4)
- The impulse is placed at a pre-delay (setting, default 4096 samples) after the start, so that the response of a plugin that implements a non-causal filter by a delay
  (linear phase, look-ahead) is completely inside the window including what comes before its main peak. Latency = peak position - pre-delay.
- Print, per rate: reported (after prepare / after audio), measured, and "output before the main peak" (energy of the response between the impulse and the peak, relative
  to the peak, dB) as a technical indicator of pre-ringing or look-ahead, without interpreting it as a filter type.
- Remove the response analysis (D2).
- Test plugin `PluginLabTestLinearPhase`: a symmetric FIR (for example 255 taps) with latency 127 reported: measured 127, output before the peak present; the latency plugins as before.

### R3 Block size test that separates smoothing from real dependence (C9, W14)
- Render 1 s (setting) of noise per block size; compare (a) the **steady state**: only the last 100 ms (setting), after the plugin had a second to settle; (b) the whole
  signal, as information. "Block size independent" is decided on (a). A plugin that smooths parameters per block shows a difference in (b) and none in (a); a plugin
  whose processing depends on the block size shows it in (a).
- The block sizes stay 32 ... 2048 plus a prime; the poke block is the same length for all (the reference block size), so the input history is equal (W14).
- Test plugins: the gain plugin with per-block parameter smoothing (difference in (b), not in (a)); a deliberately block dependent variant (difference in (a)). Then the BL
  plugins of the author are measured again: the expectation of C9 is "only (b)".

### R4 Channels: exact layout and L != R (C2, C3, W8)
- Layout table (C3): number of input and output buses with their names and default channel sets; which main-bus layouts are accepted out of a list (mono, stereo, LCR, quad,
  5.0, 5.1, 7.0, 7.1, ambisonics 1st order) as in/out pairs, including mono in / stereo out; side chain bus present; accepts / produces MIDI; instrument flag.
- Second noise signal with L != R (C2): uncorrelated noise per channel (two seeds). Measured per output channel:
  - **channel coupling**: drive one input channel, keep the other silent: level at the other output (dBFS and dB re the driven channel); "independent channels" if below
    the threshold;
  - the parameter scan is done with both signals; "changes the audio" is printed per signal (a width control reacts only with L != R);
  - all relative differences are computed on all output channels (the largest is printed with the channel).
- Test plugin flag "cross feed" (mixes a part of L into R): coupling found; the gain plugin: independent.

### R5 Summary table first (C8, P8)
- Every report starts with a **summary table** of the single results: loads, layouts (short), latency reported / measured at 48 kHz, latency consistent over the rates,
  parameters (number / changing the audio), delivery (the most careful way that works, or "none"), block size independent (steady state), deterministic, time-invariant (R6),
  output finite, recovers, silent for silence, channel coupling. Each cell "yes / no / value" and, where it applies, the measured number.
- The measurement process writes the same results as JSON next to the report (`<report>.json`). The Developer page shows them as columns (one row per selected plugin, the
  same as the table in the report), with a tooltip of the number; "View report" stays for the details.
- The detailed sections follow the summary, in the order of the table.

### R6 Fewer false alarms for time-varying plugins (W12, W13, W15, W16)
- A time invariance test: the same noise twice through one instance with 0.5 s of silence between; the two outputs must be equal. If not: "time-varying (LFO, random, dither,
  envelope with memory)". Determinism, recovery and silence are then printed with the remark "expected for a time-varying plugin", not as a fault.
- Recovery: switches and choice parameters are jumped separately from continuous parameters; the report says which group did not recover (W15).
- Silence: print the idle level in dBFS always; "silent" decided against a threshold in the settings (default: exactly 0), so that -149 dBFS can be accepted by a setting (W16).
- The stream reference problem (W13): a plugin that ignores parameter changes in the stream way gets "stream ignores parameter changes" from the reaction check of the
  stream row itself (B against A1), before the other ways are judged against it.

### R6b Parameters that act only together (found with Venn Audio Free EQ, 2026-10-05)
- Free EQ has six bands, all **disabled by default** ("Band n Enabled: Off"). Its report says "no parameter changed the audio": switching a band on alone (gain 0 dB) and
  moving a gain alone (band off) both leave the audio unchanged, and the second pass only starts from parameters that reacted in the first. The plugin is fine; the scan
  cannot find parameters that need a partner.
- Fix: a pass 0 that flips every switch away from its default (and every choice to another value) before the scan, then the two passes as now with that base; if still
  nothing reacts, try each switch together with each continuous parameter at 0.75 (pairs, capped). The report says which base the scan used.
- Test plugin: the EQ test plugin with an "Enabled" switch that is off by default.

### R7 Setting B visible (P4)
- Print the setting B at the top of the details: parameter name, normalised value, the plugin's text for it. B is still "the parameters that change the audio at 0.75,
  switches excluded", but now it can be read in the plugin's own units.

### R8 Documentation and tests
- Rewrite `fingerprint-report-explained.md` for the new report (the sections that go, the new ones, the settings file). Each of R1 to R7 has tests with test plugins of known
  behavior; the old fingerprint tests are adapted (the sample-rate expectations move to the analyzer later).
- Version: one minor version per package (0.13.0 ...), docs-only changes without a version.

## 3. Open points for the author
1. D1: reference form plus absolute (my proposal), or a symmetric form (then `max(RMS(a), RMS(b))` rather than the sum)?
Answer: we use your poposal
2. R2: pre-delay of 4096 samples enough (85 ms at 48 kHz)? Linear-phase plugins with long FIRs need more; it costs nothing but time.
Answer: It will show that there is something, so 4096 is enough
3. R3: 1 s render and the last 100 ms (the author's numbers) as defaults in the settings, agreed.
   Answer: agreed
4. R4: which layouts in the list besides mono/stereo are worth asking for (each asks the plugin, cheap)? 
Answer, what are the most common layouts in the wild? LCR, quad, 5.1, 7.1, ambisonics 1st order. The rest is not worth it.
5. R6: is "time-varying" an acceptable verdict for the summary, or should those plugins be measured with the LFO frozen when they offer that (not generic)?
Answer: "time-varying" is good and general enough. I have for example a plugin that will add noise at a level high enough for you to detect. It is time varying too.
6. Order: R1, R2, R3 first (they change existing numbers), then R5 (the table), then R4, R6, R7?
Yes
## 4. Progress
- **R1 to R3 done (0.13.0, 2026-10-05).** Settings file `fingerprint_settings.json` (all thresholds, levels, lengths, block sizes; printed in every report; `PLUGINLAB_FINGERPRINT_SETTINGS`
  for the tests); every difference relative (dB re reference) and absolute (dBFS), the absolute one decides for a silent reference; steps "continuous" / "switch"; column
  "measured with"; latency with "nothing came out", reported after prepare and after audio, delayed impulse (4096 samples), output before the peak and before the impulse; the
  response analysis is removed; block size test with 1 s render, decided on the last 0.1 s, "whole" for information, the same poke length for all block sizes; a sentence
  under every table and a legend of the differences; the numbers in the findings. New test plugins: Linear Phase (255-tap FIR), Block Smoothing, Block Fault.
- Found on the way, both defects of the test, not of the plugins:
  - the small block size differences of the BL gain plugins came from the poke block, whose length was the block size (different input history); now identical;
  - the stream way of the delivery test rendered B and the last A right after the change, without settling: a plugin that smooths its parameters (BL-Gain, WayQ) failed all
    four ways against wrong references. A settle after every change fixes it (BL-Gain, WayQ: all ways ok now).
- Not measurable yet: ZL Equalizer 2 ("runs neither with mono nor with stereo audio"), most likely because it has a side-chain input bus and the layout we ask for has one
  input bus only. This belongs to R4 (exact layouts) and also concerns the loader and the engine.
- The reports in `docs/fingerprints/` are made again with 0.13.0.
- **R5 done (0.14.0, 2026-10-05).** `summarize()` gives the single results (key, test, result, detail, good); the report starts with them as a table (bold = worth a look), the
  legend and the findings follow; `PluginLabHost --fingerprint` writes them as `<report>.json`; the Developer page shows them as columns (orange = worth a look, tooltip =
  the number) and puts the buttons and the state of the report first. Tests: summary of the gain plugin all good, the latency liar "0 / 100" flagged, JSON round trip,
  CTest checks the JSON file. The time-invariance result (R6) and the channel coupling (R4) are added to the summary with those steps.
