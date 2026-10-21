#include "pluginlab/measure/EchoResponse.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include <juce_dsp/juce_dsp.h>

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDirectToleranceSamples = 0.5;
constexpr double kTinyPower = 1.0e-30;

using Spectrum = std::vector<std::complex<double>>;

int getOrderAtLeast(int samples)
{
    int order = 0;
    while ((1 << order) < samples)
    {
        ++order;
    }
    return order;
}

double toDb(double magnitude)
{
    return 20.0 * std::log10(std::max(magnitude, 1.0e-15));
}

// the peaks of |h| above the threshold that stand out of their surroundings (the peak at least `crest` times the RMS within half a spacing
// around it: the slow undershoot that a high-pass in the loop leaves behind each echo is no arrival), at least `spacing` apart, the largest
// first, then sorted by time
std::vector<int> findArrivals(const std::vector<double>& response, double threshold, int spacing, double crest, int maximum)
{
    const int length = static_cast<int>(response.size());
    std::vector<double> energy(static_cast<size_t>(length + 1), 0.0); // energy[n] = sum of h^2 before n
    for (int index = 0; index < length; ++index)
    {
        energy[static_cast<size_t>(index + 1)] = energy[static_cast<size_t>(index)] + response[static_cast<size_t>(index)] * response[static_cast<size_t>(index)];
    }
    const int half = std::max(1, spacing / 2);
    std::vector<int> candidates;
    for (int index = 0; index < static_cast<int>(response.size()); ++index)
    {
        const double value = std::abs(response[static_cast<size_t>(index)]);
        if (value < threshold)
        {
            continue;
        }
        bool isPeak = true;
        if (index > 0 && std::abs(response[static_cast<size_t>(index - 1)]) > value)
        {
            isPeak = false;
        }
        if (index + 1 < static_cast<int>(response.size()) && std::abs(response[static_cast<size_t>(index + 1)]) >= value)
        {
            isPeak = false;
        }
        const int from = std::max(0, index - half);
        const int to = std::min(length, index + half + 1);
        const double rms = std::sqrt((energy[static_cast<size_t>(to)] - energy[static_cast<size_t>(from)]) / (to - from));
        if (isPeak && value >= crest * rms)
        {
            candidates.push_back(index);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [&response](int a, int b)
              { return std::abs(response[static_cast<size_t>(a)]) > std::abs(response[static_cast<size_t>(b)]); });
    std::vector<int> accepted;
    for (const int candidate : candidates)
    {
        const bool isNear = std::any_of(accepted.begin(), accepted.end(), [candidate, spacing](int other) { return std::abs(other - candidate) < spacing; });
        if (! isNear)
        {
            accepted.push_back(candidate);
        }
    }
    std::sort(accepted.begin(), accepted.end());
    if (static_cast<int>(accepted.size()) > maximum)
    {
        accepted.resize(static_cast<size_t>(maximum));
    }
    return accepted;
}

// the spectrum of response[start, end) placed at the start of an FFT of the given order (bins 0 ... size/2)
Spectrum getSpectrum(const std::vector<double>& response, int start, int end, int order)
{
    const int size = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<juce::dsp::Complex<float>> time(static_cast<size_t>(size)), frequency(static_cast<size_t>(size));
    for (int index = start; index < end; ++index)
    {
        time[static_cast<size_t>(index - start)] = static_cast<float>(response[static_cast<size_t>(index)]);
    }
    fft.perform(time.data(), frequency.data(), false);
    Spectrum spectrum(static_cast<size_t>(size / 2 + 1));
    for (size_t bin = 0; bin < spectrum.size(); ++bin)
    {
        spectrum[bin] = {frequency[bin].real(), frequency[bin].imag()};
    }
    return spectrum;
}

// an arrival from its window's spectrum: X = g e^(-j w tau), tau = the peak's offset in the window plus a fraction from the phase slope
// (weighted least squares in the band, as the null test), g = the mean of Re(X e^(j w tau)) in the band
Arrival measureArrival(const Spectrum& spectrum, int windowStart, int peakOffset, int size, int firstBin, int lastBin)
{
    double realSum = 0.0;
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        const double omega = 2.0 * kPi * bin / size;
        realSum += (spectrum[static_cast<size_t>(bin)] * std::polar(1.0, omega * peakOffset)).real();
    }
    double sign = 1.0;
    if (realSum < 0.0)
    {
        sign = -1.0;
    }
    double numerator = 0.0;
    double denominator = 0.0;
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        const double omega = 2.0 * kPi * bin / size;
        const std::complex<double> rotated = sign * spectrum[static_cast<size_t>(bin)] * std::polar(1.0, omega * peakOffset);
        const double weight = std::abs(rotated);
        numerator += weight * omega * std::arg(rotated);
        denominator += weight * omega * omega;
    }
    double fraction = 0.0;
    if (denominator > 0.0)
    {
        fraction = -numerator / denominator;
    }
    const double tau = peakOffset + fraction;
    double gain = 0.0;
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        const double omega = 2.0 * kPi * bin / size;
        gain += (spectrum[static_cast<size_t>(bin)] * std::polar(1.0, omega * tau)).real();
    }
    gain /= (lastBin - firstBin + 1);

    Arrival arrival;
    arrival.timeSamples = windowStart + tau;
    arrival.gainDb = toDb(std::abs(gain));
    arrival.inverted = gain < 0.0;
    return arrival;
}
}

