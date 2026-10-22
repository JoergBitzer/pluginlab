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
}
