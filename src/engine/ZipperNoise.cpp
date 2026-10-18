#include "pluginlab/engine/ZipperNoise.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "ZipperMeasurement.h"
#include "pluginlab/measure/Analyzer.h"

namespace pluginlab::engine
{
namespace
{
constexpr int kAnalysisLength = 65536;          // 1.37 s at 48 kHz: the ramp (a power of two for the FFT)
constexpr double kRunInSeconds = 0.5;           // the tones before the analysis: filters and envelopes reach their steady state
constexpr double kToneAmplitude = 0.1;          // -20 dBFS each; five tones stay below -3 dBFS together
// a smooth ramp of the level spreads a tone by some tens of hertz (a 24 dB ramp: -67 dB at 30 Hz off); steps once per block put their energy at
// multiples of the block rate (94 Hz at 512 samples) around every tone, and broadband: the guard is just under half the block rate
constexpr double kGuardHz = 45.0;
constexpr double kHighestFractionOfRate = 0.45;
constexpr double kNothingDb = -200.0;
constexpr int kTonePhaseSeed = 3;
const double kWantedTonesHz[] = {100.0, 315.0, 1000.0, 3150.0, 10000.0};

double powerToDb(double power)
{
    return power > 0.0 ? 10.0 * std::log10(power) : kNothingDb;
}

// 4-term Blackman-Harris: side lobes below -92 dB, main lobe +-4 bins
std::vector<double> makeBlackmanHarris(int length)
{
    constexpr double a0 = 0.35875;
    constexpr double a1 = 0.48829;
    constexpr double a2 = 0.14128;
    constexpr double a3 = 0.01168;
    std::vector<double> window(static_cast<size_t>(length));
    const double step = 2.0 * juce::MathConstants<double>::pi / static_cast<double>(length);
    for (int index = 0; index < length; ++index)
    {
        const double x = step * index;
        window[static_cast<size_t>(index)] = a0 - a1 * std::cos(x) + a2 * std::cos(2.0 * x) - a3 * std::cos(3.0 * x);
    }
    return window;
}
}

ToneResidual measureToneResidual(const std::vector<float>& signal, double sampleRate, const std::vector<double>& toneHz, double guardHz, int maximumHarmonic,
                                 double lowestHz, double highestHz)
{
    ToneResidual result;
    int length = 1;
    while (length * 2 <= static_cast<int>(signal.size()))
    {
        length *= 2;
    }
    if (length < 2)
    {
        return result;
    }
    const std::vector<double> window = makeBlackmanHarris(length);
    std::vector<double> data(static_cast<size_t>(length));
    double windowPower = 0.0;
    const size_t offset = signal.size() - static_cast<size_t>(length); // the end of the signal
    for (int index = 0; index < length; ++index)
    {
        const double w = window[static_cast<size_t>(index)];
        data[static_cast<size_t>(index)] = w * signal[offset + static_cast<size_t>(index)];
        windowPower += w * w;
    }
    const std::vector<std::complex<double>> spectrum = measure::getSpectrum(data, 0, length);
    const double binHz = sampleRate / length;
    const double top = std::min(highestHz, kHighestFractionOfRate * sampleRate);
    // Parseval for a one-sided spectrum of a windowed signal: the mean square of the signal; a full-scale sine is 0 dBFS (AES17: 20 lg(rms sqrt 2))
    const double scale = 2.0 * 2.0 / (static_cast<double>(length) * windowPower);
    double tones = 0.0;
    double residual = 0.0;
    for (size_t bin = 1; bin < spectrum.size(); ++bin)
    {
        const double frequency = static_cast<double>(bin) * binHz;
        if (frequency < lowestHz || frequency > top)
        {
            continue;
        }
        const double power = std::norm(spectrum[bin]) * scale;
        bool nearFundamental = false;
        bool nearHarmonic = false;
        for (const double tone : toneHz)
        {
            for (int harmonic = 1; harmonic <= maximumHarmonic; ++harmonic)
            {
                if (std::abs(frequency - harmonic * tone) <= guardHz)
                {
                    (harmonic == 1 ? nearFundamental : nearHarmonic) = true;
                }
            }
        }
        if (nearFundamental)
        {
            tones += power;
        }
        else if (! nearHarmonic)
        {
            residual += power;
        }
    }
    result.tonesDbfs = powerToDb(tones);
    result.residualDbfs = powerToDb(residual);
    result.relativeDb = tones > 0.0 && residual > 0.0 ? powerToDb(residual / tones) : kNothingDb;
    return result;
}

std::vector<double> getZipperTones(double sampleRate, int analysisLength)
{
    std::vector<double> tones;
    for (const double wanted : kWantedTonesHz)
    {
        if (wanted < kHighestFractionOfRate * sampleRate)
        {
            tones.push_back(measure::getCoherentFrequency(wanted, sampleRate, analysisLength));
        }
    }
    return tones;
}

namespace detail
{
namespace
{
constexpr int kNoIndex = -1;

Signal makeTones(const std::vector<double>& tonesHz, double sampleRate, int length)
{
    juce::Random random(kTonePhaseSeed);
    std::vector<float> samples(static_cast<size_t>(length), 0.0f);
    for (const double tone : tonesHz)
    {
        const double phase = random.nextDouble() * 2.0 * juce::MathConstants<double>::pi;
        const double step = 2.0 * juce::MathConstants<double>::pi * tone / sampleRate;
        for (int index = 0; index < length; ++index)
        {
            samples[static_cast<size_t>(index)] += static_cast<float>(kToneAmplitude * std::sin(phase + step * index));
        }
    }
    return Signal{samples};
}

Signal sliceSignal(const Signal& signal, size_t from, size_t to)
{
    Signal part;
    for (const std::vector<float>& channel : signal)
    {
        part.emplace_back(channel.begin() + static_cast<std::ptrdiff_t>(std::min(from, channel.size())),
                          channel.begin() + static_cast<std::ptrdiff_t>(std::min(to, channel.size())));
    }
    return part;
}

// The tones through a fresh instance at `setting` (the careful way of the fingerprint: poke, setting, settle), then the analysis part; with
// rampIndex >= 0 that parameter moves from its value in `setting` to rampTo during the analysis part, one new value per block
std::vector<float> renderTones(const Bench& bench, const std::vector<float>& setting, const Signal& tones, int runIn, int rampIndex, float rampTo,
                               const FingerprintSettings& settings)
{
    const std::unique_ptr<Rig> rig = bench.make(kReferenceRate, kReferenceBlock);
    if (! rig->isValid())
    {
        return {};
    }
    rig->prepare();
    rig->apply(makePokeValues(setting, settings));
    rig->process(makeDifferentNoise(kReferenceBlock, settings.noiseLevel));
    rig->apply(setting);
    rig->settle();
    rig->process(sliceSignal(tones, 0, static_cast<size_t>(runIn)));
    std::vector<float> output;
    output.reserve(static_cast<size_t>(kAnalysisLength));
    const int blocks = kAnalysisLength / kReferenceBlock;
    const float from = rampIndex >= 0 ? setting[static_cast<size_t>(rampIndex)] : 0.0f;
    for (int block = 0; block < blocks; ++block)
    {
        if (rampIndex >= 0)
        {
            rig->set(rampIndex, from + (rampTo - from) * static_cast<float>(block) / static_cast<float>(blocks - 1));
        }
        const size_t start = static_cast<size_t>(runIn + block * kReferenceBlock);
        const Signal out = rig->process(sliceSignal(tones, start, start + static_cast<size_t>(kReferenceBlock)));
        if (out.empty())
        {
            return {};
        }
        output.insert(output.end(), out.front().begin(), out.front().end());
    }
    return output;
}

double getChangeDb(const std::vector<float>& a, const std::vector<float>& b)
{
    double difference = 0.0;
    double reference = 0.0;
    for (size_t index = 0; index < a.size() && index < b.size(); ++index)
    {
        const double d = static_cast<double>(b[index]) - a[index];
        difference += d * d;
        reference += static_cast<double>(a[index]) * a[index];
    }
    if (reference <= 0.0)
    {
        return difference > 0.0 ? 0.0 : kNothingDb;
    }
    return difference > 0.0 ? powerToDb(difference / reference) : kNothingDb;
}
}

ZipperFingerprint measureZipperNoise(const Bench& bench, const std::vector<float>& base, const std::vector<ZipperCandidate>& candidates,
                                     const FingerprintSettings& settings)
{
    ZipperFingerprint result;
    result.measured = true;
    result.blockSize = kReferenceBlock;
    result.rampSeconds = kAnalysisLength / kReferenceRate;
    result.tonesHz = getZipperTones(kReferenceRate, kAnalysisLength);
    const int runIn = static_cast<int>(std::ceil(kRunInSeconds * kReferenceRate / kReferenceBlock)) * kReferenceBlock;
    const Signal tones = makeTones(result.tonesHz, kReferenceRate, runIn + kAnalysisLength);
    const auto residual = [&result](const std::vector<float>& output) {
        return measureToneResidual(output, kReferenceRate, result.tonesHz, kGuardHz).relativeDb;
    };
    for (const ZipperCandidate& candidate : candidates)
    {
        ZipperParameterResult entry;
        entry.index = candidate.index;
        entry.name = candidate.name;
        if (candidate.index < 0 || candidate.index >= static_cast<int>(base.size()))
        {
            continue;
        }
        std::vector<float> low = base;
        low[static_cast<size_t>(candidate.index)] = static_cast<float>(settings.lowSetting);
        std::vector<float> high = base;
        high[static_cast<size_t>(candidate.index)] = static_cast<float>(settings.highSetting);
        const std::vector<float> stillLow = renderTones(bench, low, tones, runIn, kNoIndex, 0.0f, settings);
        const std::vector<float> stillHigh = renderTones(bench, high, tones, runIn, kNoIndex, 0.0f, settings);
        const std::vector<float> ramp = renderTones(bench, low, tones, runIn, candidate.index, static_cast<float>(settings.highSetting), settings);
        if (stillLow.empty() || stillHigh.empty() || ramp.empty())
        {
            entry.note = "the plugin could not be rendered";
            result.parameters.push_back(entry);
            continue;
        }
        entry.changeDb = getChangeDb(stillLow, stillHigh);
        entry.rampDb = residual(ramp);
        entry.stillDb = std::max(residual(stillLow), residual(stillHigh));
        entry.excessDb = entry.rampDb - entry.stillDb;
        if (entry.changeDb < settings.differentAboveDb)
        {
            entry.note = "does not change the tones between the test positions (with the others as in B)";
        }
        else
        {
            entry.judged = true;
            entry.zipper = entry.excessDb > settings.zipperExcessDb && entry.rampDb > settings.zipperAboveDb;
            result.zipperCount += entry.zipper ? 1 : 0;
        }
        result.parameters.push_back(entry);
    }
    return result;
}
}

ZipperFingerprint measureZipperNoise(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& base,
                                     const std::vector<int>& indices, const FingerprintSettings& settings)
{
    const detail::Bench bench(formatManager, description, settings);
    std::vector<detail::ZipperCandidate> candidates;
    const std::unique_ptr<detail::Rig> rig = bench.make(detail::kReferenceRate, detail::kReferenceBlock);
    for (const int index : indices)
    {
        detail::ZipperCandidate candidate;
        candidate.index = index;
        if (rig->isValid() && index >= 0 && index < rig->get().getParameters().size())
        {
            candidate.name = rig->get().getParameters()[index]->getName(64);
        }
        candidates.push_back(candidate);
    }
    return detail::measureZipperNoise(bench, base, candidates, settings);
}
}
