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
// smallest error is the result (the paper: three variants, the smallest error chosen; a fourth since W11.3f, LevelLinear).
enum class TimeModel
{
    GainLog,
    LevelLog,
    TwoStage,
    LevelLinear       // the level smoothed as an amplitude (linear), the gain computer after it (W11.3f, SocaLabs Compressor)
};

const char* getTimeModelName(TimeModel model);

// Atypical time behaviour (W11.3f): smoothing the linear level before the gain computer makes the attack and release times, read in dB, depend
// on the height of the level step (an exponential in the amplitude is not one in dB): a large reduction recovers its first decibels quickly
// and its last ones slowly, a small one evenly. Giannoulis et al. (2012) advise smoothing in the log domain for that reason. The report marks
// such a device "atypical"
bool isAtypicalTimeModel(TimeModel model);

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
    int leadSamples = 0;                   // the look-ahead: the gain moves this long before the audio it acts on reaches the output (within the lag)
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

// The detector (W11.3d, 4.2; the paper's tests): the paper's first test: a 4 kHz square at levelDbfs, every second period replaced by zeros
// (0.25 ms: the peak stays, the RMS falls by 3 dB). An RMS window (a millisecond or more) halves the power: the gain rises as the static curve
// gives for -3 dB. A smoothed peak detector (the gain smoothing of Giannoulis et al.) is not an ideal one: in the gaps its release runs, its
// reduction settles at c / (1 + tau_a / tau_r) whatever the gap length. So the decision compares the measured change with both predictions:
// the RMS one, and the time model of W11.3c simulated with the gaps as silence; the nearer wins (unclear if they are within 0.3 dB of each other).
// Reported too: the cross-check of constant RMS and varying peak (a pulse every 16 samples, 12 dB above the square; it does not decide: a
// detector whose attack is slower than the pulses averages them).
enum class DetectorKind
{
    Peak,
    Rms,
    Unclear
};

const char* getDetectorKindName(DetectorKind kind);

struct DetectorTest
{
    DetectorKind kind = DetectorKind::Unclear;
    double crestChangeDb = 0.0;           // the gain of the pulses minus that of the square (the same RMS)
    double peakChangeDb = 0.0;            // what a peak detector would show (the static curve at +12 dB)
    double gainChangeDb = 0.0;            // the zeros test: the gain with the zeros minus without
    double rmsChangeDb = 0.0;             // what an RMS detector would show there (the static curve at -3 dB)
    double peakModelChangeDb = 0.0;       // what the time model as a peak detector would show
};

DetectorTest measureDetector(const Device& device, int channels, const StaticFit& curve, const TimeFit& time, double sampleRate, double levelDbfs = 0.0,
                             double latencySamples = 0.0);

// The side chain's frequency response (4.4): per frequency the input level of a sine at which the gain reduction reaches reductionDb (a staircase
// of sines in 1 dB steps, interpolated); a level above the staircase's top when it never does (NaN)
double measureThresholdAt(const Device& device, int channels, double frequencyHz, double sampleRate, double reductionDb = 1.0, double lowestDbfs = -60.0,
                          double highestDbfs = 0.0);

// The two-tone test (4.4): a loud tone at f1 (above the threshold) and a quiet probe at f2, against the probe alone; the probe's gain change over f2,
// the "dynamic transfer function": the same at every f2 for a single band, a plateau ending at a crossover for multiband, a bell for a dynamic EQ
enum class BandKind
{
    None,          // the loud tone changes no probe (not level dependent there)
    SingleBand,
    Multiband,
    DynamicEq
};

const char* getBandKindName(BandKind kind);

struct DynamicTransfer
{
    std::vector<double> frequencies;      // the probes (those within 1/6 octave of the tone left out: the beating of the two tones moves the gain)
    std::vector<double> reductionDb;      // positive: the probe is turned down by the loud tone
    BandKind kind = BandKind::None;
    double plateauOctaves = 0.0;          // the width of the region within 3 dB of the largest reduction: a band of a multiband is at least an
                                          // octave wide, a bell narrower (a dynamic shelf would look like a band)
};

