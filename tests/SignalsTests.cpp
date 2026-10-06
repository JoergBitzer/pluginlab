#include <cmath>
#include <complex>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "pluginlab/signals/Signals.h"
#include "pluginlab/signals/SteppedSine.h"
#include "pluginlab/signals/SweptSine.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

// The amplitude of one frequency in a window (a single DFT bin; exact for a whole number of periods)
double amplitudeAt(const float* data, int start, int length, double frequency, double sampleRate)
{
    std::complex<double> sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        sum += static_cast<double>(data[start + index]) * std::polar(1.0, -2.0 * kPi * frequency * index / sampleRate);
    }
    return 2.0 * std::abs(sum) / length;
}

// The magnitude of the spectrum of a (short) impulse response at one frequency
double magnitudeAt(const std::vector<float>& response, int start, int length, double frequency, double sampleRate)
{
    std::complex<double> sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        const int position = (start + index + static_cast<int>(response.size())) % static_cast<int>(response.size());
        sum += static_cast<double>(response[static_cast<size_t>(position)]) * std::polar(1.0, -2.0 * kPi * frequency * index / sampleRate);
    }
    return std::abs(sum);
}

double correlation(const juce::AudioBuffer<float>& buffer)
{
    double sumXY = 0.0;
    double sumXX = 0.0;
    double sumYY = 0.0;
    for (int index = 0; index < buffer.getNumSamples(); ++index)
    {
        const double x = buffer.getSample(0, index);
        const double y = buffer.getSample(1, index);
        sumXY += x * y;
        sumXX += x * x;
        sumYY += y * y;
    }
    return sumXY / std::sqrt(std::max(1.0e-30, sumXX * sumYY));
}

// Octave band levels 63 Hz ... 8 kHz (dB) from an averaged power spectrum
std::vector<double> getOctaveBandLevels(const juce::AudioBuffer<float>& buffer)
{
    constexpr int kOrder = 13;
    constexpr int kSize = 1 << kOrder;
    juce::dsp::FFT fft(kOrder);
    juce::dsp::WindowingFunction<float> window(kSize, juce::dsp::WindowingFunction<float>::hann, false);
    std::vector<double> power(kSize / 2, 0.0);
    std::vector<float> frame(2 * kSize);
    for (int start = 0; start + kSize <= buffer.getNumSamples(); start += kSize / 2)
    {
        std::fill(frame.begin(), frame.end(), 0.0f);
        std::copy(buffer.getReadPointer(0) + start, buffer.getReadPointer(0) + start + kSize, frame.begin());
        window.multiplyWithWindowingTable(frame.data(), kSize);
        fft.performFrequencyOnlyForwardTransform(frame.data());
        for (int bin = 0; bin < kSize / 2; ++bin)
        {
            power[static_cast<size_t>(bin)] += static_cast<double>(frame[static_cast<size_t>(bin)]) * frame[static_cast<size_t>(bin)];
        }
    }
    std::vector<double> bands;
    for (double centre = 62.5; centre <= 8000.0; centre *= 2.0)
    {
        const int low = static_cast<int>(std::ceil(centre / std::sqrt(2.0) * kSize / kSampleRate));
        const int high = static_cast<int>(std::floor(centre * std::sqrt(2.0) * kSize / kSampleRate));
        double energy = 0.0;
        for (int bin = low; bin <= high; ++bin)
        {
            energy += power[static_cast<size_t>(bin)];
        }
        bands.push_back(10.0 * std::log10(energy));
    }
    return bands;
}

int findPeak(const std::vector<float>& data, int from, int to)
{
    int position = from;
    for (int index = from; index < to; ++index)
    {
        if (std::abs(data[static_cast<size_t>(index)]) > std::abs(data[static_cast<size_t>(position)]))
        {
            position = index;
        }
    }
    return position;
}
}

// The test signals of W6.1, each against its definition
class SignalsTests : public juce::UnitTest
{
public:
    SignalsTests()
        : juce::UnitTest("Signals", "pluginlab")
    {
    }

