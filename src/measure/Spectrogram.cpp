#include "pluginlab/measure/Spectrogram.h"

#include <algorithm>
#include <cmath>

#include <juce_dsp/juce_dsp.h>

namespace pluginlab::measure
{
namespace
{
constexpr double kFloor = 1.0e-12;          // -240 dB: the level of an empty bin
constexpr double kFadeSeconds = 0.005;      // the sweep's fade in and out
}

const char* getSpectrogramWindowName(SpectrogramWindow window)
{
    if (window == SpectrogramWindow::Hann)
    {
        return "Hann";
    }
    return "Blackman-Harris (4 terms)";
}

double Spectrogram::getSeconds() const
{
    return static_cast<double>(frames.size()) * hopSize / sampleRate;
}

double Spectrogram::getBinHz() const
{
    return sampleRate / fftSize;
}

namespace
{
// the level of every bin (dB re a full-scale sine) of the frames of one transform; a frame k covers the samples from k hop + offset on
std::vector<std::vector<float>> transform(const std::vector<float>& signal, int order, int hop, int offset, SpectrogramWindow type, size_t frames)
{
    const int size = 1 << order;
    std::vector<float> window(static_cast<size_t>(size));
    double windowSum = 0.0;
    for (int n = 0; n < size; ++n)
    {
        const double phase = 2.0 * juce::MathConstants<double>::pi * n / size; // periodic windows
        if (type == SpectrogramWindow::Hann)
        {
            window[static_cast<size_t>(n)] = static_cast<float>(0.5 - 0.5 * std::cos(phase));
        }
        else
        {
            window[static_cast<size_t>(n)] = static_cast<float>(0.35875 - 0.48829 * std::cos(phase) + 0.14128 * std::cos(2.0 * phase) - 0.01168 * std::cos(3.0 * phase));
        }
        windowSum += window[static_cast<size_t>(n)];
    }
    juce::dsp::FFT fft(order);
    std::vector<float> buffer(static_cast<size_t>(2 * size));
    std::vector<std::vector<float>> result;
    for (size_t frameIndex = 0; frameIndex < frames; ++frameIndex)
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        const long long start = static_cast<long long>(frameIndex) * hop + offset;
        for (int n = 0; n < size; ++n)
        {
            const long long index = start + n;
            if (index >= 0 && index < static_cast<long long>(signal.size()))
            {
                buffer[static_cast<size_t>(n)] = signal[static_cast<size_t>(index)] * window[static_cast<size_t>(n)];
            }
        }
        fft.performFrequencyOnlyForwardTransform(buffer.data(), true);
        std::vector<float> frame(static_cast<size_t>(size / 2 + 1));
        for (int bin = 0; bin <= size / 2; ++bin)
        {
            // a sine of amplitude 1 gives |X| = sum(w) / 2 in its bin: 2 |X| / sum(w) = 1 (the DC and Nyquist bins have no mirror)
            double scale = 2.0;
            if (bin == 0 || bin == size / 2)
            {
                scale = 1.0;
            }
            const double amplitude = scale * buffer[static_cast<size_t>(bin)] / windowSum;
            frame[static_cast<size_t>(bin)] = static_cast<float>(20.0 * std::log10(std::max(amplitude, kFloor)));
        }
        result.push_back(std::move(frame));
    }
    return result;
}

int getOrder(int size)
{
    return juce::jlimit(6, 16, juce::roundToInt(std::log2(static_cast<double>(std::max(size, 2)))));
}
}

Spectrogram computeSpectrogram(const std::vector<float>& signal, double sampleRate, const SpectrogramSettings& settings)
{
    Spectrogram result;
    result.sampleRate = sampleRate;
    const int shortOrder = getOrder(settings.fftSize);
    const int shortSize = 1 << shortOrder;
    result.shortFftSize = shortSize;
    result.fftSize = shortSize;
    result.hopSize = std::max(1, juce::roundToInt(shortSize * (1.0 - juce::jlimit(0.0, 0.95, settings.overlap))));
    const size_t frames = (signal.size() + static_cast<size_t>(result.hopSize) - 1) / static_cast<size_t>(result.hopSize);
    const bool two = settings.lowFrequencyFftSize > shortSize;
    if (! two)
    {
        result.frames = transform(signal, shortOrder, result.hopSize, 0, settings.window, frames);
        return result;
    }
    const int longOrder = getOrder(settings.lowFrequencyFftSize);
    const int longSize = 1 << longOrder;
    result.fftSize = longSize;
    // centred on the same instants: the long frame starts (long - short) / 2 samples earlier
    const std::vector<std::vector<float>> longFrames = transform(signal, longOrder, result.hopSize, -(longSize - shortSize) / 2, settings.window, frames);
    const std::vector<std::vector<float>> shortFrames = transform(signal, shortOrder, result.hopSize, 0, settings.window, frames);
    const double ratio = static_cast<double>(longSize) / shortSize;
    for (size_t index = 0; index < frames; ++index)
    {
        std::vector<float> frame = longFrames[index];
        for (size_t bin = 0; bin < frame.size(); ++bin)
        {
            // the short transform's level at this frequency (interpolated between its bins); the smaller of the two
            const double position = static_cast<double>(bin) / ratio;
            const size_t lower = std::min(static_cast<size_t>(position), shortFrames[index].size() - 2);
            const float fraction = static_cast<float>(position - static_cast<double>(lower));
            frame[bin] = std::min(frame[bin], shortFrames[index][lower] + fraction * (shortFrames[index][lower + 1] - shortFrames[index][lower]));
        }
        result.frames.push_back(std::move(frame));
    }
    return result;
}

std::vector<float> makeSynchronizedSweep(double sampleRate, double startHz, double stopHz, double seconds, double levelDbfs)
{
    // L = round(f1 T / ln(f2 / f1)) / f1: the instantaneous phase is a whole number of cycles where the frequency doubles
    const double rate = std::max(1.0, std::round(startHz * seconds / std::log(stopHz / startHz))) / startHz;
    const double length = rate * std::log(stopHz / startHz);
    const int samples = static_cast<int>(std::ceil(length * sampleRate));
    const double amplitude = std::pow(10.0, levelDbfs / 20.0);
    const int fade = std::max(1, static_cast<int>(kFadeSeconds * sampleRate));
    std::vector<float> sweep(static_cast<size_t>(samples));
    for (int n = 0; n < samples; ++n)
    {
        const double t = n / sampleRate;
        double gain = amplitude;
        if (n < fade)
        {
            gain *= 0.5 - 0.5 * std::cos(juce::MathConstants<double>::pi * n / fade);
        }
        else if (n >= samples - fade)
        {
            gain *= 0.5 - 0.5 * std::cos(juce::MathConstants<double>::pi * (samples - 1 - n) / fade);
        }
        sweep[static_cast<size_t>(n)] = static_cast<float>(gain * std::sin(2.0 * juce::MathConstants<double>::pi * startHz * rate * (std::exp(t / rate) - 1.0)));
    }
    return sweep;
}
}
