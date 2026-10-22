#pragma once

#include <vector>

#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// Dynamics (W11.3a, W11.3b, docs/design/W11-dynamics-plan.md 4.0, 4.1): the gain envelope, the static curve from a level staircase and its fit.
// The test signal of Simmer, Schmidt, Bitzer (AES 120, 2006): a square wave (the same RMS and peak value: the curve does not depend on the
// detector), here at 500 Hz with an even half period, so that no sample is zero and the gain is output / input sample by sample.

enum class StaircaseShape
{
    Square,
    Sine
};

struct StaircaseSettings
{
    double sampleRate = 48000.0;
    double frequencyHz = 500.0;
    std::vector<double> levelsDbfs;       // the steps (the peak level of each)
    double stepSeconds = 0.5;
    StaircaseShape shape = StaircaseShape::Square;
};

// The staircase; each step starts at k * stepSeconds
std::vector<float> makeStaircase(const StaircaseSettings& settings);

// The gain envelope g(n) = output(n) / input(n - lag) in dB (the input's samples must not be zero: a square wave), lag the device's latency
std::vector<double> getGainEnvelopeDb(const std::vector<double>& input, const std::vector<double>& output, int lagSamples);

// One point of the static curve: a step's level, the steady gain at its end, whether it had settled (the gain over the last quarter of the step
// varies by less than settleDb)
struct StaticPoint
{
    double inputDbfs = 0.0;
    double gainDb = 0.0;
    double outputDbfs = 0.0;
    bool settled = false;
};

struct StaticCurveSettings
{
    double sampleRate = 48000.0;
    double lowestDbfs = -60.0;
    double highestDbfs = 0.0;
    double stepDb = 1.0;
    double stepSeconds = 0.5;
    double settleDb = 0.01;
    double latencySamples = 0.0;          // the look-ahead / the reported latency: the envelope's lag
};

struct StaticCurve
{
    std::vector<StaticPoint> rising;
    std::vector<StaticPoint> falling;
    double hysteresisDb = 0.0;            // the largest gain difference between rising and falling at the same level
    int unsettledSteps = 0;
};

// The rising and the falling staircase through the device (one channel driven on all inputs), the steady gain per step
StaticCurve measureStaticCurve(const Device& device, int channels, const StaticCurveSettings& settings);

// The gain computer of Giannoulis et al. fitted to points (input level, gain): threshold, ratio, knee width, makeup. With threshold and knee fixed
// the gain is linear in (1/R - 1) and the makeup (least squares); threshold and knee by a grid, then refined
struct StaticFit
{
    double thresholdDb = 0.0;
    double ratio = 1.0;
    double kneeDb = 0.0;
    double makeupDb = 0.0;
    double rmsErrorDb = 0.0;              // how well the model describes the points
    bool compresses = false;              // ratio above 1.01
};

StaticFit fitStaticCurve(const std::vector<StaticPoint>& points);

// The model's gain at a level (the fit's parameters)
double getModelGainDb(const StaticFit& fit, double levelDb);

// The time behaviour (W11.3c, docs/design/W11-dynamics-plan.md 4.0, 4.3): the paper's step (below the threshold, high, below again; a square
// wave, so that the detector sees one level per phase), then a second, lower step (two smoothers in series are told apart only by steps of
// different heights), the gain curve, and the model variants fitted to it with the measured static curve as
// the gain computer: GainLog (the gain reduction smoothed in dB), LevelLog (the detector level smoothed in dB), TwoStage (the detector power
// smoothed, then the gain in dB); time constants a = exp(-1 / (tau fs)) with smooth branching; a hold before the release. The variant with the
// smallest error is the result (the paper: three variants, the smallest error chosen).
enum class TimeModel
{
    GainLog,
    LevelLog,
    TwoStage
};

const char* getTimeModelName(TimeModel model);

struct TimeParameters
{
    double attackSeconds = 0.005;
    double releaseSeconds = 0.05;
    double detectorAttackSeconds = 0.002;  // TwoStage only
    double detectorReleaseSeconds = 0.02;
    double holdSeconds = 0.0;
};

// The model's gain in dB for the detector level of every sample
std::vector<double> simulateGainDb(const std::vector<double>& levelsDb, const StaticFit& curve, TimeModel model, const TimeParameters& parameters, double sampleRate);

struct TimeFit
{
    TimeModel model = TimeModel::GainLog;
    TimeParameters parameters;
    double rmsErrorDb = 1.0e300;           // over the attack and the release phase
};

struct StepSettings
{
    double sampleRate = 48000.0;
    double lowDbfs = -40.0;                // below the threshold
    double highDbfs = 0.0;
    double phaseSeconds = 0.5;
    double latencySamples = 0.0;
    double attackToleranceDb = 3.0;        // the convention of the hearing-aid standard ANSI S3.22 (attack: within 3 dB of the final value, release:
    double releaseToleranceDb = 4.0;       // within 4 dB); IEC 60268-8 and IEC 60118-2 to be checked against their text (the plan)
};

struct StepResponse
{
    std::vector<double> levelsDb;          // the input's level per sample
    std::vector<double> gainDb;            // the measured gain curve
    int attackStart = 0;                   // the samples where the level rises and falls (the first step)
    int releaseStart = 0;
    std::vector<TimeFit> fits;             // per model
    TimeFit best;
    double holdSeconds = 0.0;              // after the fall, until the gain has moved by 0.01 dB (an upper bound: a smoothed detector delays the
                                           // release's start too; the fits' hold is the model's)
    double attackSettleSeconds = 0.0;      // the standards' convention
    double releaseSettleSeconds = 0.0;
};

StepResponse measureStepResponse(const Device& device, int channels, const StaticFit& curve, const StepSettings& settings);

// The look-ahead (the paper's method): white noise below the threshold through the device; the lag of the largest cross-correlation of input and
// output (up to maximumSeconds)
int measureLookahead(const Device& device, int channels, double sampleRate, double levelDbfs = -40.0, double maximumSeconds = 0.05);
}
