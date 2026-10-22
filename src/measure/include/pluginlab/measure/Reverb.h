#pragma once

#include <vector>

namespace pluginlab::measure
{
// The decay of a reverb's impulse response (W11.2b, docs/design/W11-reverb-plan.md 4.2; ISO 3382-1): bands, the energy decay curve (Schroeder's
// backward integration) after the truncation and compensation of Lundeby et al. (1995), T20, T30, EDT by linear regression, C50, C80, D50, the
// centre time, the pre-delay, the direct path (DPD) and the direct-to-reverberant ratio (DRR).

// A band of IEC 61260-1 (base 10): centre 1000 Hz * 10^(3 x / (10 b)), edges at the centre * 10^(+-3 / (20 b)); b = 1 octaves, 3 third octaves
struct Band
{
    double centreHz = 0.0;
    double lowHz = 0.0;
    double highHz = 0.0;
};

std::vector<Band> getBands(int fraction, double lowestCentreHz, double highestCentreHz);
// The response through the band's filter (Butterworth band-pass of the order, reference::designButterworthBandPass)
std::vector<double> filterBand(const std::vector<double>& signal, const Band& band, double sampleRate, int order = 3);

// Lundeby's iteration: where the decay meets the noise floor, and the energy the decay would have had after that point
struct Truncation
{
    int sample = 0;                 // the response is used up to here (the end of a response without a floor)
    double noiseEnergy = 0.0;       // the floor's mean energy per sample (0: none found)
    double compensation = 0.0;      // the decay's energy after the truncation (added to the backward integration)
    bool noiseFound = false;
};

Truncation findTruncation(const std::vector<double>& response, int start, double sampleRate);

// The energy decay curve in dB from start (0 dB there) to the truncation
std::vector<double> getDecayCurve(const std::vector<double>& response, int start, const Truncation& truncation);

struct DecayTimes
{
    double t20 = 0.0;               // seconds; 0 when the decay does not reach the end of its range
    double t30 = 0.0;
    double edt = 0.0;
    double t30Correlation = 0.0;    // the regression's |r| (ISO 3382-1 asks for a straight decay)
    double rangeDb = 0.0;           // the decay the curve covers (to the truncation)
};

// T20 (-5 ... -25 dB), T30 (-5 ... -35 dB), EDT (0 ... -10 dB) from a decay curve: lines by least squares, T = -60 / slope
DecayTimes getDecayTimes(const std::vector<double>& curveDb, double sampleRate);

struct BandDecay
{
    Band band;
    DecayTimes times;
    double c80Db = 0.0;
};

struct ReverbSettings
{
    double sampleRate = 48000.0;
    double latencySamples = 0.0;          // the input's time in the response (the plugin's latency)
    double directWindowSeconds = 0.0025;  // the direct path: within this around its peak (DRR)
    double directBelowDb = 40.0;          // a direct path is there if its peak is at most this far below the response's largest sample
    double onsetBelowDb = 20.0;           // the reverb's onset: the first sample after the direct path at most this far below the reverb's peak
    double lowestCentreHz = 125.0;
    double highestCentreHz = 8000.0;
    int filterOrder = 3;
};

struct ReverbAnalysis
{
    bool hasDirect = false;
    double directSample = 0.0;            // the direct path's peak (or the latency without one): time 0 of the clarity and the decay
    double directPathDelaySamples = 0.0;  // DPD: the direct path against the input (the latency included)
    double directToReverberantDb = 0.0;   // DRR
    double preDelaySeconds = 0.0;         // the reverb's onset against the direct path (against the input without one)
    Truncation truncation;                // broadband
    DecayTimes broadband;
    double c50Db = 0.0;
    double c80Db = 0.0;
    double d50 = 0.0;
    double centreTimeSeconds = 0.0;
    std::vector<BandDecay> octaves;
    std::vector<BandDecay> thirdOctaves;
};

ReverbAnalysis analyseReverb(const std::vector<double>& response, const ReverbSettings& settings);
}
