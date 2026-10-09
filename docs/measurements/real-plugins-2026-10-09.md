# First run on real plugins: twelve EQs (2026-10-09, 0.37.0)

The author asked to try the new tools on the free EQs (2026-10-09). Each plugin was run through `PluginLabHost --measure` (all W7 units,
[plugins-and-host.md](plugins-and-host.md)) and `PluginLabHost --gui-snapshot` (W5d, `docs/design/W5d-gui-review.md`), each call in its own process with
a time limit, in a virtual display (Xvfb with openbox), Linux, 48 kHz. Plugin files: the free EQs downloaded by the prototype
(`measurement_tool/plugins/`) and the author's own EQs in `~/.vst3`; the same builds as in `docs/fingerprints/`. The Venn Free EQ of the fingerprints
is no longer on disk and was skipped. Images of the editors are not committed (third-party GUIs); the text results are below.

**All 24 runs ended normally** (no crash, no time-out): measurement 8 ... 26 s, GUI review 8 ... 22 s per plugin.

**Important limit: every plugin was measured at its default setting.** Most EQs are flat at their defaults (bands at 0 dB or off), so most rows
show "transparent". Measuring at chosen settings needs the band model and mapping of W8.

## Measurements (default settings)
| Plugin | Gain at 997 Hz | Response 20 Hz ... 20 kHz re 997 Hz | Delay (impulse peak) | THD+N at -1 dBFS | IMD (DFD / MD) | Idle noise (CCIR-RMS) | Crosstalk (worst) | Maximum input level |
|---|---|---|---|---|---|---|---|---|
| 4K EQ (Dusk Audio) | +0.009 dB | flat | **49 samples** | -145.2 dB | -154.6 / -157.2 dB | silent | **-60.0 dB** | none up to +24 dBFS |
| Shape it (Soundly) | 0.000 dB | flat | 0 | -153.7 dB | -162.0 / -165.7 dB | silent | silent | none |
| FreeEQ8 | 0.000 dB | flat | 0 | -153.7 dB | -162.0 / -165.7 dB | silent | silent | none |
| Multi-Q | 0.000 dB | flat | **60 samples** | -148.0 dB | -152.5 / -159.9 dB | silent | silent | none |
| WayQ | 0.000 dB | **-3.33 ... 0.00 dB** | 0 (phase delay at 100 Hz: -32.5 samples) | -143.8 dB | -152.8 / -154.9 dB | silent | below -200 dB | none |
| ZeroEQ | 0.000 dB | flat | 0 | -152.0 dB | -158.1 / -165.0 dB | silent | silent | none |
| Pult EQ | **+0.083 dB** | -0.01 ... +0.01 dB | 1 sample | **-35.4 dB** | **-33.4 / -30.2 dB** | **-152.7 dBFS** | -168.8 dB | **-4.77 dBFS** (+3 dB above: THD+N -36.3 dB, no rollover) |
| ZL Equalizer 2 | 0.000 dB | flat | 0 | -153.7 dB | -162.0 / -165.7 dB | silent | silent | none |
| WSTD MSEQ | **-0.257 dB** | **-0.01 ... +0.37 dB** | 1 sample (phase delay 0.73) | -125.7 dB | -135.9 / -153.6 dB | silent | silent | none |
| PeakEQ (own) | 0.000 dB | flat | **96 samples** | -118.0 dB | -126.2 / -151.5 dB | silent | silent | none |
| PeakEqualizer (own) | 0.000 dB | flat | **96 samples** | -153.7 dB | -162.0 / -165.7 dB | silent | silent | none |
| EQoder (own) | 0.000 dB | flat | **95 samples** | -153.7 dB | -162.0 / -165.7 dB | silent | silent | none |

"flat": within +-0.005 dB; "silent": exact zeros; THD+N near -153.7 dB and IMD near -162 dB are the float floor of a transparent path (the same
numbers as the test gain plugin). Dynamic range and gain non-linearity are in the single reports; every plugin is linear to within 0.001 dB down to
-140 dBFS except Pult EQ (0.19 dB at -140 dBFS, its noise).