    void runTest() override
    {
        testLevelsAndSine();
        testTwoToneAndMultitone();
        testNoise();
        testSweptSine();
        testSteppedSine();
        testSteppedSineFades();
        testWav();
    }

private:
    void testLevelsAndSine()
    {
        beginTest("sine: peak level, frequency on an FFT bin, fades; impulse and step");
        pluginlab::signals::SineSettings settings;
        settings.frequencyHz = pluginlab::signals::snapToBin(997.0, kSampleRate, 4800);
        expectWithinAbsoluteError(settings.frequencyHz, 1000.0, 1.0e-9);
        settings.length = 4800;
        const juce::AudioBuffer<float> sine = pluginlab::signals::makeSine(settings, 2);
        expectWithinAbsoluteError(pluginlab::signals::gainToDb(pluginlab::signals::getPeak(sine, 0)), -6.0, 0.01);
        expectWithinAbsoluteError(pluginlab::signals::gainToDb(amplitudeAt(sine.getReadPointer(1), 0, 4800, 1000.0, kSampleRate)), -6.0, 0.001);
        settings.fadeSamples = 480;
        const juce::AudioBuffer<float> faded = pluginlab::signals::makeSine(settings, 1);
        expectEquals(faded.getSample(0, 0), 0.0f);
        const juce::AudioBuffer<float> impulse = pluginlab::signals::makeImpulse(100, 10, 0.5, 2);
        expectEquals(impulse.getSample(1, 10), 0.5f);
        expectEquals(impulse.getSample(1, 11), 0.0f);
        const juce::AudioBuffer<float> step = pluginlab::signals::makeStep(100, 10, 1.0, 1);
        expectEquals(step.getSample(0, 9), 0.0f);
        expectEquals(step.getSample(0, 99), 1.0f);
    }

    void testTwoToneAndMultitone()
    {
        beginTest("two-tone: SMPTE 60 Hz / 7 kHz at 4:1, the peak of the sum at the level");
        const juce::AudioBuffer<float> smpte = pluginlab::signals::makeTwoTone(pluginlab::signals::makeSmpteSettings(kSampleRate, 48000), 1);
        const double low = amplitudeAt(smpte.getReadPointer(0), 0, 48000, 60.0, kSampleRate);
        const double high = amplitudeAt(smpte.getReadPointer(0), 0, 48000, 7000.0, kSampleRate);
        expectWithinAbsoluteError(low / high, 4.0, 0.001);
        expect(pluginlab::signals::getPeak(smpte, 0) <= pluginlab::signals::dbToGain(-6.0) + 1.0e-6);

        beginTest("multitone: tones on bins, RMS at the level, Schroeder phases give a lower crest factor than zero phases");
        pluginlab::signals::MultitoneSettings settings;
        const juce::AudioBuffer<float> schroeder = pluginlab::signals::makeMultitone(settings, 1);
        settings.schroederPhases = false;
        const juce::AudioBuffer<float> zeroPhase = pluginlab::signals::makeMultitone(settings, 1);
        expectWithinAbsoluteError(pluginlab::signals::gainToDb(pluginlab::signals::getRms(schroeder, 0)), -20.0, 0.001);
        const double crestSchroeder = pluginlab::signals::gainToDb(pluginlab::signals::getPeak(schroeder, 0) / pluginlab::signals::getRms(schroeder, 0));
        const double crestZero = pluginlab::signals::gainToDb(pluginlab::signals::getPeak(zeroPhase, 0) / pluginlab::signals::getRms(zeroPhase, 0));
        logMessage("crest factor Schroeder " + juce::String(crestSchroeder, 2) + " dB, zero phase " + juce::String(crestZero, 2) + " dB");
        expect(crestSchroeder + 5.0 < crestZero);
        for (const double frequency : pluginlab::signals::getMultitoneFrequencies(settings))
        {
            const double bin = frequency * settings.fftSize / kSampleRate;
            expectWithinAbsoluteError(bin, std::round(bin), 1.0e-6);
        }
    }

