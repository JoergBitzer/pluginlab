#!/bin/bash
# Runs the CTest suite in a virtual display (Xvfb) with a window manager (openbox; hosted VST2 editors need one), so that no test window
# appears on the desktop. Needs xvfb and openbox (sudo apt install xvfb openbox). Arguments are passed to ctest, e.g.
#   tools/ctest_offscreen.sh -R PluginLabHost
#   tools/ctest_offscreen.sh -j10     (the author allows 2/3 of the 16 cores; tests with windows hold the lock "display")
set -e
BUILD_DIR="${BUILD_DIR:-build}"
CONFIG="${CONFIG:-Debug}"
env -u WAYLAND_DISPLAY xvfb-run -a bash -c "openbox >/dev/null 2>&1 & sleep 2; ctest --test-dir \"$BUILD_DIR\" -C \"$CONFIG\" --output-on-failure $*"