EchoAnalysis analyseEchoes(const std::vector<double>& response, const EchoSettings& settings)
{
    EchoAnalysis result;
    double largest = 0.0;
    for (const double value : response)
    {
        largest = std::max(largest, std::abs(value));
    }
    if (largest <= 0.0)
    {
        return result;
    }
    const int spacing = std::max(1, juce::roundToInt(settings.minimumSpacingSeconds * settings.sampleRate));
    const std::vector<int> peaks = findArrivals(response, largest * std::pow(10.0, settings.thresholdDb / 20.0), spacing, settings.minimumCrest,
                                                  settings.maximumEchoes + 1);
    if (peaks.empty())
    {
        return result;
    }
    result.found = true;

    // each arrival's window: from the middle to the arrival before to the middle to the one after (the first and the last as wide on the outside)
    const int count = static_cast<int>(peaks.size());
    std::vector<int> starts(static_cast<size_t>(count)), ends(static_cast<size_t>(count));
    int widest = 1;
    for (int index = 0; index < count; ++index)
    {
        int halfBefore = spacing / 2;
        int halfAfter = spacing / 2;
        if (index > 0)
        {
            halfBefore = (peaks[static_cast<size_t>(index)] - peaks[static_cast<size_t>(index - 1)]) / 2;
        }
        if (index + 1 < count)
        {
            halfAfter = (peaks[static_cast<size_t>(index + 1)] - peaks[static_cast<size_t>(index)]) / 2;
        }
        if (index == 0 && count > 1)
        {
            halfBefore = halfAfter;
        }
        if (index + 1 == count && count > 1)
        {
            halfAfter = halfBefore;
        }
        starts[static_cast<size_t>(index)] = std::max(0, peaks[static_cast<size_t>(index)] - halfBefore);
        ends[static_cast<size_t>(index)] = std::min(static_cast<int>(response.size()), peaks[static_cast<size_t>(index)] + halfAfter);
        widest = std::max(widest, ends[static_cast<size_t>(index)] - starts[static_cast<size_t>(index)]);
    }
    const int order = getOrderAtLeast(2 * widest);
    const int size = 1 << order;
    const int firstBin = std::max(1, static_cast<int>(std::ceil(settings.bandLowHz * size / settings.sampleRate)));
    const int lastBin = std::max(firstBin, std::min(size / 2 - 1, static_cast<int>(std::floor(settings.bandHighHz * size / settings.sampleRate))));

    std::vector<Spectrum> spectra;
    std::vector<Arrival> arrivals;
    for (int index = 0; index < count; ++index)
    {
        const int start = starts[static_cast<size_t>(index)];
        spectra.push_back(getSpectrum(response, start, ends[static_cast<size_t>(index)], order));
        arrivals.push_back(measureArrival(spectra.back(), start, peaks[static_cast<size_t>(index)] - start, size, firstBin, lastBin));
    }

    // the direct path: the arrival at the latency
    size_t firstEcho = 0;
    if (std::abs(arrivals.front().timeSamples - settings.latencySamples) <= kDirectToleranceSamples)
    {
        result.hasDirect = true;
        result.direct = arrivals.front();
        firstEcho = 1;
    }
    result.echoes.assign(arrivals.begin() + static_cast<std::ptrdiff_t>(firstEcho), arrivals.end());
    if (static_cast<int>(result.echoes.size()) > settings.maximumEchoes)
    {
        result.echoes.resize(static_cast<size_t>(settings.maximumEchoes));
    }
    if (result.echoes.empty())
    {
        return result;
    }
    double origin = settings.latencySamples;
    if (result.hasDirect)
    {
        origin = result.direct.timeSamples;
        result.mixDb = result.echoes.front().gainDb - result.direct.gainDb;
    }
    result.delaySamples = result.echoes.front().timeSamples - origin;

    const size_t echoes = result.echoes.size();
    if (echoes >= 2)
    {
        const double last = result.echoes.back().timeSamples;
        result.spacingSamples = (last - result.echoes.front().timeSamples) / static_cast<double>(echoes - 1);
        double stepSum = 0.0;
        for (size_t echo = 1; echo < echoes; ++echo)
        {
            const double distance = result.echoes[echo].timeSamples - result.echoes[echo - 1].timeSamples;
            result.spacingSpreadSamples = std::max(result.spacingSpreadSamples, std::abs(distance - result.spacingSamples));
            stepSum += result.echoes[echo].gainDb - result.echoes[echo - 1].gainDb;
        }
        result.feedbackDb = stepSum / static_cast<double>(echoes - 1);
        result.decayDbPerSecond = result.feedbackDb / (result.spacingSamples / settings.sampleRate);
    }

    // the loop filter: |X(k+1)| / |X(k)| per bin, the mean in dB over the pairs where both are above the floor
    const size_t bins = static_cast<size_t>(size / 2 + 1);
    double strongest = 0.0;
    for (size_t echo = firstEcho; echo < spectra.size(); ++echo)
    {
        for (const std::complex<double>& value : spectra[echo])
        {
            strongest = std::max(strongest, std::norm(value));
        }
    }
    const double floor = strongest * std::pow(10.0, settings.loopFloorDb / 10.0);
    result.loopFrequencies.resize(bins);
    result.loopMagnitudeDb.assign(bins, 0.0);
    result.loopValid.assign(bins, false);
    for (size_t bin = 0; bin < bins; ++bin)
    {
        result.loopFrequencies[bin] = static_cast<double>(bin) * settings.sampleRate / size;
        double sum = 0.0;
        int pairs = 0;
        for (size_t echo = firstEcho + 1; echo < firstEcho + echoes; ++echo)
        {
            const double before = std::norm(spectra[echo - 1][bin]);
            const double after = std::norm(spectra[echo][bin]);
            if (before > floor && after > floor)
            {
                sum += 10.0 * std::log10(std::max(after, kTinyPower) / before);
                ++pairs;
            }
        }
        if (pairs > 0)
        {
            result.loopMagnitudeDb[bin] = sum / pairs;
            result.loopValid[bin] = true;
        }
    }
    return result;
}

