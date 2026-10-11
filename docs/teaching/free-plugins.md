# Free plugins to download (list for the manual)

The training material uses only pluginlab's own reference models and deliberately wrong test plugins (planning, "Test objects of the project",
decided 2026-10-09). The manual points readers to these pages, where they can download open-source or free plugins themselves to try the tools on.
None of these plugins is part of the repository or of CI.

| Plugin | Developer | Licence | Systems | Download | What pluginlab found (local runs) |
|---|---|---|---|---|---|
| ZeroEQ | Jun Murakami | AGPL-3.0 | Windows, macOS, Linux | [jun-murakami.web.app/apps/zeroeq](https://jun-murakami.web.app/apps/zeroeq) (direct download for all systems) | 1.0.7 (Linux, VST3): minimum-phase EQ, zero latency; bit-transparent at its defaults; parameter changes settle in 32 ms; the same normalised setting gives very different responses at 44.1, 48 and 96 kHz (knob ranges probably tied to the sample rate: a case for the calibration, W8.6); editor 760 x 540, not resizable (`docs/fingerprints/ZeroEQ.md`, `docs/measurements/real-plugins-2026-10-09.md`) |
| Hyperion EQ | Joachim Michaelis (Robotplanet; the site's author, who writes that he made the plugins) | freeware ("free VST and AudioUnit plugins"; no licence stated, no source) | Windows VST 32/64-bit (v311), macOS AU and VST 64-bit (v313), Linux VST x86_64 (v322) | [robotplanet.dk/audio_plugins/hyperion_eq](https://www.robotplanet.dk/audio_plugins/hyperion_eq/) (direct ZIP / 7z, no registration) | not measured yet. The page: an EQ "with lots of character", per-band coloration "similar to how some analog EQs behave" (saturation, compression, overdrive only in each band's frequency range), per-band solo, oversampling. Expected: not LTI (the system class, W8.1), so a teaching example for "the frequency response is valid only at one level". The page says "VST" only: probably VST2, which pluginlab hosts only in a build with VST2 support (`PLUGINLAB_VST2`) |

Added on the author's request (2026-10-11): ZeroEQ, Hyperion EQ (facts from the developers' pages on that day). Before the manual is published, each link and licence is checked again.
