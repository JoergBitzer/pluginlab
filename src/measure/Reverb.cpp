#include "pluginlab/measure/Reverb.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Designs.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kTinyEnergy = 1.0e-300;
constexpr double kFirstIntervalSeconds = 0.01;   // Lundeby: the first averaging interval
constexpr double kIntervalsPer10Db = 5.0;        // ... then 3 to 10 intervals per 10 dB of decay (Lundeby: 5)
constexpr int kIterations = 5;
constexpr double kNoiseShare = 0.1;              // the noise from at least the last 10 % of the response
constexpr double kNoFloorDb = -150.0;            // the last part this far below the start: no floor (a digital reverb decays into silence)

double toDb(double energyRatio)
{
    return 10.0 * std::log10(std::max(energyRatio, kTinyEnergy));
}

// the mean energy of h^2 in intervals of `interval` samples from start; the interval centres
void getEnvelope(const std::vector<double>& response, int start, int end, int interval, std::vector<double>& levelsDb, std::vector<double>& centres)
{
    levelsDb.clear();
    centres.clear();
    for (int from = start; from + interval <= end; from += interval)
    {
        double energy = 0.0;
        for (int sample = from; sample < from + interval; ++sample)
        {
            energy += response[static_cast<size_t>(sample)] * response[static_cast<size_t>(sample)];
        }
        levelsDb.push_back(toDb(energy / interval));
        centres.push_back(from + 0.5 * interval);
    }
}

double getMeanEnergy(const std::vector<double>& response, int from, int end)
{
    double energy = 0.0;
    for (int sample = from; sample < end; ++sample)
    {
        energy += response[static_cast<size_t>(sample)] * response[static_cast<size_t>(sample)];
    }
    return energy / std::max(1, end - from);
}

// least squares level = a + b t over the points with level in [lowDb, highDb]; false with fewer than two points
bool fitLine(const std::vector<double>& levelsDb, const std::vector<double>& times, double lowDb, double highDb, double& a, double& b)
{
    double n = 0.0;
    double st = 0.0;
    double sl = 0.0;
    double stt = 0.0;
    double stl = 0.0;
    for (size_t index = 0; index < levelsDb.size(); ++index)
    {
        if (levelsDb[index] < lowDb || levelsDb[index] > highDb)
        {
            continue;
        }
        n += 1.0;
        st += times[index];
        sl += levelsDb[index];
        stt += times[index] * times[index];
        stl += times[index] * levelsDb[index];
    }
    const double denominator = n * stt - st * st;
    if (n < 2.0 || std::abs(denominator) < kTinyEnergy)
    {
        return false;
    }
    b = (n * stl - st * sl) / denominator;
    a = (sl - b * st) / n;
    return true;
}
}

std::vector<Band> getBands(int fraction, double lowestCentreHz, double highestCentreHz)
{
    std::vector<Band> bands;
    const double b = static_cast<double>(fraction);
    for (int x = -40; x <= 40; ++x)
    {
        const double centre = 1000.0 * std::pow(10.0, 3.0 * x / (10.0 * b));
        if (centre < lowestCentreHz * 0.99 || centre > highestCentreHz * 1.01)
        {
            continue;
        }
        const double half = std::pow(10.0, 3.0 / (20.0 * b));
        bands.push_back({centre, centre / half, centre * half});
    }
    return bands;
}

std::vector<double> filterBand(const std::vector<double>& signal, const Band& band, double sampleRate, int order)
{
    // section by section over the whole signal (transposed direct form II), a tight loop: the reverb analysis filters seconds of response in
    // dozens of bands
    std::vector<double> output = signal;
    for (const reference::BiquadCoefficients& section : reference::designButterworthBandPass(order, sampleRate, band.lowHz, band.highHz))
    {
        double z1 = 0.0;
        double z2 = 0.0;
        for (double& value : output)
        {
            const double input = value;
            value = section.b0 * input + z1;
            z1 = section.b1 * input - section.a1 * value + z2;
            z2 = section.b2 * input - section.a2 * value;
        }
    }
    return output;
}