StereoEchoAnalysis analyseStereoEchoes(const std::vector<double> (&responses)[2][2], const EchoSettings& settings)
{
    StereoEchoAnalysis result;
    bool crossEchoes = false;
    bool alternating = true;
    int alternatingInputs = 0;
    double spacingSum = 0.0;
    int spacingCount = 0;
    for (size_t input = 0; input < 2; ++input)
    {
        for (size_t output = 0; output < 2; ++output)
        {
            result.paths[input][output] = analyseEchoes(responses[input][output], settings);
        }
        const size_t other = 1 - input;
        if (result.paths[input][other].echoes.empty() && ! result.paths[input][other].hasDirect)
        {
            continue;
        }
        crossEchoes = true;
        // the echoes of this input over both outputs in the order of time: ping-pong if the output changes from each echo to the next. One
        // threshold for both outputs (each path's own is relative to its largest arrival: the side with the dry path would stop earlier)
        double loudest = -1.0e9;
        for (size_t output = 0; output < 2; ++output)
        {
            const EchoAnalysis& path = result.paths[input][output];
            if (path.hasDirect)
            {
                loudest = std::max(loudest, path.direct.gainDb);
            }
            for (const Arrival& echo : path.echoes)
            {
                loudest = std::max(loudest, echo.gainDb);
            }
        }
        std::vector<std::pair<double, size_t>> merged;
        for (size_t output = 0; output < 2; ++output)
        {
            for (const Arrival& echo : result.paths[input][output].echoes)
            {
                if (echo.gainDb >= loudest + settings.thresholdDb)
                {
                    merged.emplace_back(echo.timeSamples, output);
                }
            }
        }
        std::sort(merged.begin(), merged.end());
        constexpr size_t kFewestEchoes = 3;
        if (merged.size() < kFewestEchoes)
        {
            alternating = false;
            continue;
        }
        for (size_t index = 1; index < merged.size(); ++index)
        {
            if (merged[index].second == merged[index - 1].second)
            {
                alternating = false;
            }
            spacingSum += merged[index].first - merged[index - 1].first;
            ++spacingCount;
        }
        ++alternatingInputs;
    }
    if (! crossEchoes)
    {
        return result;
    }
    result.kind = StereoEchoKind::CrossFeedback;
    if (alternating && alternatingInputs > 0)
    {
        result.kind = StereoEchoKind::PingPong;
        result.pingPongSpacingSamples = spacingSum / spacingCount;
    }
    return result;
}

