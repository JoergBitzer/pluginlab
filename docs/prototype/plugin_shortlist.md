# EQ plugin shortlist (M0)

Criterion (relaxed 2026-10-02): the test objects must be **free**, not necessarily open source. VST3 required. All three OSes needed for the student/CI setup.

## Spike result on this machine (Linux, pedalboard 0.9.25, 48 kHz) – `spikes/m0_spike.py`

| Plugin | Origin | Loads | Parameters | Deterministic | Block size 512 = full | Impulse delay |
|---|---|---|---|---|---|---|
| PeakEQ (AAT_EQ1) | own | yes | see spike log | yes | identical | 96 samples |
| PeakEqualizer (AAT_PeakEqualizer) | own | yes | gain_db, q, freq_hz, bypass | yes | identical | 96 samples (peak 1.078) |
| EQoder | own | yes | 19 (filter-bank EQ) | yes | identical | 95 samples |
| BL-EQHack | own | yes | bypass, mode, monitor (not a parametric EQ) | yes | identical | 0 |
| LSP Parametric Equalizer (x8/x16/x32) | open source (LSP, GPLv3) | **crashes on load** (bus error inside `load_plugin`, with and without display) | – | – | – | – |

Observations to follow up in M1:
- The impulse peak delay of 96 samples in the own EQs is a measured value; whether it is reported latency, oversampling filter delay or lookahead is **not yet checked** (pedalboard does not expose reported latency).
- LSP: the failure is in loading the bundle (190 plugins in one `.vst3`), cause unknown. Next steps: other pedalboard version, load the LV2 build with another tool, or try a second backend. Until then LSP is not usable as a test object here.

## GitHub search (2026-10-02): topics `vst3` / `vst` + equalizer/eq/parametric
Search by topic and name, sorted by stars. The release assets column was read from the GitHub releases; **VST3 content of each archive, license text and sound/behavior are not yet checked**.

| Repo | Stars | License | Release binaries (Linux/Win/macOS) | Why interesting |
|---|---|---|---|---|
| ZL-Audio/ZLEqualizer | 1024 | AGPL-3.0 | all three | Dynamic EQ with linear/min/mixed phase: rich "detective" case |
| ffAudio/Frequalizer | 375 | BSD-3 | none (build from source, JUCE dsp module) | Simple 6-band, JUCE dsp filters, good reference |
| tobanteAudio/modEQ | 83 | GPL-3.0 | all three | Parametric EQ with modulation (time-variant case) |
| brummer10/ToneShiftEQ | 83 | BSD-3 | Linux VST3 (Win: CLAP only) | 12-band, Linux-focused |
| consint/Pult-EQ | 51 | GPL-3.0 | Linux, Windows (no macOS) | Pultec-style tube EQ: analog-modeled, nonlinear |
| GareBear99/FreeEQ8 | 48 | GPL-3.0 | all three | 8-band parametric |
| yonie/WetEQ | 20 | MIT | one zip (platform unclear) | 4-band British console EQ, circuit model |
| nathanjhood/Biquads | 19 | GPL-3.0 | all three (JUCE 7.0.9) | Plain two-pole biquad EQ with variable oversampling: ideal for aliasing/oversampling lessons |
| Jun-Murakami/ZeroEQ | 18 | AGPL-3.0 | all three | Zero-latency (minimum-phase) EQ |
| hollance/ThreeBandEQ | 8 | MIT | none | Very simple bass/mid/treble, easy to understand |
| egor-sm/equalize_it | 63 | GPL-3.0 | Windows installer only | 12-band; not usable on Linux |
| Others (not looked at) | | | | AmeHanare/OctaEQ, gusmrocha/ParamEq, mantisvex/mantisvex-Q, lukeglad/EQ, stoatworks-labs/zero-eq, Wasted-Audio/wstd-eq, GiorgosChr/MultiBandEQ |

Not EQ plugins but relevant: **sergree/matchering** (GPL-3.0, 2.6k stars) matches the sound of a reference track to a target. Different goal (mastering match), but a useful comparison for our own matching idea.

Free (closed source) VST3 EQs seen on https://freevsthub.com/eq/ (no platform/license data there): Vitric Bevel EQ, Dusk Audio Multi-Q / 4K EQ 2, Delos DEQ, ViatorDSP Radiant Q, Tonelib BaxEQ, Sonimus SonEQ, Kuassa BasiQ 2, Little Audio Little EQ, TBProAudio sTilt, Kage KG series. TDR Nova (free, closed) from prior knowledge.

## Proposed selection for the spike (to be confirmed by running them)
- **Linear / textbook:** Biquads (nathanjhood), Frequalizer (build), ThreeBandEQ (build)
- **Feature-rich modern:** ZLEqualizer, modEQ, FreeEQ8, ZeroEQ
- **Analog-modeled (nonlinear, detective cases):** Pult-EQ, WetEQ
- **Own:** PeakEqualizer, PeakEQ, EQoder

## Proposed first test set (own plugins)
1. PeakEqualizer (own, 1 band, simplest; ground truth known from source)
2. PeakEQ (own, same job, different implementation) – first "same but different" pair
3. EQoder (own, multiband)
4. One or two third-party free VST3 EQs with Linux builds (to be selected from the candidates above)