Truncation findTruncation(const std::vector<double>& response, int start, double sampleRate)
{
    Truncation result;
    const int end = static_cast<int>(response.size());
    result.sample = end;
    if (end - start < 4)
    {
        return result;
    }
    // the start's level: the largest interval at the beginning; no floor if the last 10 % are far below it
    std::vector<double> levels;
    std::vector<double> centres;
    int interval = std::max(1, static_cast<int>(kFirstIntervalSeconds * sampleRate));
    getEnvelope(response, start, end, interval, levels, centres);
    if (levels.size() < 3)
    {
        return result;
    }
    const double top = *std::max_element(levels.begin(), levels.end());
    const int tailStart = end - std::max(interval, static_cast<int>(kNoiseShare * (end - start)));
    double noiseDb = toDb(getMeanEnergy(response, tailStart, end));
    if (noiseDb - top < kNoFloorDb)
    {
        return result; // decays into (numerical) silence: the whole response, no compensation
    }
    // a floor is flat: if the last part still falls (its second half 3 dB below its first), the response ends before any floor
    constexpr double kStillDecayingDb = 3.0;
    const int middle = (tailStart + end) / 2;
    if (toDb(getMeanEnergy(response, tailStart, middle)) - toDb(getMeanEnergy(response, middle, end)) > kStillDecayingDb)
    {
        return result;
    }
    // 1. a line from the top to 10 dB above the noise; where it meets the noise
    double a = 0.0;
    double b = 0.0;
    if (! fitLine(levels, centres, noiseDb + 10.0, top + 1.0, a, b) || b >= 0.0)
    {
        return result;
    }
    double crossing = (noiseDb - a) / b;
    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
        // 2. intervals of a fifth of a 10 dB decay; the noise from 5 ... 10 dB below the crossing (as a time: + 10 dB of decay), at least the last 10 %
        interval = std::max(1, static_cast<int>(10.0 / -b / kIntervalsPer10Db));
        getEnvelope(response, start, end, interval, levels, centres);
        const int noiseFrom = std::min(tailStart, static_cast<int>(crossing + 10.0 / -b));
        noiseDb = toDb(getMeanEnergy(response, std::max(start, noiseFrom), end));
        // 3. the line over the decay 5 ... 25 dB above the noise
        double na = 0.0;
        double nb = 0.0;
        const double highest = std::min(top, noiseDb + 25.0);
        if (! fitLine(levels, centres, noiseDb + 5.0, highest, na, nb) || nb >= 0.0)
        {
            break;
        }
        a = na;
        b = nb;
        crossing = (noiseDb - a) / b;
    }
    result.noiseFound = true;
    result.noiseEnergy = std::pow(10.0, noiseDb / 10.0);
    result.sample = std::clamp(static_cast<int>(crossing), start + 1, end);
    // the decay after the crossing: e(t) = e_c 10^(b (t - t_c) / 10) per sample, summed to infinity
    const double energyAtCrossing = std::pow(10.0, (a + b * result.sample) / 10.0);
    const double perSample = std::pow(10.0, b / 10.0);
    result.compensation = energyAtCrossing * perSample / (1.0 - perSample);
    return result;
}

std::vector<double> getDecayCurve(const std::vector<double>& response, int start, const Truncation& truncation)
{
    const int end = std::min(truncation.sample, static_cast<int>(response.size()));
    std::vector<double> curve(static_cast<size_t>(std::max(0, end - start)));
    double sum = truncation.compensation;
    for (int sample = end - 1; sample >= start; --sample)
    {
        sum += response[static_cast<size_t>(sample)] * response[static_cast<size_t>(sample)];
        curve[static_cast<size_t>(sample - start)] = sum;
    }
    if (curve.empty())
    {
        return curve;
    }
    const double total = std::max(curve.front(), kTinyEnergy);
    for (double& value : curve)
    {
        value = toDb(value / total);
    }
    return curve;
}