    void testNoise()
    {
        beginTest("noise: RMS at the level, white and pink, channel relations, deterministic");
        pluginlab::signals::NoiseSettings settings;
        settings.length = 480000;
        const juce::AudioBuffer<float> gaussian = pluginlab::signals::makeNoise(settings, 2, pluginlab::signals::ChannelRelation::Uncorrelated);
        expectWithinAbsoluteError(pluginlab::signals::gainToDb(pluginlab::signals::getRms(gaussian, 0)), -20.0, 0.001);
        expectWithinAbsoluteError(pluginlab::signals::gainToDb(pluginlab::signals::getRms(gaussian, 1)), -20.0, 0.001);
        expect(std::abs(correlation(gaussian)) < 0.01, "uncorrelated: " + juce::String(correlation(gaussian)));
        const juce::AudioBuffer<float> inverted = pluginlab::signals::makeNoise(settings, 2, pluginlab::signals::ChannelRelation::Inverted);
        expectWithinAbsoluteError(correlation(inverted), -1.0, 1.0e-9);
        const juce::AudioBuffer<float> same = pluginlab::signals::makeNoise(settings, 2, pluginlab::signals::ChannelRelation::Same);
        expectWithinAbsoluteError(correlation(same), 1.0, 1.0e-9);
        const juce::AudioBuffer<float> left = pluginlab::signals::makeNoise(settings, 2, pluginlab::signals::ChannelRelation::LeftOnly);
        expectEquals(pluginlab::signals::getPeak(left, 1), 0.0);
        const juce::AudioBuffer<float> again = pluginlab::signals::makeNoise(settings, 2, pluginlab::signals::ChannelRelation::Uncorrelated);
        expectEquals(again.getSample(1, 12345), gaussian.getSample(1, 12345));

        // pink: the same energy in every octave band (1/f), white rises by 3 dB per octave; averaged power spectrum (Welch, Hann)
        settings.colour = pluginlab::signals::NoiseColour::Pink;
        const juce::AudioBuffer<float> pink = pluginlab::signals::makeNoise(settings, 1);
        settings.colour = pluginlab::signals::NoiseColour::WhiteGaussian;
        const juce::AudioBuffer<float> white = pluginlab::signals::makeNoise(settings, 1);
        const std::vector<double> pinkBands = getOctaveBandLevels(pink);
        const std::vector<double> whiteBands = getOctaveBandLevels(white);
        const double spread = *std::max_element(pinkBands.begin(), pinkBands.end()) - *std::min_element(pinkBands.begin(), pinkBands.end());
        const double whiteSlope = (whiteBands.back() - whiteBands.front()) / static_cast<double>(whiteBands.size() - 1);
        logMessage("pink noise: spread of the octave band levels 63 Hz ... 8 kHz " + juce::String(spread, 2) + " dB; white noise: "
                   + juce::String(whiteSlope, 2) + " dB per octave");
        expect(spread < 1.0);
        expectWithinAbsoluteError(whiteSlope, 3.01, 0.1);
    }

