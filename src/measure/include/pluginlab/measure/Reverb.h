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

// The texture of the tail (W11.2c, docs/design/W11-reverb-plan.md 4.3), on response[start, end) (start: the reverb's onset):
// - the echo density profile of Abel and Huang (2006): per window (Hann, normalized) the weighted share of samples outside one standard
//   deviation, divided by erfc(1 / sqrt(2)) (the share for Gaussian noise): 1 = noise-like, below 1 sparse; the mixing time: where it first
//   reaches densityMixed
// - Griesinger's echo count: per block of 20 ms the samples within 20 dB of the block's largest, per second
// - the short-time histogram and kurtosis (Extra, Simmer, Fischer, Bitzer 2006): h / sqrt(EDC) (the decay removed), blocks of 1024 samples, per
//   block the histogram of the amplitudes and the excess kurtosis (0 for Gaussian noise)
// - the normalized autocorrelation of h / sqrt(EDC) after the mixing time: repetitions in the tail (a peak away from lag 0)
// - the coloration: the scatter in dB of the late spectrum's fine structure around its third-octave smoothing (Rayleigh noise: about 5.6 dB)
struct TextureSettings
{
    double sampleRate = 48000.0;
    double windowSeconds = 0.02;          // the echo density's window and Griesinger's block
    double stepSeconds = 0.001;           // the echo density every millisecond
    double densityMixed = 0.95;
    int histogramBlock = 1024;
    int histogramBins = 41;               // from -4 to +4 (in units of the normalized response's standard deviation)
    double autocorrelationSeconds = 0.1;  // the lags examined
    double shortestLagSeconds = 0.001;    // repetitions: peaks beyond this lag
    double colorationSeconds = 0.2;       // the late window of the coloration (after the mixing time)
};

struct Texture
{
    std::vector<double> densityTimes;     // seconds after start
    std::vector<double> density;
    double mixingTimeSeconds = -1.0;      // after start; -1: never mixed
    std::vector<double> echoTimes;
    std::vector<double> echoesPerSecond;
    std::vector<double> kurtosisTimes;
    std::vector<double> kurtosis;
    std::vector<std::vector<double>> histograms; // per block, histogramBins shares
    double lateKurtosis = 0.0;            // the mean over the blocks after the mixing time (never mixed: the second half)
    std::vector<double> autocorrelation;  // lag 0 ... autocorrelationSeconds
    double repetitionLagSeconds = 0.0;    // the largest peak beyond shortestLagSeconds
    double repetitionStrength = 0.0;      // its height (1 = a perfect repetition)
    double colorationDb = 0.0;
};

Texture analyseTexture(const std::vector<double>& response, int start, int end, const TextureSettings& settings);

// The energy decay relief (Jot 1992): the backward-integrated energy per frequency bin of the short-time spectrum, in dB re its start; the decay
// time per bin by least squares (-5 ... -35 dB, as T30)
struct DecayRelief
{
    std::vector<double> frequencies;
    std::vector<double> times;            // the frames' centres, seconds after start
    std::vector<std::vector<float>> levelsDb; // [frame][bin]
    std::vector<double> t60;              // per bin (0 where the decay does not reach -35 dB)
};

DecayRelief getDecayRelief(const std::vector<double>& response, int start, int end, double sampleRate, int frameSize = 1024, int hop = 256);

// Time variance (W11.2d, docs/design/W11-reverb-plan.md 4.5): one instance, impulses far apart (each response meets a modulation at another
// phase). Per response its decay (Schroeder, T30) and their spread; the strongest early peaks of the first response followed through all of them
// (the author's method, AES 121: a modulated reflection spreads, a fixed one does not). Averaging the responses would cancel a modulated tail, so
// every decay is taken from one response alone.
struct VarianceSettings
{
    double sampleRate = 48000.0;
    double latencySamples = 0.0;
    double earlySeconds = 0.05;       // the peaks: the strongest within this after the onset ...
    int peaks = 8;                    // ... this many
    int peakSearch = 3;               // a peak is followed within +-this many samples
    double spreadVariant = 0.05;      // a peak whose value scatters by more than this (std / |mean|) is a modulated reflection
};

struct PeakSpread
{
    double timeSeconds = 0.0;         // in the first response, after the impulse
    double meanValue = 0.0;
    double valueSpread = 0.0;         // the standard deviation of its value over the responses, relative to |mean|
    double timeSpreadSamples = 0.0;   // the standard deviation of its position
};

struct TimeVariance
{
    std::vector<double> t30PerResponse;
    double t30Mean = 0.0;
    double t30Spread = 0.0;           // standard deviation, seconds
    std::vector<PeakSpread> peaks;
    double largestValueSpread = 0.0;
    bool timeVariant = false;         // a peak scatters by more than spreadVariant
};

// output: one channel of the response to makeImpulseTrain(count, periodSamples, ...) (each period longer than the tail)
TimeVariance analyseTimeVariance(const std::vector<double>& output, int count, int periodSamples, const VarianceSettings& settings);

// The interrupted-noise method of ISO 3382-1 (natural for time-variant reverbs): noise bursts, each followed by silence; the decays after the
// bursts averaged in energy (10 ms blocks), the line from -5 to -35 dB below the steady level
std::vector<float> makeInterruptedNoise(int bursts, int burstSamples, int periodSamples, float level, int seed);
double getInterruptedNoiseT30(const std::vector<double>& output, int bursts, int burstSamples, int periodSamples, double sampleRate);
}
