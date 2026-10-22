#pragma once

#include <vector>

namespace pluginlab::measure
{
// The delay trajectory of a modulated delay (W11.1d, docs/design/W11-delay-plan.md 4.3): tau(t), the delay at the output time t (output t
// reads the input of t - tau(t)), measured two ways, then its LFO fitted.
// - Impulse train: an impulse every periodSamples; the first echo of each (the arrival after the dry path, by the echo analysis) samples
//   tau at its arrival time (resolution: the period). Works with a dry path; feedback should be 0 or the echo train shorter than a period.
// - Steady tone (3150 Hz, the wow and flutter tone of AES6-2008): the phase of the output's analytic signal gives tau at every sample, up to
//   whole periods of the tone (taken from a rough delay, e.g. the impulse train's mean). Needs the wet signal alone (mix 100 %, no feedback).
struct TrajectoryPoint
{
    double timeSamples = 0.0;  // the output time
    double delaySamples = 0.0; // tau there
};

enum class LfoShapeFound
{
    None,      // no modulation (the depth below minimumDepthSamples)
    Sine,
    Triangle,
    Other      // neither fits (random, several LFOs, a drift)
};

struct DelayTrajectory
{
    std::vector<TrajectoryPoint> points;
    double meanSamples = 0.0;
    double depthSamples = 0.0;          // the peak deviation of the fitted LFO
    double rateHz = 0.0;
    LfoShapeFound shape = LfoShapeFound::None;
    double sineResidualSamples = 0.0;   // RMS of the points against the best sine ...
    double triangleResidualSamples = 0.0; // ... and the best triangle of the same rate
    double pitchDeviationCents = 0.0;   // the largest pitch deviation of the fitted LFO: 1200 log2(1 / (1 -+ dtau/dt))
};

struct TrajectorySettings
{
    double sampleRate = 48000.0;
    double latencySamples = 0.0;        // the dry path of each impulse arrives here (it is not the echo)
    double expectedDelaySamples = 0.0;  // > 0: in each period the echo nearest to this delay (with feedback, later echoes of the impulse before
                                        // can come first); 0: the first echo
    double minimumDepthSamples = 0.05;  // below this: no modulation
    double lowestRateHz = 0.05;         // the rates searched
    double shapeResidualShare = 0.1;    // a shape fits if its residual is below this share of the depth and below the other shape's
};

// The test signal: count impulses of height `level`, one every period samples (the first at sample 0)
std::vector<float> makeImpulseTrain(int count, int periodSamples, float level);
// The output for that train (one channel, divided by the level or not: only times are used)
DelayTrajectory measureTrajectoryFromImpulses(const std::vector<double>& output, int count, int periodSamples, const TrajectorySettings& settings);

// The test signal: sin(2 pi f n / fs) for length samples
std::vector<float> makeTone(double frequencyHz, int length, double sampleRate, float level);
// The output for that tone (the wet signal alone); roughDelaySamples picks the whole periods; edgeSamples at both ends are not used
DelayTrajectory measureTrajectoryFromTone(const std::vector<double>& output, double frequencyHz, double roughDelaySamples, int edgeSamples,
                                          const TrajectorySettings& settings);

// The LFO of measured points: the best sine (rate searched, then refined), the best triangle of that rate (phase searched), the shape
DelayTrajectory fitTrajectory(std::vector<TrajectoryPoint> points, const TrajectorySettings& settings);

// How a delay changes its time (W11.1h, docs/design/W11-delay-plan.md 4.4b): a steady tone (the wet signal alone), the time changed once in
// the middle. From the analytic signal: the phase against the tone's own, r(n) = phase(n) - w n, is constant before and after; where it moves is
// the change. A change within 64 samples is a jump (a click; the analytic signal spreads it a little); one whose phase moves in a straight line by
// the whole w (tau_after - tau_before), without a dip of the envelope, a glide (tape: the pitch is f (1 - dtau/dt) during it, the deviation in
// cents); any other a crossfade (two read positions mixed: a dip, the phase moves by the wrapped difference only). A crossfade between delays a
// whole number of the tone's periods apart does not show (old and new are in phase): the test signal's frequency then has to change.
enum class TimeChangeKind
{
    None,
    Jump,
    Crossfade,
    Glide
};

struct TimeChangeAnalysis
{
    TimeChangeKind kind = TimeChangeKind::None;
    double startSamples = 0.0;          // where the phase leaves its value before
    double lengthSamples = 0.0;         // until it has its value after
    double delayBeforeSamples = 0.0;    // from the phase, whole periods from the rough delays
    double delayAfterSamples = 0.0;
    double envelopeDipDb = 0.0;         // the lowest envelope during the change against before
    double glidePitchCents = 0.0;       // the glide's pitch deviation (negative: the delay grows, the pitch falls)
    double glideResidualRadians = 0.0;  // RMS of the phase against the straight line in the middle 80 % of the change
};

// output: the response to makeTone(frequencyHz, ...); the first and the last quarter (inside edgeSamples) are taken as before and after
TimeChangeAnalysis analyseTimeChange(const std::vector<double>& output, double frequencyHz, double roughDelayBefore, double roughDelayAfter, int edgeSamples,
                                     double sampleRate);
}
