#pragma once

#include <vector>

#include <juce_core/juce_core.h>

namespace pluginlab::engine
{
// The numbers the fingerprint uses: decision thresholds, signal levels and lengths. Kept in a JSON file the author can edit
// (~/.config/pluginlab/fingerprint_settings.json); every report prints the values it was made with.
struct FingerprintSettings
{
    // decisions (dB relative to the reference, see the report; for a silent reference the absolute difference in dBFS decides)
    double reactsAboveDb = -80.0;            // a parameter changes the audio
    double differentAboveDb = -60.0;         // B differs from A; a candidate parameter is kept in B
    double sameBelowDb = -80.0;              // two outputs count as the same
    double blockIndependentBelowDb = -100.0; // block size independent (steady state)
    double silentReferenceDbfs = -150.0;     // a reference below this level counts as silent: the absolute difference decides
    double couplingBelowDb = -100.0;         // the channels are independent if the silent one stays below this, relative to the driven one

    // signals and timing
    double noiseLevel = 0.1;                 // peak of the uniform white noise (linear)
    double settleSeconds = 0.25;             // silence before every measured signal
    int impulsePreDelaySamples = 4096;       // the impulse comes after this many samples of silence
    double latencyObserveSeconds = 1.0;      // how long the output of the impulse is watched
    double latencyMinimumPeak = 1.0e-4;      // below this peak "nothing came out"
    double blockRenderSeconds = 1.0;         // block size test: length of the noise
    double blockCompareSeconds = 0.1;        // block size test: the steady state is the end of the render
    std::vector<int> blockSizes = {32, 64, 128, 256, 1024, 2048, 509};

    // parameters
    double lowSetting = 0.25;                // the two test positions (normalised)
    double highSetting = 0.75;
    double pokeDistance = 0.4;               // the other value a parameter gets before its target
    int maximumParameters = 64;              // only the first ones are examined
    int maximumJumpedParameters = 16;        // in the jump test

    // The default place of the file: the application data folder of the user, or the file named by the environment variable
    // PLUGINLAB_FINGERPRINT_SETTINGS (tests use it, so that they never touch the user's file).
    static juce::File getDefaultFile();

    // Reads the file. A missing key keeps its default, unknown keys are ignored. Returns false (and the defaults) if the file is not valid
    // JSON; warning tells what happened. A missing file is written with the defaults.
    static FingerprintSettings loadOrCreate(const juce::File& file, juce::String& warning);

    juce::String toJson() const;
    bool save(const juce::File& file) const;
};
}