DecayTimes getDecayTimes(const std::vector<double>& curveDb, double sampleRate)
{
    DecayTimes result;
    if (curveDb.empty())
    {
        return result;
    }
    result.rangeDb = -curveDb.back();
    // a line by least squares over the samples between two levels (from the first time the curve falls below `top` to the first below `bottom`)
    const auto fit = [&curveDb, sampleRate](double top, double bottom, double& correlation)
    {
        size_t first = curveDb.size();
        size_t last = curveDb.size();
        for (size_t index = 0; index < curveDb.size(); ++index)
        {
            if (first == curveDb.size() && curveDb[index] <= top)
            {
                first = index;
            }
            if (curveDb[index] <= bottom)
            {
                last = index;
                break;
            }
        }
        if (first >= last || last == curveDb.size())
        {
            return 0.0;
        }
        double n = 0.0;
        double st = 0.0;
        double sl = 0.0;
        double stt = 0.0;
        double stl = 0.0;
        double sll = 0.0;
        for (size_t index = first; index <= last; ++index)
        {
            const double t = static_cast<double>(index) / sampleRate;
            const double l = curveDb[index];
            n += 1.0;
            st += t;
            sl += l;
            stt += t * t;
            stl += t * l;
            sll += l * l;
        }
        const double covariance = n * stl - st * sl;
        const double slope = covariance / (n * stt - st * st);
        correlation = std::abs(covariance) / std::sqrt((n * stt - st * st) * (n * sll - sl * sl));
        if (slope >= 0.0)
        {
            return 0.0;
        }
        return -60.0 / slope;
    };
    double unused = 0.0;
    result.edt = fit(0.0, -10.0, unused);
    result.t20 = fit(-5.0, -25.0, unused);
    result.t30 = fit(-5.0, -35.0, result.t30Correlation);
    return result;
}

namespace
{
// C_t = 10 lg(E[0, t) / E[t, end)) from `start`
double getClarity(const std::vector<double>& response, int start, int end, int split)
{
    double early = 0.0;
    double late = 0.0;
    for (int sample = start; sample < end; ++sample)
    {
        const double energy = response[static_cast<size_t>(sample)] * response[static_cast<size_t>(sample)];
        if (sample < start + split)
        {
            early += energy;
        }
        else
        {
            late += energy;
        }
    }
    return toDb(early / std::max(late, kTinyEnergy));
}

// per band: the decay from the reverberant part (from `decayStart`, the direct path removed before the filter), C80 from the whole response (from
// `start`, the direct path)
std::vector<BandDecay> analyseBands(const std::vector<double>& response, const std::vector<double>& reverberant, int fraction, int start, int decayStart,
                                    const ReverbSettings& settings)
{
    std::vector<BandDecay> result;
    const int split80 = static_cast<int>(0.08 * settings.sampleRate);
    for (const Band& band : getBands(fraction, settings.lowestCentreHz, settings.highestCentreHz))
    {
        if (band.highHz >= 0.5 * settings.sampleRate)
        {
            continue;
        }
        const std::vector<double> filtered = filterBand(reverberant, band, settings.sampleRate, settings.filterOrder);
        BandDecay decay;
        decay.band = band;
        const Truncation truncation = findTruncation(filtered, decayStart, settings.sampleRate);
        decay.times = getDecayTimes(getDecayCurve(filtered, decayStart, truncation), settings.sampleRate);
        if (&reverberant == &response)
        {
            decay.c80Db = getClarity(filtered, start, truncation.sample, split80);
        }
        else
        {
            const std::vector<double> whole = filterBand(response, band, settings.sampleRate, settings.filterOrder);
            decay.c80Db = getClarity(whole, start, truncation.sample, split80);
        }
        result.push_back(decay);
    }
    return result;
}
}

