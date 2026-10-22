#pragma once

#include <vector>

namespace pluginlab::measure
{
// Loudness (W11.3f, docs/design/W11-dynamics-plan.md 4.8; the author decided for ITU-R BS.1770-5): the K-weighting (the pre-filter, a high
// shelf of +4 dB above about 1.7 kHz, and the RLB high-pass at 38 Hz; their analog prototypes, so that every sample rate gets the filter the
// standard gives at 48 kHz), the mean square per channel in blocks of 400 ms with 75 % overlap, the channels summed (weights 1 for left and
// right), L = -0.691 + 10 lg(sum); the integrated loudness with the absolute gate (-70 LKFS) and the relative one (-10 LU below the
// absolutely gated mean); the loudness range of EBU Tech 3342 (short-term loudness of 3 s every 100 ms, gated at -70 LKFS and -20 LU, the
// 10th to the 95th percentile); the true peak (BS.1770 annex 2), the sample peak and the crest factor.
struct LoudnessResult
{
    double integratedLufs = -1000.0;
    double loudnessRangeLu = 0.0;
    double truePeakDbfs = -1000.0;
    double samplePeakDbfs = -1000.0;
    double crestFactorDb = 0.0;          // the sample peak against the RMS of the whole signal (the loudest channel)
    std::vector<double> momentaryLufs;   // 400 ms blocks every 100 ms
    std::vector<double> shortTermLufs;   // 3 s every 100 ms
};

LoudnessResult measureLoudness(const std::vector<std::vector<double>>& channels, double sampleRate);

// The K-weighted signal of one channel (for tests and views)
std::vector<double> applyKWeighting(const std::vector<double>& signal, double sampleRate);

// The gain a device applies to an arbitrary signal (W11.3f, 4.8: the gain curves on music): per window g = sum x y / sum x^2 (least squares,
// the input delayed by the lag), in dB; windows whose input is more than floorDb below the loudest window are left out (NaN). A sample-by-sample
// ratio would glitch at the zero crossings (the paper's figures).
struct GainCurve
{
    std::vector<double> times;           // window centres in seconds
    std::vector<double> gainDb;
    std::vector<double> inputLevelDbfs;  // the window's input RMS
};

GainCurve getGainCurve(const std::vector<double>& input, const std::vector<double>& output, int lagSamples, double sampleRate, double windowSeconds = 0.01,
                       double floorDb = 50.0);
}
