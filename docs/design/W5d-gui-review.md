# W5d: GUI review aids on the Developer page (design and progress)

Status: **W5d.1 ... W5d.3 done on Linux** (2026-10-09, 0.37.0 and 0.38.0; the author's idea of 2026-10-09 in planning §8, row W5d; "You can also start with 5d (the GUI fingerprint)").
Goal (planning): the plugin's editor rendered offscreen and shown to the developer in variants that make GUI problems visible at once: (1) grayscale,
(2) zoom (smallest and largest size, scale factors 1, 1.5, 2), proposed additions (3) colour-vision simulation, (4) a low-contrast map, (5) scale-factor
honesty, (6) editor robustness, (7) GUI load, (8) accessibility. The risk named in the plan: on Linux a hosted plugin's editor is a native child window
(XEmbed for VST2, an X11 child for VST3); JUCE's component snapshot may not capture it; then the virtual display has to be grabbed instead. "Test this
first."

## Sources
- WCAG 2.x (W3C, Web Content Accessibility Guidelines): relative luminance and contrast ratio, $(L_1 + 0.05)/(L_2 + 0.05)$; luminance weights of
  ITU-R BT.709 on linear sRGB.
- G. M. Machado, M. M. Oliveira, L. A. F. Fernandes, "A physiologically-based model for simulation of color vision deficiency", IEEE Transactions on
  Visualization and Computer Graphics 15(6), 2009, pp. 1291-1298: the simulation matrices (severity 1.0) for protanopia, deuteranopia, tritanopia,
  applied to linear RGB.
- IEC 61966-2-1 (sRGB): the transfer function between sRGB and linear RGB.

## Steps
| Step | Content | Done when |
|---|---|---|
| **W5d.1** | **Capture and the risk test**: `PluginLabHost --gui-snapshot <plugin file> <folder>` opens the editor as a window of its own (in a virtual display), captures it with JUCE's `createComponentSnapshot` and, on Linux, the X window (`xwd`, ImageMagick `convert`); scale factors 1, 1.5, 2; smallest and largest size of a resizable editor; which capture has content | The capture works for the reference plugins (JUCE editors, hosted as VST3) and for two real plugins; the result of the risk test is documented. |
| **W5d.2** | **Vision variants and contact sheets** (`pluginlab_hosting_ui`, `GuiReview.h`): grayscale (relative luminance), protanopia, deuteranopia, tritanopia (Machado 2009), low-contrast map (colour edges with luminance contrast below 1.5:1), contact sheets of the variants and of the sizes | Unit tests of the colour science (white stays white, WCAG values, red and green merge under protan/deutan but not tritan, the map finds a colour-only edge); the sheets for the reference plugins. |
| W5d.3 | **Scale-factor honesty and size limits** in the summary: does the editor follow the host's scale factor (size and content), is it resizable, its limits | Reported per plugin; checked with a JUCE plugin that supports scaling and one that does not. |
| W5d.4 | **Editor robustness**: open and close ten times, open while audio runs (leaks, crashes, dropouts) in a child process | A crashing editor (test plugin) is reported, not fatal. |
| W5d.5 | **GUI load**: repaints and CPU while nothing changes (idle editor) | Measured for the reference plugins and two real plugins. |
| W5d.6 | **Accessibility** (JUCE plugins): titles, roles and focus order of the accessibility tree | Reported for the reference plugins. |
| W5d.7 | **Developer page**: a "GUI review" button per plugin (child process, like the report) and a view of the contact sheets and the summary | The author can review a plugin's GUI from the host. |

Steps W5d.1 and W5d.2 are the start (this commit); W5d.3 is partly in it (sizes, scale-factor reaction measured and reported).

## Progress
### W5d.1 and W5d.2: capture, vision variants, contact sheets (0.37.0, done on Linux)
`PluginLabHost --gui-snapshot <plugin file> <folder> [<plugin identifier>]` (`apps/host/HostGuiSnapshot.cpp`): the plugin is created with the GUI formats,
its editor (or the host's generic editor) is put on the desktop as a window of its own at (0, 0), the message loop runs 1.5 s, then two captures:
`createComponentSnapshot` and, on Linux, the X window by `xwd -id <window>` and ImageMagick `convert`. Then the scale factors 1.5 and 2
(`AudioProcessorEditor::setScaleFactor`) and, for a resizable editor, half and twice the default size (or the limits of its constrainer, if it reports
real ones). The image functions are in `pluginlab_hosting_ui` (`src/ui/GuiReview.cpp`): the variants, the low-contrast map (colour edges whose luminance
contrast is below 1.5:1), contact sheets, the content share of a capture. Output: the single captures, `variant_*.png`, `low_contrast_map.png`,
`contact_sheet_vision.png`, `contact_sheet_sizes.png` and `gui_review.md`. Tests: `tests/GuiReviewTests.cpp` (colour science) and the CTest
`PluginLabHostGuiSnapshot` (Linux, in the virtual display of `tools/ctest_offscreen.sh`) on the Reference EQ.

**The risk test, answered:** JUCE's component snapshot of a hosted editor is **empty** (0.0 % of the pixels differ) for the Reference EQ hosted as
VST3: the plugin draws into a native child window that the snapshot does not see. The **X window capture works** (9 % of the pixels differ from the
background colour: the drawing). The tool therefore uses the X capture when the snapshot is empty. Windows and macOS need their own native capture
(not done; the JUCE snapshot is tried there and will likely be empty too).

**First findings on our own Reference EQ (W6.4), from its contact sheets** (the kind of result the tool is for):
- the footer text is clipped at the right edge at every size ("... no smoothing, latency" ends in the middle of a word);
- the editor follows the host's scale factor (960 x 600 at 1.5, 1280 x 800 at 2, everything scaled);
- resized to twice its size, only the graph grows; knobs, labels and fonts keep their size (a layout that does not scale);
- asked for half its size it stays at 640 x 400 (its real minimum), while the hosted wrapper's constrainer reports no limits (0 ... 2^30);
- the low-contrast map marks the gray dB labels and grid of the graph (gray on gray, 11.4 % of the colour edges below 1.5:1); in grayscale and the three
  colour-vision simulations everything stays readable: no information is carried by colour alone.
These are review items for the Reference EQ's editor, not fixed here.

Not done yet: W5d.4 ... W5d.7, real plugins (the
author's plugins are not used without asking), Windows and macOS capture. The X server prints "BadWindow" errors when the hosted editor closes; harmless
(the program exits normally), noted for W5d.4.

### W5d.3: scale-factor honesty and resizing (0.38.0, done on Linux)
For the scale factors 1.5 and 2 the review reports the size, the size ratio and the **content similarity**: the capture reduced to the default size
against the default capture, the correlation coefficient of their luminance (`ui::getImageSimilarity`). The judgement (`ui::judgeScaling`): **follows**
(size by the factor within 3 %, similarity at least 0.7), **size only** (size by the factor, content not scaled uniformly: kept at its size or
re-laid out), **ignores** (size unchanged), **partly** (size changed, not by the factor), **not judged** (the window is larger than the screen, so the
capture is incomplete). Resizing: the content similarity at twice the size tells whether the content zooms or the layout changes. The results also go
into `gui_review.json` (for the Developer page, W5d.7). Since 0.38.0 the review leaves the editor at the size it opened with (see below).

**How JUCE scales** (found on the way): JUCE 9's VST3 wrapper does not call `AudioProcessorEditor::setScaleFactor` for the host's scale factor; its
scale manager sets it as the platform scale of the editor's peer (`ComponentPeer::setCustomPlatformScaleFactor`), so every JUCE editor follows the
host's scale factor without doing anything. Test editors that do not follow therefore have to undo it on purpose: the test plugins
`PluginLab Test Editor Scales` (follows), `... Ignores Scale` (shrinks itself by the peer's platform scale) and `... Size Only` (draws with the inverse
scale) (`tests/plugins/TestPlugin.cpp`, editor modes 1 ... 3, an optional 17th argument of `pluginlab_add_test_plugin`).

**Calibration of the threshold** (similarity at scale 2): the test editor that follows 0.995, the Reference EQ 0.895 (fine text and 1-pixel lines do not
reduce exactly), the size-only editor 0.0; hence 0.7. Tests: `GuiReviewTests` (similarity of an image with itself, with its enlargement, with a drawing
that kept its size; the four judgements) and the CTest `PluginLabHostGuiScaling` (the three test editors judged correctly), Linux.

**Real EQs** (second run, scale 2): Shape it, FreeEQ8 (0.78), Pult EQ, ZL Equalizer 2, PeakEqualizer, EQoder follow; WSTD MSEQ ignores the scale
factor; ZeroEQ (0.28) is re-laid out rather than zoomed (the graph grows more than the knob rows: "size only" in the strict sense of the judgement);
the own PeakEQ (0.49) does not scale uniformly either. For 4K EQ, Multi-Q and WayQ the second run said "size only", but that is **invalid**: their
windows were larger than the virtual screen (see the side effect below); the review now marks such captures "not judged".

**A side effect of the first run (fixed in 0.38.0):** the review resized editors to twice their size and closed them in that state; 4K EQ, Multi-Q,
WayQ and ZL Equalizer 2 store their editor size, so they opened at twice their default size in the second run (and in the author's DAW until resized;
the author resizes them by hand). At scale 2 these windows were larger than the virtual screen (2560 x 1600) and were captured only in part. The review now
restores the opening size and scale before it closes the editor.

**Windows and macOS** (the author: "show-stoppers for this feature"; to be tried in CI before going on): Windows `PrintWindow` with
`PW_RENDERFULLCONTENT` (renders DirectX/OpenGL child windows since Windows 8.1) on the host's window, fallback Windows.Graphics.Capture; macOS in-process
`NSView cacheDisplayInRect` (no permission needed; Metal/OpenGL layers may come out empty), ScreenCaptureKit only locally (needs the Screen Recording
permission).

### W5d.4 and W5d.5: robustness and GUI load (0.40.0, done on Linux)
After the captures the review runs three more steps on the same instance (`apps/host/HostGuiSnapshot.cpp`; CPU time and resident memory per platform in
`apps/host/ProcessStats.cpp`: Linux `/proc/self/statm` and `getrusage`, macOS `task_info` and `getrusage`, Windows `GetProcessMemoryInfo` and
`GetProcessTimes`):
- **GUI load** (W5d.5): the process CPU time over 3 s with the editor open and idle, minus the same 3 s without it, in % of one core.
- **Open and close ten times** (W5d.4): the resident memory after the first opening and its growth per further opening; more than 5 MB per opening is
  reported as a probable leak.
- **Audio while the editor opens and closes** (W5d.4): noise through `processBlock` at real-time pace in a thread of its own (blocks of 512 at 48 kHz,
  10.67 ms), 2 s alone and 2 s while the editor opens and closes; the blocks that took longer than they last, the longest block, non-finite output.
- **Progress file**: before every step the review writes the step into `progress.txt`; a plugin that crashes ends the process (the caller, the
  Developer page in W5d.7, runs the review in a child process) and the file names the step. Results also in `gui_robustness.json`.

Deliberately wrong test editors (`tests/plugins/TestPlugin.cpp`, editor modes 4 ... 7) and what the review finds (Linux, Xvfb):

| Test editor | Built to | Found |
|---|---|---|
| PluginLab Test Editor Crashes | crash when it opens | exit 139 (segmentation fault), progress "opening the editor" |
| PluginLab Test Editor Leaks | keep 20 MB per opening | 41.95 MB per opening, "probable leak" (twice the 20 MB: probably two editor instances per opening on the plugin side; not verified) |
| PluginLab Test Editor Busy | repaint at 60 Hz, 10 ms of work per paint (60 % of a core) | idle load 51.0 % |
| PluginLab Test Editor Blocks Audio | hold the lock of `processBlock` for 30 ms at 30 Hz in its paint | 40 of 156 audio blocks late, longest 27.8 ms (alone: 0 late, 0.47 ms); idle load 74.6 % |
| PluginLab Test Editor Scales (correct) | - | 0 late blocks, 0.00 MB per opening, 1.5 % load |

CTest `PluginLabHostGuiRobustness` (Linux) checks these five. The audio test uses the editor's own instance: the situation of a DAW (the editor and
the audio of one plugin at the same time). A plugin whose GUI blocks its audio in a DAW causes dropouts only when the editor is open; this test makes it
visible without a DAW.

### The capture on Windows and macOS: CI trial (0.39.0, run 37990824875 of the workflow "GUI capture trial", 2026-10-09)
The author called the Windows and macOS capture the show-stoppers of the feature; they were tried in CI before going on
(`.github/workflows/gui-capture.yml`, started by hand, no tag). `apps/host/NativeWindowCapture_*`: Linux `xwd`, Windows `PrintWindow` with
`PW_RENDERFULLCONTENT`, macOS `NSView cacheDisplayInRect` in the process.

**Result: the capture works on both.** As on Linux, JUCE's component snapshot of a hosted editor is empty (0.0 %) on Windows and macOS, and the native
capture has the editor (Reference EQ: 9.5 % of the pixels differ from the background on macOS, the same picture as on Linux; the test editors 50.7 ...
50.9 %). The images (artifacts of the run) show the complete Reference EQ editor on both systems.
- **Windows**: the scale factor judgements equal Linux: the test editors follow / ignore / size only (similarity 0.993 / 1.000 / -0.002); the Reference
  EQ follows at 1.5 (0.888) and is "not judged" at 2 (1280 x 800 is larger than the runner's screen: the screen check works).
- **macOS**: every editor stays at its size for the host's scale factors: macOS has no host scale factor for VST3 editors (JUCE's wrapper refuses
  `setContentScaleFactor` there; the system scales by the backing scale). Since 0.40.0 the review says "not applicable" on macOS instead of "ignores".
- Not tried: plugins that draw with Metal or OpenGL on macOS (the in-process `NSView` cache may miss them; ScreenCaptureKit would need the Screen Recording
  permission), DirectX editors on Windows (what `PW_RENDERFULLCONTENT` is for).
