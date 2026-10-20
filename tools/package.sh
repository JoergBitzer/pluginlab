#!/usr/bin/env bash
# package.sh -- the archives of the two products (W10.8): the plugin Jade Benchmark and the stand-alone application PluginLabHost.
#
# Usage:  tools/package.sh <build folder> <version> <platform label> <output folder>
#         e.g. tools/package.sh build 0.70.0 linux-x86_64 dist
#
# Writes <output>/JadeBenchmark-<version>-<platform>.zip (JadeBenchmark.vst3 with its PluginLabScanner and PluginLabHost inside; on macOS
# also JadeBenchmark.component) and <output>/PluginLabHost-<version>-<platform>.zip (the application with PluginLabScanner next to it;
# on macOS PluginLabHost.app, the scanner inside), each with LICENSE, THIRD_PARTY.md and the installation notes of docs/manual.
# Release artefacts are taken if they exist, else Debug. Linux, macOS and Windows (Git Bash on the CI runner).

set -euo pipefail

if [ $# -ne 4 ]; then
    echo "Usage: $0 <build folder> <version> <platform label> <output folder>"
    exit 2
fi
build=$1
version=$2
platform=$3
output=$4
root=$(cd "$(dirname "$0")/.." && pwd)

mkdir -p "$output"
output=$(cd "$output" && pwd)
staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT

config=Release
if [ ! -d "$build/apps/host/PluginLabHost_artefacts/Release" ]; then
    config=Debug
fi
host_dir="$build/apps/host/PluginLabHost_artefacts/$config"

common_files() {
    cp "$root/LICENSE" "$1/"
    cp "$root/THIRD_PARTY.md" "$1/"
    cp "$root/docs/manual/01-installation.md" "$1/INSTALL.md"
}

zip_folder() {
    # cmake -E tar writes zip files on every platform (no zip program needed on the runners)
    (cd "$staging" && cmake -E tar cf "$output/$2" --format=zip "$1")
}

# the plugin
plugin="JadeBenchmark-$version-$platform"
mkdir -p "$staging/$plugin"
cp -R "$build/benchmark_plugin/JadeBenchmark.vst3" "$staging/$plugin/"
if [ -d "$build/benchmark_plugin/JadeBenchmark.component" ]; then
    cp -R "$build/benchmark_plugin/JadeBenchmark.component" "$staging/$plugin/"
fi
common_files "$staging/$plugin"
zip_folder "$plugin" "$plugin.zip"

# the application
application="PluginLabHost-$version-$platform"
mkdir -p "$staging/$application"
if [ -d "$host_dir/PluginLabHost.app" ]; then
    cp -R "$host_dir/PluginLabHost.app" "$staging/$application/"
else
    for file in PluginLabHost PluginLabHost.exe PluginLabScanner PluginLabScanner.exe; do
        if [ -f "$host_dir/$file" ]; then
            cp "$host_dir/$file" "$staging/$application/"
        fi
    done
fi
common_files "$staging/$application"
zip_folder "$application" "$application.zip"

ls -l "$output"
