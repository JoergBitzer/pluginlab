#include "pluginlab/signals/Signals.h"

#include <cmath>

#include <juce_audio_formats/juce_audio_formats.h>

namespace pluginlab::signals
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDbFactor = 20.0;
constexpr double kFloorDb = -200.0;
constexpr int kSecondChannelSeedOffset = 4; // the seed of channel n is seed + 4 n for uncorrelated noise
constexpr double kSmpteFirstHz = 60.0;
constexpr double kSmpteSecondHz = 7000.0;
constexpr double kSmpteRatio = 4.0;
constexpr double kCcifFirstHz = 19000.0;
constexpr double kCcifSecondHz = 20000.0;
constexpr double kCcifRatio = 1.0;
constexpr int kPinkRows = 16;             // Voss-McCartney: rows of the generator

// Copies channel 0 into the other channels as the relation says
void applyRelation(juce::AudioBuffer<float>& buffer, ChannelRelation relation)
{
    const int channels = buffer.getNumChannels();
    const int length = buffer.getNumSamples();
    for (int channel = 1; channel < channels; ++channel)
    {
        buffer.copyFrom(channel, 0, buffer, 0, 0, length);
        if (relation == ChannelRelation::Inverted && channel == 1)
        {
            buffer.applyGain(channel, 0, length, -1.0f);
        }
    }
    if (relation == ChannelRelation::LeftOnly)
    {
        for (int channel = 1; channel < channels; ++channel)
        {
            buffer.clear(channel, 0, length);
        }
    }
    if (relation == ChannelRelation::RightOnly && channels > 1)
    {
        buffer.clear(0, 0, length);
        for (int channel = 2; channel < channels; ++channel)
        {
            buffer.clear(channel, 0, length);
        }
    }
}

void applyFades(juce::AudioBuffer<float>& buffer, int fadeSamples)
{
    const int length = buffer.getNumSamples();
    if (fadeSamples <= 0 || 2 * fadeSamples > length)
    {
        return;
    }
    for (int index = 0; index < fadeSamples; ++index)
    {
        const float gain = static_cast<float>(0.5 - 0.5 * std::cos(kPi * index / fadeSamples));
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.setSample(channel, index, buffer.getSample(channel, index) * gain);
            buffer.setSample(channel, length - 1 - index, buffer.getSample(channel, length - 1 - index) * gain);
        }
    }
}

void fillNoise(float* data, int length, NoiseColour colour, int seed)
{
    juce::Random random(seed);
    if (colour == NoiseColour::WhiteUniform)
    {
        for (int index = 0; index < length; ++index)
        {
            data[index] = random.nextFloat() * 2.0f - 1.0f;
        }
        return;
    }
    if (colour == NoiseColour::WhiteGaussian)
    {
        // Box-Muller
        for (int index = 0; index < length; ++index)
        {
            const double first = std::max(1.0e-12, static_cast<double>(random.nextDouble()));
            const double second = random.nextDouble();
            data[index] = static_cast<float>(std::sqrt(-2.0 * std::log(first)) * std::cos(kTwoPi * second));
        }
        return;
    }
    // pink: Voss-McCartney, rows updated at rates halving from row to row
    double rows[kPinkRows] = {};
    double sum = 0.0;
    for (int row = 0; row < kPinkRows; ++row)
    {
        rows[row] = random.nextDouble() * 2.0 - 1.0;
        sum += rows[row];
    }
    for (int index = 0; index < length; ++index)
    {
        // the row to update: the number of trailing zeros of the counter
        int counter = index + 1;
        int row = 0;
        while ((counter & 1) == 0 && row < kPinkRows - 1)
        {
            counter >>= 1;
            ++row;
        }
        sum -= rows[row];
        rows[row] = random.nextDouble() * 2.0 - 1.0;
        sum += rows[row];
        data[index] = static_cast<float>(sum + (random.nextDouble() * 2.0 - 1.0));
    }
}

void scaleToRms(float* data, int length, double targetRms)
{
    double sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        sum += static_cast<double>(data[index]) * data[index];
    }
    const double rms = std::sqrt(sum / std::max(1, length));
    if (rms <= 0.0)
    {
        return;
    }
    const float gain = static_cast<float>(targetRms / rms);
    for (int index = 0; index < length; ++index)
    {
        data[index] *= gain;
    }
}
}

double dbToGain(double decibels)
{
    return std::pow(10.0, decibels / kDbFactor);
}

double gainToDb(double gain)
{
    if (gain <= 0.0)
    {
        return kFloorDb;
    }
    return std::max(kFloorDb, kDbFactor * std::log10(gain));
}