ReverbAnalysis analyseReverb(const std::vector<double>& response, const ReverbSettings& settings)
{
    ReverbAnalysis result;
    const int length = static_cast<int>(response.size());
    if (length == 0)
    {
        return result;
    }
    const double fs = settings.sampleRate;
    double largest = 0.0;
    for (const double value : response)
    {
        largest = std::max(largest, std::abs(value));
    }
    // the direct path: the peak within a millisecond after the latency
    const int latency = std::clamp(static_cast<int>(std::lround(settings.latencySamples)), 0, length - 1);
    const int searchEnd = std::min(length, latency + static_cast<int>(0.001 * fs) + 1);
    int peak = latency;
    for (int sample = latency; sample < searchEnd; ++sample)
    {
        if (std::abs(response[static_cast<size_t>(sample)]) > std::abs(response[static_cast<size_t>(peak)]))
        {
            peak = sample;
        }
    }
    result.hasDirect = std::abs(response[static_cast<size_t>(peak)]) >= largest * std::pow(10.0, -settings.directBelowDb / 20.0);
    const int window = static_cast<int>(settings.directWindowSeconds * fs);
    int reverbFrom = latency;
    result.directSample = latency;
    if (result.hasDirect)
    {
        result.directSample = peak;
        result.directPathDelaySamples = peak;
        const int from = std::max(0, peak - window);
        const int to = std::min(length, peak + window + 1);
        double direct = 0.0;
        double total = 0.0;
        for (int sample = 0; sample < length; ++sample)
        {
            const double energy = response[static_cast<size_t>(sample)] * response[static_cast<size_t>(sample)];
            total += energy;
            if (sample >= from && sample < to)
            {
                direct += energy;
            }
        }
        result.directToReverberantDb = toDb(direct / std::max(total - direct, kTinyEnergy));
        reverbFrom = to;
    }
    // the onset: the first sample after the direct path within onsetBelowDb of the reverb's peak
    double reverbPeak = 0.0;
    for (int sample = reverbFrom; sample < length; ++sample)
    {
        reverbPeak = std::max(reverbPeak, std::abs(response[static_cast<size_t>(sample)]));
    }
    const double onsetLevel = reverbPeak * std::pow(10.0, -settings.onsetBelowDb / 20.0);
    for (int sample = reverbFrom; sample < length; ++sample)
    {
        if (std::abs(response[static_cast<size_t>(sample)]) >= onsetLevel && reverbPeak > 0.0)
        {
            result.preDelaySeconds = (sample - result.directSample) / fs;
            result.onsetSample = sample;
            break;
        }
    }

    // the decay from the reverberant part: with a direct path it is cut out (+-directWindowSeconds) and the curves start at the reverb's onset
    // (a plugin's dry signal is often 10 ... 20 dB above its tail: the ranges of ISO 3382-1, made for rooms, would see a few dB of the tail only);
    // the clarity, D50 and Ts keep the direct path, as defined
    const int start = static_cast<int>(result.directSample);
    std::vector<double> withoutDirect;
    int decayStart = start;
    if (result.hasDirect)
    {
        withoutDirect = response;
        for (int sample = std::max(0, peak - window); sample < std::min(length, peak + window + 1); ++sample)
        {
            withoutDirect[static_cast<size_t>(sample)] = 0.0;
        }
        decayStart = std::max(start, static_cast<int>(result.onsetSample));
    }
    const std::vector<double>* reverberant = &response;
    if (result.hasDirect)
    {
        reverberant = &withoutDirect;
    }
    result.truncation = findTruncation(*reverberant, decayStart, fs);
    result.broadband = getDecayTimes(getDecayCurve(*reverberant, decayStart, result.truncation), fs);
    const int end = result.truncation.sample;
    result.c50Db = getClarity(response, start, end, static_cast<int>(0.05 * fs));
    result.c80Db = getClarity(response, start, end, static_cast<int>(0.08 * fs));
    double early = 0.0;
    double total = 0.0;
    double moment = 0.0;
    for (int sample = start; sample < end; ++sample)
    {
        const double energy = response[static_cast<size_t>(sample)] * response[static_cast<size_t>(sample)];
        total += energy;
        moment += energy * (sample - start) / fs;
        if (sample < start + static_cast<int>(0.05 * fs))
        {
            early += energy;
        }
    }
    result.d50 = early / std::max(total, kTinyEnergy);
    result.centreTimeSeconds = moment / std::max(total, kTinyEnergy);
    result.octaves = analyseBands(response, *reverberant, 1, start, decayStart, settings);
    result.thirdOctaves = analyseBands(response, *reverberant, 3, start, decayStart, settings);
    return result;
}
}
