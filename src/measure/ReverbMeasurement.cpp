#include "pluginlab/measure/Reverb.h"

#include <algorithm>
#include <cmath>

namespace pluginlab::measure
{
namespace
{
constexpr double kDiffuse = 0.9;
constexpr double kShortestDecaySeconds = 0.1;
constexpr double kEarlySeconds = 0.08;
}

ReverbMeasurement measureReverbResponse(const Device& device, const ReverbMeasurementSettings& settings)
{
    ReverbMeasurement result;
    const double fs = settings.sampleRate;
    const int length = static_cast<int>(settings.renderSeconds * fs);
    const double height = std::pow(10.0, settings.levelDbfs / 20.0);
    juce::AudioBuffer<float> input(settings.channels, length);
    input.clear();
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        input.setSample(channel, 0, static_cast<float>(height));
    }
    const juce::AudioBuffer<float> output = device(input, fs);
    if (output.getNumChannels() == 0)
    {
        return result;
    }
    std::vector<std::vector<double>> responses(static_cast<size_t>(output.getNumChannels()));
    for (int channel = 0; channel < output.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < output.getNumSamples(); ++sample)
        {
            responses[static_cast<size_t>(channel)].push_back(output.getSample(channel, sample) / height);
        }
    }
    const std::vector<double>& first = responses.front();
    ReverbSettings reverb;
    reverb.sampleRate = fs;
    reverb.latencySamples = settings.latencySamples;
    result.decay = analyseReverb(first, reverb);
    const int onset = static_cast<int>(result.decay.directSample + result.decay.preDelaySeconds * fs);
    const int end = result.decay.truncation.sample;
    TextureSettings texture;
    texture.sampleRate = fs;
    result.texture = analyseTexture(first, onset, end, texture);
    result.hasReverb = result.texture.mixingTimeSeconds >= 0.0 && result.texture.mixingTimeSeconds <= settings.diffuseWithinSeconds
                       && result.decay.broadband.t30 > kShortestDecaySeconds;
    if (! result.hasReverb || responses.size() < 2)
    {
        return result;
    }
    result.stereo = true;
    const std::vector<double>& second = responses[1];
    const int direct = static_cast<int>(result.decay.directSample);
    result.iaccEarly = getIacc(first, second, direct, 0.0, kEarlySeconds, fs);
    result.iaccLate = getIacc(first, second, direct, kEarlySeconds, std::max(kEarlySeconds, (end - direct) / fs), fs);
    const Coherence coherence = getCoherence(first, second, direct + static_cast<int>(kEarlySeconds * fs), end, fs);
    double sum = 0.0;
    int bins = 0;
    for (size_t bin = 0; bin < coherence.frequencies.size(); ++bin)
    {
        if (coherence.frequencies[bin] >= 200.0 && coherence.frequencies[bin] <= 8000.0)
        {
            sum += coherence.coherence[bin];
            ++bins;
        }
    }
    result.meanCoherence = sum / std::max(1, bins);
    result.lateral = getLateralDecay(first, second, direct, fs);
    return result;
}

const char* getJndVerdictName(JndVerdict verdict)
{
    if (verdict == JndVerdict::Same)
    {
        return "the same";
    }
    if (verdict == JndVerdict::Close)
    {
        return "audibly close";
    }
    return "different";
}

namespace
{
double getPercent(double a, double b)
{
    if (a <= 0.0)
    {
        return 0.0;
    }
    return 100.0 * (b / a - 1.0);
}

JndVerdict judge(double percent, double sameShare, double closeShare)
{
    if (std::abs(percent) <= 100.0 * sameShare)
    {
        return JndVerdict::Same;
    }
    if (std::abs(percent) <= 100.0 * closeShare)
    {
        return JndVerdict::Close;
    }
    return JndVerdict::Different;
}

std::vector<BandComparison> compareBands(const std::vector<BandDecay>& a, const std::vector<BandDecay>& b, double sameShare, double closeShare)
{
    std::vector<BandComparison> result;
    for (size_t index = 0; index < a.size() && index < b.size(); ++index)
    {
        BandComparison band;
        band.band = a[index].band;
        band.t30A = a[index].times.t30;
        band.t30B = b[index].times.t30;
        band.differencePercent = getPercent(band.t30A, band.t30B);
        band.verdict = judge(band.differencePercent, sameShare, closeShare);
        result.push_back(band);
    }
    return result;
}
}

ReverbComparison compareReverbs(const ReverbMeasurement& a, const ReverbMeasurement& b, double sameShare, double closeShare)
{
    ReverbComparison result;
    result.octaves = compareBands(a.decay.octaves, b.decay.octaves, sameShare, closeShare);
    result.thirdOctaves = compareBands(a.decay.thirdOctaves, b.decay.thirdOctaves, sameShare, closeShare);
    for (const BandComparison& band : result.octaves)
    {
        if (band.verdict == JndVerdict::Same)
        {
            ++result.octavesSame;
        }
        else if (band.verdict == JndVerdict::Close)
        {
            ++result.octavesClose;
        }
        else
        {
            ++result.octavesDifferent;
        }
    }
    result.t30DifferencePercent = getPercent(a.decay.broadband.t30, b.decay.broadband.t30);
    result.t30Verdict = judge(result.t30DifferencePercent, sameShare, closeShare);
    result.edtDifferencePercent = getPercent(a.decay.broadband.edt, b.decay.broadband.edt);
    result.c80DifferenceDb = b.decay.c80Db - a.decay.c80Db;
    result.preDelayDifferenceMs = 1000.0 * (b.decay.preDelaySeconds - a.decay.preDelaySeconds);
    result.drrDifferenceDb = b.decay.directToReverberantDb - a.decay.directToReverberantDb;
    result.mixingDifferenceMs = 1000.0 * (b.texture.mixingTimeSeconds - a.texture.mixingTimeSeconds);
    result.lateKurtosisDifference = b.texture.lateKurtosis - a.texture.lateKurtosis;
    result.iaccLateDifference = b.iaccLate.value - a.iaccLate.value;
    return result;
}
}