DynamicTransfer measureDynamicTransfer(const Device& device, int channels, double sampleRate, double toneHz, double toneDbfs, double probeDbfs,
                                       const std::vector<double>& probeFrequencies);

// The stereo link (4.6): left above the threshold, right below it; the right channel's gain reduction relative to the left's (0: independent, 1:
// fully linked; between: what the device's link law gives at these levels)
double measureStereoLink(const Device& device, double sampleRate, double highDbfs = 0.0, double lowDbfs = -30.0);

// Distortion caused by the dynamics (W11.3e, 4.5): THD (IEC 60268-3, the unit of W7.5) of a sine above the threshold at several frequencies: a
// fast attack and release follow the waveform and modulate it, so the THD rises towards low frequencies
struct DynamicsDistortion
{
    std::vector<double> frequencies;
    std::vector<double> thdDb;
};

DynamicsDistortion measureDynamicsDistortion(const Device& device, int channels, double sampleRate, double levelDbfs, const std::vector<double>& frequencies);

// The true peak (ITU-R BS.1770, annex 2): the largest magnitude after 4x oversampling. Here by a windowed-sinc interpolator (48 zero crossings
// per side, Blackman window, every phase normalised to unity gain at DC; with 16 the ripple reached 0.1 dB at fs/4); BS.1770 gives its own
// 48-tap filter, which allows more.
double getTruePeakDbfs(const std::vector<double>& signal);
double getSamplePeakDbfs(const std::vector<double>& signal);

// Limiters (4.7): a sine at fs/4 with 45 degrees of phase (its samples 3.01 dB below its true peak) at driveDbfs: the output's sample peak (the
// ceiling a sample-peak limiter keeps) and its true peak; a step from -40 dBFS to driveDbfs (a square): the overshoot of the output's peak in the
// first 50 ms over its steady peak
struct LimiterTest
{
    double samplePeakDbfs = 0.0;
    double truePeakDbfs = 0.0;
    double overshootDb = 0.0;
};

LimiterTest measureLimiter(const Device& device, int channels, double sampleRate, double driveDbfs = 6.0);

// Everything of W11.3a ... e on one device (W11.3f: the unit "dynamics" of the reports): the static curve and its fit; for a compressor (the fit's
// ratio above 1.01) the time behaviour, the look-ahead, the detector, the two-tone test, the stereo link, the distortion and the limiter test, with
// their levels chosen from the fitted threshold (the low step and the quiet channel 10 dB below the knee, the loud ones at 0 dBFS)
struct DynamicsMeasurementSettings
{
    double sampleRate = 48000.0;
    int channels = 2;
    double latencySamples = 0.0;
    bool bands = true;                    // the two-tone test (a sixth-octave grid of probes: the slowest part)
};

struct DynamicsMeasurement
{
    bool levelDependent = false;          // the steady gain varies by more than 0.5 dB over the staircase
    double gainRangeDb = 0.0;             // the largest minus the smallest steady gain
    StaticCurve curve;
    StaticFit fit;
    bool compressor = false;              // level dependent and the fit's ratio above 1.01 (expanders and gates: later)
    StepResponse step;
    int audioDelaySamples = 0;            // the lag of the largest cross-correlation of noise below the threshold: the look-ahead and any latency
    DetectorTest detector;
    DynamicTransfer transfer;
    double toneHz = 1000.0;               // the loud tone of the two-tone test
    bool stereo = false;
    double stereoLink = 0.0;
    DynamicsDistortion distortion;
    double distortionLevelDbfs = 0.0;
    LimiterTest limiter;
};

DynamicsMeasurement measureDynamicsResponse(const Device& device, const DynamicsMeasurementSettings& settings);
}
