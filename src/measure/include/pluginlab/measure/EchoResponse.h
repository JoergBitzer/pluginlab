#pragma once

#include <vector>

namespace pluginlab::measure
{
// The echo response of a delay (W11.1b, docs/design/W11-delay-plan.md 4.1): from the response to a unit impulse (sample 0 = the impulse, the
// whole tail: engine::renderImpulse divided by the impulse's height) the arrivals are found, the one at the latency is the direct (dry) path,
// the others are the echoes. Per arrival: the time (fractional) and the gain from the phase slope and the real part of its spectrum in a low band
// (where the interpolator of a fractional delay is still flat), the polarity. Derived: the delay time, the spacing of the echoes and how constant
// it is, the feedback (dB per repeat) and the decay of the train (dB per second), the mix (the first echo against the direct path), and the loop
// filter: the ratio of the spectra of successive echoes, the magnitude of one trip around the loop with its feedback gain.
struct EchoSettings
{
    double sampleRate = 48000.0;
    double latencySamples = 0.0;         // where the direct path arrives (the plugin's latency)
    double thresholdDb = -60.0;          // arrivals: peaks of |h| at most this far below the largest
    double minimumSpacingSeconds = 0.002;// two arrivals closer than this are one
    double minimumCrest = 2.0;           // an arrival's peak against the RMS of |h| within half that spacing around it
    double bandLowHz = 20.0;             // time and gain from this band ...
    double bandHighHz = 2000.0;          // ... (Lagrange of order 3 at 2 kHz, 48 kHz: 0.002 dB below 1)
    int maximumEchoes = 200;
    double loopFloorDb = -50.0;          // the loop filter where both echoes' spectra are above this, relative to the largest echo spectrum
};

struct Arrival
{
    double timeSamples = 0.0;            // fractional, counted from the impulse
    double gainDb = 0.0;                 // the magnitude of the gain in the band
    bool inverted = false;
};

struct EchoAnalysis
{
    bool found = false;                  // at least one arrival
    bool hasDirect = false;              // an arrival at the latency (within half a sample)
    Arrival direct;
    std::vector<Arrival> echoes;
    double delaySamples = 0.0;           // the first echo after the direct path (after the latency when there is none)
    double spacingSamples = 0.0;         // the mean distance of successive echoes (0 with fewer than two echoes)
    double spacingSpreadSamples = 0.0;   // the largest deviation of a distance from the mean
    double feedbackDb = 0.0;             // the mean level step from one echo to the next (dB per repeat)
    double decayDbPerSecond = 0.0;       // the same per second
    double mixDb = 0.0;                  // the first echo against the direct path (0 without a direct path)
    std::vector<double> loopFrequencies; // the loop filter: |X(k+1) / X(k)| averaged (in dB) over the echo pairs above the floor
    std::vector<double> loopMagnitudeDb;
    std::vector<bool> loopValid;         // false where no pair was above the floor
};

EchoAnalysis analyseEchoes(const std::vector<double>& response, const EchoSettings& settings);
}
