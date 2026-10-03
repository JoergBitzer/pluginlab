# pluginlab - planning (fresh start)

Status: draft v1 (2026-10-03). This is the second attempt. The first prototype (repo `measurement_tool`, tag `prototype-0.6.0`)
was a learning vehicle; its results are in [docs/prototype/](docs/prototype/). Nothing here is decided until marked **DECIDED**.
No code has been written yet; the prototype code is reading material only.

## 1. Goals
1. Measure VST plugins (EQ first, later compressor, delay, ...) with the audio-analyzer "big 6" and extensions.
2. Run several plugins in parallel on identical stimuli and compare them.
3. Find parameter settings of plugin B (C, D, ...) that match a master setting of plugin A, or a measured target.
4. **Teaching is a main goal:** show that different tools do the same thing when adjusted correctly, and teach a method for finding out *why* they differ (the "audio detective").
5. Real-time listening (A/B/residual) so students hear the differences.

## 2. Decisions carried over from the prototype (all **DECIDED**)
| Topic | Decision |
|---|---|
| Framework | Python + pedalboard as plugin host behind a thin host interface; no own VST host. A second backend (DawDreamer or a JUCE command-line host) only when a real limit is hit. |
| Big 6 | Level/gain, frequency response, THD+N, noise/SNR, phase, crosstalk; the extensions (IMD, aliasing, latency, dynamics, delay, null test, linearity) are welcome. |
| Platforms | Linux is the main development platform; Windows and macOS must work for students; CI with GitHub Actions on all three. |
| Test objects | Free VST3 EQs first (open source not required); own plugins from `~/AudioDev` included; VST2 only if needed; commercial plugins optional, for case studies (tested on Windows). |
| Real time | Required (students must hear differences); offline rendering + GUI first, real-time host for listening and the detective. |
| Licence | Code GPLv3; teaching material CC BY-SA 4.0. |
| Audience | Basic DSP knowledge, some Python; the tool must be usable with a GUI, the backend stays accessible. |
| GUI | PySide6/Qt. |
| Order of plugin types | EQ, then compressor, then delay. Mono/stereo first, multichannel/sidechain later. |
| "Equal" | Technical first: frequency response within 0.1 dB, null depth below -60 dB, in a settings file; perceptual models (PEAQ, PEMO-Q, Dau 2003) later. |
| Test audio | MusicRadar SampleRadar free samples (check licences; download scripts instead of storing audio in the repo). |

## 3. What the prototype taught and what changes
Details and evidence: [docs/prototype/LESSONS_LEARNED.md](docs/prototype/LESSONS_LEARNED.md), [docs/prototype/findings.md](docs/prototype/findings.md).

1. **Plugins are not functions "parameters in, audio out".** Parameters set before the first process call can be lost, applied at the wrong sample rate (44.1 kHz), delivered only after a later change, smoothed, or can poison the filter state (NaN, silence). Block-size and latency effects came from the host. => The host layer comes first and has an explicit *parameter delivery protocol* (prime, deliver, settle, render, flush) and a *self-test per plugin* (target, target, alternative, target must give A, A, B != A, A; most conservative strategy first). A repeatable result is not a correct result.
2. **Plugin fingerprint as the first deliverable per plugin:** parameters and ranges, reported and measured latency, delivery strategy, determinism, behavior at 44.1/48/96 kHz, block-size independence, reachable knob ranges.
3. **Ground truth before real plugins:** numpy reference processors and analytic tests for every measurement.
4. **A general EQ band model** (type, frequency, gain, Q, slope, phase behavior) separated from per-plugin mapping files; a tool that dumps all parameters of a plugin and proposes a mapping. The "one peaking band" model of the prototype was too narrow (shelves, high/low-pass, multiple bands, fixed-Q bands).
5. **Architecture rules that worked:** everything that loads a plugin runs in a worker process; device-independent audio engine; configuration in YAML.
6. **Experiments:** save results incrementally, repeat fits with several seeds, state the band a test covers, separate knob-range limits from filter behavior, and report what the data does not show.
7. **Do not conclude from the first measurement of a new plugin:** two early "findings" were host artifacts; check against a second strategy (and the source code where available).

## 4. Work packages and milestones (proposal, each with a definition of done)
| # | Work package | Done when |
|---|---|---|
| W1 | **Host layer + fingerprint** (host interface, pedalboard backend, delivery protocol, self-test, worker-process runner, fingerprint report) | Fingerprint of all test plugins; delivery strategy per plugin recorded and reproducible; block-size and sample-rate checks pass; documented failure cases (crash on load, NaN). |
| W2 | **Reference plugins + stimuli + measurement set** (sweep response, phase, group delay, level, THD+N, noise/SNR, crosstalk, latency) | Every measurement agrees with analytic ground truth within a stated tolerance; the band of validity is documented. |
| W3 | **EQ band model + mapping tool** | A plugin is described by a mapping file generated/validated by the tool; shelves, filters and multi-band plugins covered; knob ranges and tapers calibrated by measurement. |
| W4 | **Matching** (descriptors, optimizer, multi-seed, residual explanation) | A master setting is reproduced on each test plugin or the residual is quantified and attributed (knob limit, filter shape, ...). |
| W5 | **Compare + detective** (alignment, null test, linear/nonlinear separation, checklist of causes) | Documented case studies with diagnosis path (e.g. bilinear-transform cramping, wrong sample rate, doubled gain). |
| W6 | **GUI + real-time listening** | A/B/residual switching, live and rendered, level match, tolerance display; listened to and judged by a person. |
| W7 | **Teaching material** (notebooks, exercises, case-study library) under CC BY-SA 4.0 | Notebooks run on all three OSes with the free plugin set. |
| W8 | **CI per OS with plugins installed** | Plugin tests run on Linux, Windows, macOS. |
Later: compressor (static curve, attack/release, detector), delay, perceptual metrics, multichannel/sidechain.

## 5. Open questions
1. Repository/package name (proposal: `pluginlab`).
2. Should the open PeakEQ question (frequency knob 1.0875 too high; 44.1 kHz design?) be the first test case for the fingerprint?
3. Which plugins form the fixed test set (13 free/own EQs in the prototype, `docs/prototype/plugin_shortlist.md`; which to drop, which to add)? Air-G EaseQ still needs a manual download (Cloudflare).
4. How to get the free plugins onto Windows/macOS CI runners and onto students' machines.
5. Sample-rate policy for tests (48 kHz default; 44.1 and 96 kHz in the fingerprint).