double getPeak(const juce::AudioBuffer<float>& buffer, int channel)
{
    return buffer.getMagnitude(channel, 0, buffer.getNumSamples());
}

double getRms(const juce::AudioBuffer<float>& buffer, int channel, int start, int length)
{
    if (length < 0)
    {
        length = buffer.getNumSamples() - start;
    }
    double sum = 0.0;
    const float* data = buffer.getReadPointer(channel);
    for (int index = start; index < start + length; ++index)
    {
        sum += static_cast<double>(data[index]) * data[index];
    }
    return std::sqrt(sum / std::max(1, length));
}

juce::AudioBuffer<float> makeSilence(int length, int channels)
{
    juce::AudioBuffer<float> buffer(channels, length);
    buffer.clear();
    return buffer;
}

juce::AudioBuffer<float> makeImpulse(int length, int position, double amplitude, int channels)
{
    juce::AudioBuffer<float> buffer = makeSilence(length, channels);
    for (int channel = 0; channel < channels; ++channel)
    {
        buffer.setSample(channel, position, static_cast<float>(amplitude));
    }
    return buffer;
}

juce::AudioBuffer<float> makeStep(int length, int position, double amplitude, int channels)
{
    juce::AudioBuffer<float> buffer = makeSilence(length, channels);
    for (int channel = 0; channel < channels; ++channel)
    {
        for (int index = position; index < length; ++index)
        {
            buffer.setSample(channel, index, static_cast<float>(amplitude));
        }
    }
    return buffer;
}

double snapToBin(double frequencyHz, double sampleRate, int fftSize)
{
    const double binWidth = sampleRate / fftSize;
    return std::max(1.0, std::round(frequencyHz / binWidth)) * binWidth;
}

juce::AudioBuffer<float> makeSine(const SineSettings& settings, int channels, ChannelRelation relation)
{
    juce::AudioBuffer<float> buffer(channels, settings.length);
    const double amplitude = dbToGain(settings.levelDbfsPeak);
    float* data = buffer.getWritePointer(0);
    for (int index = 0; index < settings.length; ++index)
    {
        data[index] = static_cast<float>(amplitude * std::sin(kTwoPi * settings.frequencyHz * index / settings.sampleRate + settings.phaseRadians));
    }
    applyRelation(buffer, relation);
    applyFades(buffer, settings.fadeSamples);
    return buffer;
}

TwoToneSettings makeSmpteSettings(double sampleRate, int length)
{
    TwoToneSettings settings;
    settings.sampleRate = sampleRate;
    settings.firstHz = kSmpteFirstHz;
    settings.secondHz = kSmpteSecondHz;
    settings.amplitudeRatio = kSmpteRatio;
    settings.length = length;
    return settings;
}

TwoToneSettings makeCcifSettings(double sampleRate, int length)
{
    TwoToneSettings settings;
    settings.sampleRate = sampleRate;
    settings.firstHz = kCcifFirstHz;
    settings.secondHz = kCcifSecondHz;
    settings.amplitudeRatio = kCcifRatio;
    settings.length = length;
    return settings;
}

juce::AudioBuffer<float> makeTwoTone(const TwoToneSettings& settings, int channels, ChannelRelation relation)
{
    juce::AudioBuffer<float> buffer(channels, settings.length);
    // amplitudes a1 = ratio a2, peak of the sum = a1 + a2
    const double peak = dbToGain(settings.levelDbfsPeak);
    const double second = peak / (1.0 + settings.amplitudeRatio);
    const double first = second * settings.amplitudeRatio;
    float* data = buffer.getWritePointer(0);
    for (int index = 0; index < settings.length; ++index)
    {
        const double time = index / settings.sampleRate;
        data[index] = static_cast<float>(first * std::sin(kTwoPi * settings.firstHz * time) + second * std::sin(kTwoPi * settings.secondHz * time));
    }
    applyRelation(buffer, relation);
    return buffer;
}

std::vector<double> getMultitoneFrequencies(const MultitoneSettings& settings)
{
    std::vector<double> frequencies;
    const double ratio = std::pow(settings.highestHz / settings.lowestHz, 1.0 / std::max(1, settings.numberOfTones - 1));
    double frequency = settings.lowestHz;
    for (int tone = 0; tone < settings.numberOfTones; ++tone)
    {
        const double snapped = snapToBin(frequency, settings.sampleRate, settings.fftSize);
        if (frequencies.empty() || snapped > frequencies.back())
        {
            frequencies.push_back(snapped); // two tones on the same bin at the low end: only one
        }
        frequency *= ratio;
    }
    return frequencies;
}

