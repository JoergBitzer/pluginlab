#pragma once

#include <vector>

#include "pluginlab/measure/DelayTrajectory.h"
#include "pluginlab/measure/Device.h"

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

// The level dependence (W11.1e, docs/design/W11-delay-plan.md 4.4): the echo response at several input levels (e.g. -40, -20, -6 dBFS, each
// response divided by its impulse's height). Echo gains that fall with the level: a saturation in the loop (a tape or BBD model, a limiter);
// echoes that do not decay: the loop oscillates (feedback at or above 100 %, held by a saturation).
struct EchoLevel
{
    double levelDbfs = 0.0;
    double firstEchoGainDb = 0.0;
    double feedbackDb = 0.0;
    int echoes = 0;
};

struct LevelDependence
{
    std::vector<EchoLevel> levels;        // as given, the lowest level first after sorting
    double compressionDb = 0.0;           // the first echo's gain at the highest level minus at the lowest (negative: saturation)
    double feedbackChangeDb = 0.0;        // the feedback per repeat at the highest level minus at the lowest
    bool saturates = false;               // one of the two below -thresholdDb
    bool selfOscillating = false;         // at some level the echoes do not decay (at least three, feedback above -0.05 dB per repeat)
};

LevelDependence analyseLevelDependence(const std::vector<double>& levelsDbfs, const std::vector<EchoAnalysis>& analyses, double thresholdDb = 0.1);

// Stereo (W11.1c, docs/design/W11-delay-plan.md 4.2): the 2 x 2 echo response, one input driven at a time, each to both outputs
enum class StereoEchoKind
{
    Independent,    // no echo reaches the other output
    CrossFeedback,  // echoes reach the other output, and the driven side keeps its own echo train
    PingPong        // the echoes of one input alternate between the outputs (L, R, L, ... or R, L, R, ...)
};

struct StereoEchoAnalysis
{
    EchoAnalysis paths[2][2];             // [input][output]
    StereoEchoKind kind = StereoEchoKind::Independent;
    double pingPongSpacingSamples = 0.0;  // ping-pong: the distance of successive echoes over both outputs (the delay of one bounce)
};

// responses[input][output]: the response of each output to a unit impulse into one input (the other input silent)
StereoEchoAnalysis analyseStereoEchoes(const std::vector<double> (&responses)[2][2], const EchoSettings& settings);

// The echo response of a device (W11.1f, the unit "echoes" of the reports): an impulse into both inputs, then silence; if echoes come back,
// the stereo paths (one input at a time), the levels and the delay trajectory (an impulse train) follow
struct EchoMeasurementSettings
{
    double sampleRate = 48000.0;
    int channels = 2;
    double latencySamples = -1.0;         // where the dry path arrives; < 0: unknown, the first arrival is taken as the dry path (a single arrival
                                          // is then a latency, no echo; a 100 % wet delay loses its first echo to it)
    double levelDbfs = -6.0;
    std::vector<double> levelsDbfs = {-40.0, -20.0, -6.0};
    double renderSeconds = 4.0;           // the silence after each impulse (longer echo trains are cut)
    double trajectorySeconds = 4.0;       // the impulse train
};

struct EchoMeasurement
{
    bool hasEchoes = false;
    EchoAnalysis echoes;                  // both inputs driven, output channel 1
    bool stereo = false;
    StereoEchoAnalysis stereoEchoes;
    LevelDependence levels;
    DelayTrajectory trajectory;
    int trajectoryPeriodSamples = 0;
};

EchoMeasurement measureEchoResponse(const Device& device, const EchoMeasurementSettings& settings);
}
