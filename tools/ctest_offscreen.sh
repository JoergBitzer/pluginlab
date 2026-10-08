#!/bin/bash
# Runs the CTest suite in a virtual display (Xvfb) with a window manager (openbox; hosted VST2 editors need one), so that no test window
# appears on the desktop. Needs xvfb and openbox (sudo apt install xvfb openbox). Arguments are passed to ctest, e.g.
#   tools/ctest_offscreen.sh -R PluginLabHost
set -e
BUILD_DIR="${BUILD_DIR:-build}"
CONFIG="${CONFIG:-Debug}"
env -u WAYLAND_DISPLAY xvfb-run -a bash -c "openbox >/dev/null 2>&1 & sleep 2; ctest --test-dir \"$BUILD_DIR\" -C \"$CONFIG\" --output-on-failure $*"