LevelDependence analyseLevelDependence(const std::vector<double>& levelsDbfs, const std::vector<EchoAnalysis>& analyses, double thresholdDb)
{
    constexpr double kNotDecayingDb = -0.05;
    constexpr size_t kFewestEchoes = 3;
    LevelDependence result;
    for (size_t index = 0; index < levelsDbfs.size() && index < analyses.size(); ++index)
    {
        const EchoAnalysis& analysis = analyses[index];
        if (analysis.echoes.empty())
        {
            continue;
        }
        EchoLevel level;
        level.levelDbfs = levelsDbfs[index];
        level.firstEchoGainDb = analysis.echoes.front().gainDb;
        level.feedbackDb = analysis.feedbackDb;
        level.echoes = static_cast<int>(analysis.echoes.size());
        result.levels.push_back(level);
        if (analysis.echoes.size() >= kFewestEchoes && analysis.feedbackDb > kNotDecayingDb)
        {
            result.selfOscillating = true;
        }
    }
    std::sort(result.levels.begin(), result.levels.end(), [](const EchoLevel& a, const EchoLevel& b) { return a.levelDbfs < b.levelDbfs; });
    if (result.levels.size() < 2)
    {
        return result;
    }
    result.compressionDb = result.levels.back().firstEchoGainDb - result.levels.front().firstEchoGainDb;
    result.feedbackChangeDb = result.levels.back().feedbackDb - result.levels.front().feedbackDb;
    result.saturates = result.compressionDb < -thresholdDb || result.feedbackChangeDb < -thresholdDb;
    return result;
}
}
