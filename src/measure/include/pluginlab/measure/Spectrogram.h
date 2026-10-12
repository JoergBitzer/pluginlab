#pragma once

#include <vector>

#include <juce_core/juce_core.h>

namespace pluginlab::measure
{
// A spectrogram (short-time Fourier transform) for the reports (docs/measurements/spectrogram.md): a window, overlapping frames, the level of
// each bin in dB re a full-scale sine (a sine of amplitude 1 that falls on a bin reads 0 dB), as JadeSpectrogram2 computes it.
enum class SpectrogramWindow
{
    Hann,             // first sidelobe -31 dB: enough for a range of about 80 dB
    BlackmanHarris    // 4 terms, sidelobes below -92 dB: for a range of 120 dB (harmonics and noise far below the signal)
};

const char* getSpectrogramWindowName(SpectrogramWindow window);

// Two resolutions (optional): a logarithmic frequency axis needs fine frequency steps at the low end and fine time steps at the high end, which
// one FFT size cannot give both. With lowFrequencyFftSize > fftSize, a long and a short transform with the same hop, centred on the same
// instants, are combined bin by bin to the smaller level: the long one smears in time, the short one in frequency, and each smear is absent in
// the other (for sparse signals such as a sweep and its harmonics; JadeSpectrogram's idea of two FFT sizes at the same time).
struct SpectrogramSettings
{
    int fftSize = 2048;
    double overlap = 0.75;          // of a frame of fftSize: 0.75 gives a hop of a quarter frame
    SpectrogramWindow window = SpectrogramWindow::BlackmanHarris;
    int lowFrequencyFftSize = 0;    // 0: one resolution
};

struct Spectrogram
{
    double sampleRate = 48000.0;
    int fftSize = 2048;             // the bins of the frames: the long transform's if there are two
    int shortFftSize = 2048;        // the short transform (= fftSize with one resolution)
    int hopSize = 512;
    std::vector<std::vector<float>> frames; // per frame (time), per bin 0 ... fftSize / 2: dB re full-scale sine
    double getSeconds() const;              // the time the frames span
    double getBinHz() const;
};

Spectrogram computeSpectrogram(const std::vector<float>& signal, double sampleRate, const SpectrogramSettings& settings = {});

// The synchronized exponential sweep of Novak, Lotton, Simon (JAES 63(10), 2015): x(t) = A sin(2 pi f1 L (e^(t / L) - 1)), L chosen so that the
// sweep's phase is a whole number of cycles at every octave (its harmonics then line up), faded in and out over a few milliseconds
std::vector<float> makeSynchronizedSweep(double sampleRate, double startHz, double stopHz, double seconds, double levelDbfs);
}