// Schroeder phases for tones of any spacing: the group delay of tone k is k / N of the period, so the tones peak one after the other
// (phase_k = phase_k-1 - 2 pi tau_k (f_k - f_k-1)); for linearly spaced tones this is Schroeder's -pi k (k - 1) / N.
// Without Schroeder phases all tones are in phase at the start (cosines): the highest crest factor, for comparison.
static std::vector<double> getMultitonePhases(const std::vector<double>& frequencies, const MultitoneSettings& settings)
{
    std::vector<double> phases(frequencies.size(), 0.0);
    if (!settings.schroederPhases)
    {
        return phases;
    }
    const double period = settings.fftSize / settings.sampleRate;
    const double count = static_cast<double>(frequencies.size());
    for (size_t tone = 1; tone < frequencies.size(); ++tone)
    {
        const double groupDelay = period * static_cast<double>(tone) / count;
        phases[tone] = phases[tone - 1] - kTwoPi * groupDelay * (frequencies[tone] - frequencies[tone - 1]);
    }
    return phases;
}

juce::AudioBuffer<float> makeMultitone(const MultitoneSettings& settings, int channels, ChannelRelation relation)
{
    juce::AudioBuffer<float> buffer(channels, settings.length);
    const std::vector<double> frequencies = getMultitoneFrequencies(settings);
    const int count = static_cast<int>(frequencies.size());
    int generatedChannels = 1;
    if (relation == ChannelRelation::Uncorrelated)
    {
        generatedChannels = channels;
    }
    const std::vector<double> phases = getMultitonePhases(frequencies, settings);
    for (int channel = 0; channel < generatedChannels; ++channel)
    {
        float* data = buffer.getWritePointer(channel);
        for (int index = 0; index < settings.length; ++index)
        {
            double sum = 0.0;
            for (int tone = 0; tone < count; ++tone)
            {
                // uncorrelated channels: a different phase offset per channel and tone
                const double phase = phases[static_cast<size_t>(tone)] + channel * kPi / 2.0 * tone;
                sum += std::cos(kTwoPi * frequencies[static_cast<size_t>(tone)] * index / settings.sampleRate + phase);
            }
            data[index] = static_cast<float>(sum);
        }
        scaleToRms(data, settings.length, dbToGain(settings.levelDbfsRms));
    }
    if (relation != ChannelRelation::Uncorrelated)
    {
        applyRelation(buffer, relation);
    }
    return buffer;
}

juce::AudioBuffer<float> makeNoise(const NoiseSettings& settings, int channels, ChannelRelation relation)
{
    juce::AudioBuffer<float> buffer(channels, settings.length);
    int generatedChannels = 1;
    if (relation == ChannelRelation::Uncorrelated)
    {
        generatedChannels = channels;
    }
    for (int channel = 0; channel < generatedChannels; ++channel)
    {
        float* data = buffer.getWritePointer(channel);
        fillNoise(data, settings.length, settings.colour, settings.seed + kSecondChannelSeedOffset * channel);
        scaleToRms(data, settings.length, dbToGain(settings.levelDbfsRms));
    }
    if (relation != ChannelRelation::Uncorrelated)
    {
        applyRelation(buffer, relation);
    }
    return buffer;
}

juce::AudioBuffer<float> makeBursts(const BurstSettings& settings, int channels)
{
    const int burst = static_cast<int>(settings.burstSeconds * settings.sampleRate);
    const int gap = static_cast<int>(settings.gapSeconds * settings.sampleRate);
    const int count = static_cast<int>(settings.levelsDbfsPeak.size());
    juce::AudioBuffer<float> buffer = makeSilence(count * (burst + gap), channels);
    for (int index = 0; index < count; ++index)
    {
        const double amplitude = dbToGain(settings.levelsDbfsPeak[static_cast<size_t>(index)]);
        const int start = index * (burst + gap);
        for (int sample = 0; sample < burst; ++sample)
        {
            const float value = static_cast<float>(amplitude * std::sin(kTwoPi * settings.frequencyHz * sample / settings.sampleRate));
            for (int channel = 0; channel < channels; ++channel)
            {
                buffer.setSample(channel, start + sample, value);
            }
        }
    }
    return buffer;
}

bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    constexpr int kBitsPerSample = 32;
    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
    {
        return false;
    }
    juce::WavAudioFormat wav;
    juce::AudioFormatWriterOptions options = juce::AudioFormatWriterOptions()
                                                 .withSampleRate(sampleRate)
                                                 .withNumChannels(buffer.getNumChannels())
                                                 .withBitsPerSample(kBitsPerSample)
                                                 .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    std::unique_ptr<juce::OutputStream> output = std::move(stream);
    std::unique_ptr<juce::AudioFormatWriter> writer = wav.createWriterFor(output, options);
    if (writer == nullptr)
    {
        return false;
    }
    return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}
}