    void testSweptSine()
    {
        beginTest("synchronized swept sine: k integer, zero phase at the start, duration from k");
        pluginlab::signals::SweptSineSettings settings;
        settings.approximateSeconds = 2.0;
        const pluginlab::signals::SweptSine sweep = pluginlab::signals::makeSweptSine(settings, 1);
        const double k = sweep.rate * settings.startHz;
        expectWithinAbsoluteError(k, std::round(k), 1.0e-9);
        expectWithinAbsoluteError(sweep.durationSeconds, sweep.rate * std::log(settings.stopHz / settings.startHz), 1.0e-12);
        expectWithinAbsoluteError(static_cast<double>(sweep.signal.getSample(0, sweep.startSample)), 0.0, 1.0e-6);
        expectWithinAbsoluteError(pluginlab::signals::gainToDb(pluginlab::signals::getPeak(sweep.signal, 0)), -6.0, 0.01);

        beginTest("synchronized swept sine: deconvolution of the identity gives a unit impulse, flat between 2 f1 and f2 / 2; a delay moves it");
        std::vector<float> identity(sweep.signal.getReadPointer(0), sweep.signal.getReadPointer(0) + sweep.signal.getNumSamples());
        const std::vector<float> impulse = pluginlab::signals::deconvolveSweptSine(sweep, identity);
        const int peak = findPeak(impulse, 0, 2000);
        expectEquals(peak, 0);
        // band-limited (f1 ... f2): the peak of the impulse is the share of the band in 0 ... fs / 2
        const double bandShare = 2.0 * (settings.stopHz - settings.startHz) / kSampleRate;
        expectWithinAbsoluteError(static_cast<double>(impulse[0]), bandShare, 0.01);
        for (const double frequency : {100.0, 1000.0, 5000.0, 10000.0})
        {
            const double level = pluginlab::signals::gainToDb(magnitudeAt(impulse, -256, 1024, frequency, kSampleRate));
            expect(std::abs(level) < 0.1, "flat at " + juce::String(frequency) + " Hz: " + juce::String(level, 3) + " dB");
        }
        constexpr int kDelay = 37;
        std::vector<float> delayed(identity.size(), 0.0f);
        for (size_t index = kDelay; index < delayed.size(); ++index)
        {
            delayed[index] = identity[index - kDelay];
        }
        expectEquals(findPeak(pluginlab::signals::deconvolveSweptSine(sweep, delayed), 0, 2000), kDelay);

        beginTest("synchronized swept sine: the harmonic impulse responses of y = x + a2 x^2 + a3 x^3 at -L ln(n) with the closed-form levels");
        constexpr double kA2 = 0.1;
        constexpr double kA3 = 0.05;
        std::vector<float> distorted(identity.size());
        for (size_t index = 0; index < identity.size(); ++index)
        {
            const double x = identity[index];
            distorted[index] = static_cast<float>(x + kA2 * x * x + kA3 * x * x * x);
        }
        const std::vector<float> response = pluginlab::signals::deconvolveSweptSine(sweep, distorted);
        const int size = static_cast<int>(response.size());
        const double amplitude = pluginlab::signals::dbToGain(settings.levelDbfsPeak);
        for (const int harmonic : {2, 3})
        {
            const double advance = pluginlab::signals::getHarmonicAdvanceSamples(sweep, harmonic);
            const int expected = size - static_cast<int>(std::round(advance));
            const int found = findPeak(response, expected - 200, expected + 200);
            expect(std::abs(found - expected) <= 2, "harmonic " + juce::String(harmonic) + " at " + juce::String(found) + ", expected " + juce::String(expected));
            // levels relative to the fundamental: 2nd a2 A / 2, 3rd a3 A^2 / 4 (x^2 = A^2/2 (1 - cos 2 phi), x^3 = A^3 (3/4 sin phi - 1/4 sin 3 phi))
            const double fundamental = magnitudeAt(response, -128, 512, 1000.0, kSampleRate);
            const double harmonicLevel = magnitudeAt(response, expected - 128, 512, 1000.0, kSampleRate);
            double expectedRatio = kA2 * amplitude / 2.0;
            if (harmonic == 3)
            {
                expectedRatio = kA3 * amplitude * amplitude / 4.0;
            }
            const double measured = pluginlab::signals::gainToDb(harmonicLevel / fundamental);
            const double wanted = pluginlab::signals::gainToDb(expectedRatio);
            logMessage("harmonic " + juce::String(harmonic) + ": " + juce::String(measured, 2) + " dB, closed form " + juce::String(wanted, 2) + " dB");
            expect(std::abs(measured - wanted) < 0.5, "harmonic level");
        }
    }

    void testSteppedSine()
    {
        beginTest("stepped sine: log-spaced from 20 kHz down to 20 Hz, windows after fade-in, latency and settling, whole periods, the level in every window");
        pluginlab::signals::SteppedSineSettings settings;
        settings.latencySamples = 64;
        const pluginlab::signals::SteppedSine stepped = pluginlab::signals::makeSteppedSine(settings, 1);
        expectEquals(static_cast<int>(stepped.steps.size()), 31); // 10 octaves at 3 steps per octave, both ends included
        expectWithinAbsoluteError(stepped.steps.front().frequencyHz, 20000.0, 1.0e-9);
        expectWithinAbsoluteError(stepped.steps.back().frequencyHz, 20.0, 1.0e-9);
        for (const pluginlab::signals::SineStep& step : stepped.steps)
        {
            const int fadeLength = static_cast<int>(settings.fadeSeconds * kSampleRate);
            expectEquals(step.measureStart - step.start, fadeLength + settings.latencySamples + static_cast<int>(settings.settleSeconds * kSampleRate));
            expectEquals(step.length, step.measureStart + step.measureLength + fadeLength - step.start);
            const double periods = step.measureLength * step.frequencyHz / kSampleRate;
            expect(std::abs(periods - std::round(periods)) < 0.02 * step.frequencyHz / 1000.0 + 0.02, "whole periods at " + juce::String(step.frequencyHz));
            const double level = amplitudeAt(stepped.signal.getReadPointer(0), step.measureStart, step.measureLength, step.frequencyHz, kSampleRate);
            expect(std::abs(pluginlab::signals::gainToDb(level) + 6.0) < 0.05, "level at " + juce::String(step.frequencyHz) + ": "
                                                                                     + juce::String(pluginlab::signals::gainToDb(level), 3));
        }
    }

