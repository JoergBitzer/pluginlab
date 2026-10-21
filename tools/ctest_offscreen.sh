#!/bin/bash
# Runs the CTest suite in a virtual display (Xvfb) with a window manager (openbox; hosted VST2 editors need one), so that no test window
# appears on the desktop. Needs xvfb and openbox (sudo apt install xvfb openbox). Arguments are passed to ctest, e.g.
#   tools/ctest_offscreen.sh -R PluginLabHost
#   tools/ctest_offscreen.sh -LE long  (the quick run: without the long tests, a few minutes)
# The tests run side by side, 10 at a time (the author allows 2/3 of the 16 cores; CTEST_PARALLEL_LEVEL overrides it, -j too); tests with
# windows hold the lock "display", tests that measure time run alone (RUN_SERIAL).
set -e
BUILD_DIR="${BUILD_DIR:-build}"
CONFIG="${CONFIG:-Debug}"
export CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-10}"
env -u WAYLAND_DISPLAY xvfb-run -a bash -c "openbox >/dev/null 2>&1 & sleep 2; ctest --test-dir \"$BUILD_DIR\" -C \"$CONFIG\" --output-on-failure $*"