## Spike result for the downloaded third-party plugins (2026-10-02, Linux, pedalboard 0.9.25; log: `docs/spike_results.log`)

| id | Plugin | Loads | Parameters | Deterministic | Block 512 = full | Impulse delay | Notes |
|---|---|---|---|---|---|---|---|
| zleq2 | ZL Equalizer 2 1.4.1 (AGPL-3.0) | yes | 609 | yes | yes | 0 | many choice parameters (filter structure: minimum/matched/zero phase ...) |
| zeroeq | ZeroEQ 1.0.7 (AGPL-3.0) | yes | 71 | yes | 7.5e-9 | 0 | |
| freeeq8 | FreeEQ8 2.3.1 (GPL-3.0) | yes | 129 | yes | yes | 0 | |
| biquads | Biquads 1.2.2 (GPL-3.0) | yes | 9 | yes | yes | 0 | oversampling and transform-structure choices |
| pulteq | Pult-EQ 1.0.0 (GPL-3.0) | yes | 29 | **no** (6e-8) | **no (0.65 max difference)** | 0 | analog model; block-size dependence is a finding to investigate |
| modEQ 0.4.0 | – | – | – | – | – | – | Linux zip contains only a standalone binary and a static library, no VST3: dropped |

All licences were taken from the license files in the archives (GPLv3 / AGPLv3). Choice parameters (enums) needed support in the host layer (`ParameterInfo.choices`).

## Findings from M1 (measurements on real plugins)
- **PeakEqualizer (own) = RBJ peaking filter**: the swept magnitude, phase and group delay overlay the RBJ reference exactly (`examples/m1_measure.py`, plot in `renders/`). Its 96-sample delay is the template's synchronous block size (`m_Latency += synchronblocksize`), i.e. reported latency, not filter delay.
- **Parameter pick-up bug of the PeakEqualizer template**: `prepareToPlay` hard-codes f0 = 4 kHz, Q = 9, gain = 20 dB and `updateWithNotification` only re-reads a parameter when its value *changes*. A parameter set before the first process call works; on every later call the plugin runs with the hard-coded values although `get` still returns the set value. `PedalboardHost` detects such plugins (two identical renders differ) and reloads a fresh instance before each render (`reload="auto"`). This is also a good teaching example for "measure, don't trust the knob".

## Second batch (links from KVR, 2026-10-02)
Linux builds were downloaded, unpacked into `plugins/ext/` and run through the same spike (`spikes/m0_spike2.py`); macOS and Windows archives were only downloaded (`plugins/dl_other/`), not opened or tested. `tools/fetch_plugins.sh` repeats all downloads. Licences are the ones stated on the KVR pages; only SAFE ships a licence file (GPLv3).

| Plugin | Linux | macOS / Windows downloaded | Test result (Linux, pedalboard) |
|---|---|---|---|
| Dusk Audio Multi-Q 0.10.9 (free; GitHub dusk-audio/plugins) | VST3 | yes / yes | loads, 191 parameters, deterministic; block-size independent after the host fix (the first result, a difference of 0.6, was a host bug, see `docs/findings.md`) |
| Dusk Audio 4K-EQ 1.0.12 (free; same repo) | VST3 | yes / yes | loads, 26 parameters, deterministic; block-size independent after the host fix; output 0.98 of input rms at default |
| WayQ 1.0.0 (GPL-3.0; GitHub rcrath/wayq) | VST3 | yes (pkg) / yes | loads, 29 parameters, deterministic, block-size independent; default output is not flat (rms ratio 0.904, peak 0.687) |
| ToneLib BaxEQ 1.0.0 (free, closed) | VST3 (from the .deb) | yes (pkg) / yes | loads, 16 parameters, deterministic, block-size independent |
| WSTD MSEQ 1.0.1 (your local download) | VST3 | not downloaded | loads, 10 parameters (mid/side EQ: `m_high_db`, `m_low_db`, `m_mid_db`, `m_mid_freq_hz`, ... and buffer/sample-rate parameters); impulse peak only 0.027, delay +2 samples: probably a mid/side or crossover structure, needs a closer look |
| SAFEEqualiser 1.32 (GPL-3.0; GitHub semanticaudio/SAFE) | **VST2 only** (`.so`) | yes (OSX zip, Windows VST 64 bit) | **not testable**: pedalboard hosts VST3 only on Linux |
| Air-G EaseQ 1.0 (free, MIT-derived from Airwindows) | VST3 | **not downloaded** | the only download is on ko-fi (https://ko-fi.com/s/acb81e62e3), which answers scripted requests with a Cloudflare challenge (403). I did not try to get around it: please download the three archives by hand into `~/Downloads/` |
| Harrison XT-EQ 3.7 | LV2 only | not downloaded | **not testable** (LV2, and not VST3); KVR's download page is behind a Cloudflare challenge, and the plugin is listed as key-file protected from Harrison's store |

Peak-band mappings (`configs/peak_band.yaml`) exist for Multi-Q (band 2), WayQ (mid band) and 4K-EQ (high-mid band); the matching and the GUI use them. BaxEQ (shelving tone control) and WSTD MSEQ (no Q control) have none. All new plugins are in `configs/plugins.yaml`, so the big-6 runner measures them.
