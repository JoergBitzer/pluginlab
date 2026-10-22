#pragma once

#include <array>
#include <vector>

#include "pluginlab/reference/Processor.h"

namespace pluginlab::reference
{
// The reference compressor (W11.3, docs/design/W11-dynamics-plan.md 7): the feed-forward design that Giannoulis, Massberg and Reiss (2012)
// recommend. Per sample: the detector level x_dB (peak: 20 lg |x|; RMS: a moving mean square over rmsSeconds), with the stereo link
// x_c = (1 - link) x_c + link max(x); the gain computer (hard or soft knee W):
//   y = x                                       for 2 (x - T) < -W
//   y = x + (1/R - 1) (x - T + W/2)^2 / (2 W)   for |2 (x - T)| <= W
//   y = T + (x - T) / R                         for 2 (x - T) > W
// the gain reduction c = x - y >= 0 smoothed in the log domain with smooth branching (attack while it grows, release while it falls):
//   s(n) = a s(n - 1) + (1 - a) c(n),  a = exp(-1 / (tau fs)), tau the attack or the release time
// and the gain makeup - s applied to the input delayed by the look-ahead.
// Where the smoothing is (the model variants of Simmer, Schmidt, Bitzer 2006 and the plan's comments, W11.3c): GainLog smooths the gain
// reduction in dB (Giannoulis' recommendation); LevelLog smooths the detector level in dB and the gain computer follows it unsmoothed; TwoStage
// smooths the detector's power (linear, its own attack and release) and then the gain reduction in dB; LevelLinear smooths the level as an
// amplitude (linear) and the gain computer follows it unsmoothed (W11.3f: an atypical design, its times in dB depend on the step's height)
enum class Smoothing
{
    GainLog,
    LevelLog,
    TwoStage,
    LevelLinear
};

enum class Detector
{
    Peak,
    Rms
};

struct CompressorSettings
{
    double sampleRate = 48000.0;
    double thresholdDb = -20.0;
    double ratio = 4.0;
    double kneeDb = 0.0;
    double makeupDb = 0.0;
    double attackSeconds = 0.005;
    double releaseSeconds = 0.05;
    Detector detector = Detector::Peak;
    double rmsSeconds = 0.01;
    double lookaheadSeconds = 0.0;
    double stereoLink = 1.0;           // 0: each channel its own gain, 1: one gain from the louder channel
    Smoothing smoothing = Smoothing::GainLog;
    double detectorAttackSeconds = 0.002;  // TwoStage: the detector's power smoothing
    double detectorReleaseSeconds = 0.02;
    double holdSeconds = 0.0;          // after the target reduction falls, the smoothed value stays this long before the release starts
    double sidechainHighPassHz = 0.0;  // a second-order Butterworth high-pass before the detector (0: none; W11.3d)
};

class Compressor : public Processor
{
public:
    explicit Compressor(const CompressorSettings& settings);

    const CompressorSettings& getSettings() const;

    // The static curve: the gain in dB for a steady detector level (the gain computer and the makeup)
    double getStaticGainDb(double levelDb) const;
    // The gain applied to each sample since the reset, channel 0 (recorded when enabled; for the tests)
    void setRecording(bool record);
    const std::vector<double>& getGainTraceDb() const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;
    int getLatencySamples() const override;

private:
    double computeReduction(double levelDb) const;
    double getDetectorStart() const;

    CompressorSettings m_settings;
    double m_attack = 0.0;
    double m_release = 0.0;
    double m_detectorAttack = 0.0;
    double m_detectorRelease = 0.0;
    int m_hold = 0;
    std::vector<double> m_detector;              // per channel: the smoothed level (LevelLog: dB) or power (TwoStage)
    std::vector<double> m_sidechainFilter;       // the high-pass: b0, b1, b2, a1, a2 (empty: none)
    std::vector<std::array<double, 2>> m_sidechainState; // per channel: its two registers
    std::vector<int> m_holding;                  // per channel: samples of hold left
    int m_lookahead = 0;
    int m_rmsLength = 1;
    std::vector<double> m_smoothed;              // per channel: the smoothed gain reduction in dB
    std::vector<std::vector<double>> m_delay;    // per channel: the look-ahead line
    std::vector<std::vector<double>> m_squares;  // per channel: the RMS window
    std::vector<double> m_squareSum;
    size_t m_position = 0;
    size_t m_rmsPosition = 0;
    bool m_record = false;
    std::vector<double> m_trace;
};
}