    void testSteppedSineFades()
    {
        beginTest("stepped sine: Hann fades at every step: starts and ends at zero, no jump between steps, nothing outside the band of the step");
        pluginlab::signals::SteppedSineSettings settings;
        const pluginlab::signals::SteppedSine stepped = pluginlab::signals::makeSteppedSine(settings, 1);
        const float* data = stepped.signal.getReadPointer(0);
        const double amplitude = pluginlab::signals::dbToGain(settings.levelDbfsPeak);
        for (const pluginlab::signals::SineStep& step : stepped.steps)
        {
            expectEquals(data[step.start], 0.0f);
            // the last sample is the first sample of the mirrored fade: zero up to the rounding of the sine
            expect(std::abs(data[step.start + step.length - 1]) < 1.0e-3 * amplitude, "the end of the step at " + juce::String(step.frequencyHz));
        }
        // a hard switch at 20 kHz jumps by about the amplitude; with the fades the largest sample-to-sample step of the highest step stays that of the sine
        double largestJump = 0.0;
        for (int index = stepped.steps[1].start - 20; index < stepped.steps[1].start + 20; ++index)
        {
            largestJump = std::max(largestJump, static_cast<double>(std::abs(data[index + 1] - data[index])));
        }
        const double sineSlope = amplitude * 2.0 * std::sin(kPi * stepped.steps[1].frequencyHz / kSampleRate);
        expect(largestJump < 0.2 * sineSlope, "around the switch: " + juce::String(largestJump) + " against the sine's " + juce::String(sineSlope));

        // the spectrum of one whole low step (fades included) far from the tone: 97 Hz (the step is not a whole number of periods long, as most
        // steps), looked at 1234.5 Hz (no harmonic): at least 80 dB below the tone and far below a step with hard switches
        constexpr double kFarHz = 1234.5;
        settings.stepsPerOctave = 1;
        settings.startHz = 97.0;
        settings.stopHz = 48.5;
        const pluginlab::signals::SteppedSine low = pluginlab::signals::makeSteppedSine(settings, 1);
        const pluginlab::signals::SineStep& first = low.steps.front();
        const double tone = amplitudeAt(low.signal.getReadPointer(0), first.start, first.length, first.frequencyHz, kSampleRate);
        const double far = amplitudeAt(low.signal.getReadPointer(0), first.start, first.length, kFarHz, kSampleRate);
        settings.fadeSeconds = 0.0;
        const pluginlab::signals::SteppedSine hard = pluginlab::signals::makeSteppedSine(settings, 1);
        const pluginlab::signals::SineStep& hardFirst = hard.steps.front();
        const double hardTone = amplitudeAt(hard.signal.getReadPointer(0), hardFirst.start, hardFirst.length, hardFirst.frequencyHz, kSampleRate);
        const double hardFar = amplitudeAt(hard.signal.getReadPointer(0), hardFirst.start, hardFirst.length, kFarHz, kSampleRate);
        logMessage("stepped sine, one 97 Hz step: at 1234.5 Hz " + juce::String(pluginlab::signals::gainToDb(far / tone), 1) + " dB re the tone with 10 ms fades, "
                   + juce::String(pluginlab::signals::gainToDb(hardFar / hardTone), 1) + " dB with hard switches");
        expect(pluginlab::signals::gainToDb(far / tone) < -80.0);
        expect(pluginlab::signals::gainToDb(far / tone) < pluginlab::signals::gainToDb(hardFar / hardTone) - 40.0);
    }

    void testWav()
    {
        beginTest("a signal written as 32-bit float WAV reads back sample exact");
        pluginlab::signals::NoiseSettings settings;
        settings.length = 1000;
        const juce::AudioBuffer<float> noise = pluginlab::signals::makeNoise(settings, 2, pluginlab::signals::ChannelRelation::Uncorrelated);
        const juce::TemporaryFile file(".wav");
        expect(pluginlab::signals::writeWav(file.getFile(), noise, kSampleRate));
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file.getFile()));
        expect(reader != nullptr);
        if (reader == nullptr)
        {
            return;
        }
        juce::AudioBuffer<float> back(2, 1000);
        reader->read(&back, 0, 1000, 0, true, true);
        expectEquals(back.getSample(1, 777), noise.getSample(1, 777));
        expectEquals(reader->sampleRate, kSampleRate);
    }
};

static SignalsTests signalsTests;