**What the numbers say** (myth and reality at the defaults):
- **Pult EQ is not transparent at its defaults**: with all EQ knobs at 0 it adds harmonic distortion (THD+N -35 dB at -1 dBFS), intermodulation
  (-30 dB modulation distortion), noise (-153 dBFS CCIR-RMS) and a gain of +0.08 dB, and it reaches THD+N -40 dB already at -4.8 dBFS. The editor shows
  why: its default "Drive" is 3.0. An "analogue" colour that is on by default.
- **4K EQ leaks -60 dB between the channels at every frequency**: deliberate crosstalk (console emulation), plus 49 samples of latency (probably
  oversampling) and +0.009 dB of gain.
- **WayQ is not flat at its defaults**: the response falls by 3.33 dB somewhere in the band and the phase leads at 100 Hz (phase delay -32.5 samples),
  which points to a low cut that is on by default (not located further here).
- **WSTD MSEQ changes the level at its defaults**: -0.26 dB at 997 Hz and up to +0.37 dB elsewhere, with THD+N -126 dB (above the float floor: some
  processing in single precision or a small non-linearity).
- **Latency**: 4K EQ 49, Multi-Q 60, the own PeakEQ / PeakEqualizer 96 and EQoder 95 samples (PeakEQ's 96 samples were found in W5; it reports 0).
  The own PeakEQ also has the highest float noise of the clean paths (THD+N -118 dB).
- The other EQs (Shape it, FreeEQ8, ZeroEQ, ZL Equalizer 2) are bit-transparent at their defaults within the float precision.

## GUI review (default editor)
| Plugin | Size | Resizable | Follows the host's scale factor | JUCE snapshot / X capture | Low-contrast colour edges |
|---|---|---|---|---|---|
| 4K EQ | 950 x 655 | yes (no limits reported) | yes | empty / works | 8.8 % |
| Shape it | 720 x 400 | yes | yes | empty / works | 5.9 % |
| FreeEQ8 | 900 x 620 | yes | yes | empty / works | 10.1 % |
| Multi-Q | 1050 x 700 | yes | yes | empty / works | 12.9 % |
| WayQ | 1220 x 636 | yes | yes | empty / works | 16.7 % |
| ZeroEQ | 760 x 540 | no | yes | empty / works | 13.6 % |
| Pult EQ | 640 x 330 | no | yes | empty / works | 9.5 % |
| ZL Equalizer 2 | 600 x 371 | yes | yes | empty / works | 3.7 % |
| WSTD MSEQ | 228 x 537 | no | **no** (stays 228 x 537 at 1.5 and 2) | empty / works | 22.4 % |
| PeakEQ (own) | 200 x 400 | yes | yes | empty / works | 22.8 % |
| PeakEqualizer (own) | 100 x 300 | yes | yes | empty / works | 24.9 % |
| EQoder (own) | 660 x 400 | yes | yes | empty / works | 19.8 % |

- The risk test of W5d holds for every plugin: JUCE's component snapshot of a hosted editor is empty on Linux, the X window capture works.
- **WSTD MSEQ ignores the host's scale factor** (on a HiDPI screen it stays small) and is the one editor whose colour coding carries meaning: the
  bands are told apart by colour (High blue, Mid green, Low red); in grayscale the knobs become similar grays and under protanopia/deuteranopia Mid and
  Low become the same olive tone. The labels ("High", "Mid", "Low") keep it usable.
- Pult EQ: the inactive entries of its mode switches ("Off", "Mid", "Side") are gray on gray (the low-contrast map marks them).
- The author's own small editors (PeakEQ, PeakEqualizer, EQoder) have the largest share of low-contrast colour edges (20 ... 25 %): review items.
- The low-contrast share is a hint (it counts edges of colour changes whose luminance contrast is below 1.5:1, including decoration); a share alone
  is not a verdict.

## Next
- Measurements at chosen settings (a bell at 1 kHz +6 dB on every EQ, the comparison of the prototype) need the W8 mapping.
- The default-setting run is cheap (about 25 s per plugin with the GUI review) and could become part of the Developer page's "Generate report".
