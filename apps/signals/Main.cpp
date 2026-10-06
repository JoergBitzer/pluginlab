#include <iostream>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "pluginlab/signals/Signals.h"
#include "pluginlab/signals/SteppedSine.h"
#include "pluginlab/signals/SweptSine.h"

namespace
{
namespace sig = pluginlab::signals;

constexpr int kExitOk = 0;
constexpr int kExitWrongArguments = 2;
constexpr int kExitCannotWrite = 3;
constexpr int kChannels = 2;
constexpr double kDefaultSampleRate = 48000.0;
constexpr double kHighLevelDbfs = -6.0;        // the agreed standard levels (docs/design/W6-plan.md section 2)
constexpr double kLowLevelDbfs = -20.0;
constexpr double kNoiseLevelDbfsRms = -20.0;
constexpr double kOnsetSeconds = 0.1;          // impulse and step come after this time
constexpr double kFadeSeconds = 0.01;          // raised-cosine fades of the sines and two-tones
constexpr double kMinimumPostSilenceSeconds = 0.5;
constexpr double kBurstCycleSeconds = 1.0;     // 0.5 s burst, 0.5 s gap
constexpr double kQuietestBurstDbfs = -50.0;
constexpr double kLoudestBurstDbfs = -5.0;
constexpr double kBurstStepDb = 5.0;           // the levels step in whole multiples of this
constexpr int kSteppedSineMinimumPeriods = 4;

const char* const kUsage = "Usage: PluginLabSignals <folder> [--seconds 5,10] [--rate 48000]\n"
                           "Writes the standard test signals (stereo, 32-bit float WAV) with signals.txt describing every file.\n";

struct SignalFile
{
    juce::String name;          // file name without the length and the extension
    juce::String description;
    juce::AudioBuffer<float> buffer;
    juce::String steps;         // CSV of the steps of a stepped sine (empty for the others)
};

juce::String formatSeconds(double seconds)
{
    return juce::String(seconds, 0) + "s";
}

// Pads with silence or cuts to exactly the length
juce::AudioBuffer<float> fitLength(const juce::AudioBuffer<float>& buffer, int length)
{
    juce::AudioBuffer<float> fitted(buffer.getNumChannels(), length);
    fitted.clear();
    const int copied = std::min(length, buffer.getNumSamples());
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        fitted.copyFrom(channel, 0, buffer, channel, 0, copied);
    }
    return fitted;
}

juce::String describeLevel(double levelDbfs)
{
    return juce::String(static_cast<int>(levelDbfs)) + "dBFS";
}

void addSines(std::vector<SignalFile>& files, double sampleRate, int length)
{
    struct SineCase
    {
        double frequencyHz;
        double levelDbfs;
        const char* name;
    };
    const SineCase cases[] = {{1000.0, kHighLevelDbfs, "1kHz"}, {1000.0, kLowLevelDbfs, "1kHz"}, {100.0, kHighLevelDbfs, "100Hz"},
                              {10000.0, kHighLevelDbfs, "10kHz"}};
    for (const SineCase& sineCase : cases)
    {
        sig::SineSettings settings;
        settings.sampleRate = sampleRate;
        settings.frequencyHz = sineCase.frequencyHz;
        settings.levelDbfsPeak = sineCase.levelDbfs;
        settings.length = length;
        settings.fadeSamples = static_cast<int>(kFadeSeconds * sampleRate);
        files.push_back({juce::String("sine_") + sineCase.name + "_" + describeLevel(sineCase.levelDbfs),
                         "sine " + juce::String(sineCase.frequencyHz) + " Hz, " + juce::String(sineCase.levelDbfs) + " dBFS peak, L = R, 10 ms raised-cosine fades",
                         sig::makeSine(settings, kChannels), {}});
    }
}

void addTwoTones(std::vector<SignalFile>& files, double sampleRate, int length)
{
    files.push_back({"twotone_SMPTE_60Hz_7kHz_" + describeLevel(kHighLevelDbfs),
                     "two-tone SMPTE (IMD): 60 Hz and 7 kHz, amplitudes 4:1, -6 dBFS peak of the sum, L = R",
                     sig::makeTwoTone(sig::makeSmpteSettings(sampleRate, length), kChannels), {}});
    files.push_back({"twotone_CCIF_19kHz_20kHz_" + describeLevel(kHighLevelDbfs),
                     "two-tone CCIF/ITU (IMD): 19 kHz and 20 kHz, amplitudes 1:1, -6 dBFS peak of the sum, L = R",
                     sig::makeTwoTone(sig::makeCcifSettings(sampleRate, length), kChannels), {}});
}

void addMultitone(std::vector<SignalFile>& files, double sampleRate, int length)
{
    sig::MultitoneSettings settings;
    settings.sampleRate = sampleRate;
    settings.length = length;
    files.push_back({"multitone_31tones_30Hz_20kHz_-20dBFSrms",
                     "multitone: 31 log-spaced tones 30 Hz ... 20 kHz on the bins of a 65536-point FFT (periodic in 65536 samples), Schroeder phases, "
                     "-20 dBFS RMS, L = R",
                     sig::makeMultitone(settings, kChannels), {}});
}

void addNoises(std::vector<SignalFile>& files, int length)
{
    struct NoiseCase
    {
        sig::NoiseColour colour;
        sig::ChannelRelation relation;
        const char* name;
        const char* description;
    };
    const NoiseCase cases[] = {
        {sig::NoiseColour::WhiteGaussian, sig::ChannelRelation::Same, "noise_white_gaussian_LeqR", "white noise, Gaussian, L = R"},
        {sig::NoiseColour::WhiteGaussian, sig::ChannelRelation::Uncorrelated, "noise_white_gaussian_uncorrelated", "white noise, Gaussian, L and R uncorrelated"},
        {sig::NoiseColour::WhiteGaussian, sig::ChannelRelation::LeftOnly, "noise_white_gaussian_Lonly", "white noise, Gaussian, left only (crosstalk L -> R)"},
        {sig::NoiseColour::WhiteGaussian, sig::ChannelRelation::RightOnly, "noise_white_gaussian_Ronly", "white noise, Gaussian, right only (crosstalk R -> L)"},
        {sig::NoiseColour::WhiteGaussian, sig::ChannelRelation::Inverted, "noise_white_gaussian_LeqminusR", "white noise, Gaussian, L = -R (side only)"},
        {sig::NoiseColour::WhiteUniform, sig::ChannelRelation::Same, "noise_white_uniform_LeqR", "white noise, uniform, L = R"},
        {sig::NoiseColour::Pink, sig::ChannelRelation::Same, "noise_pink_LeqR", "pink noise (Voss-McCartney, 16 rows), L = R"},
        {sig::NoiseColour::Pink, sig::ChannelRelation::Uncorrelated, "noise_pink_uncorrelated", "pink noise (Voss-McCartney, 16 rows), L and R uncorrelated"}};
    for (const NoiseCase& noiseCase : cases)
    {
        sig::NoiseSettings settings;
        settings.colour = noiseCase.colour;
        settings.levelDbfsRms = kNoiseLevelDbfsRms;
        settings.length = length;
        files.push_back({juce::String(noiseCase.name) + "_-20dBFSrms", juce::String(noiseCase.description) + ", -20 dBFS RMS on every channel that carries it, seed 7",
                         sig::makeNoise(settings, kChannels, noiseCase.relation), {}});
    }
}

// The synchronized swept sine: the sweep as long as the rounding of k allows, the rest of the file is silence after it (at least 0.5 s)
void addSweeps(std::vector<SignalFile>& files, double sampleRate, double seconds, int length)
{
    for (const double level : {kHighLevelDbfs, kLowLevelDbfs})
    {
        sig::SweptSineSettings settings;
        settings.sampleRate = sampleRate;
        settings.levelDbfsPeak = level;
        settings.preSilenceSeconds = kOnsetSeconds;
        settings.postSilenceSeconds = 0.0;
        settings.approximateSeconds = seconds - kOnsetSeconds - kMinimumPostSilenceSeconds;
        sig::SweptSine sweep = sig::makeSweptSine(settings, kChannels);
        // one k less if the rounding made the sweep too long for the file (one step of k is ln(f2/f1) / f1 seconds)
        const double secondsPerK = std::log(settings.stopHz / settings.startHz) / settings.startHz;
        while (sweep.signal.getNumSamples() > length - static_cast<int>(kMinimumPostSilenceSeconds * sampleRate))
        {
            settings.approximateSeconds -= secondsPerK;
            sweep = sig::makeSweptSine(settings, kChannels);
        }
        const juce::String description = "synchronized swept sine (Novak et al. 2015) 30 Hz ... 20 kHz, " + juce::String(level) + " dBFS peak, L = R; pre-silence "
                                         + juce::String(kOnsetSeconds) + " s (sweep starts at sample " + juce::String(sweep.startSample) + "), sweep "
                                         + juce::String(sweep.durationSeconds, 6) + " s (" + juce::String(sweep.sweepLength) + " samples), L = "
                                         + juce::String(sweep.rate, 9) + " s, k = L f1 = " + juce::String(sweep.rate * settings.startHz, 0)
                                         + "; silence after the sweep to the end of the file";
        files.push_back({"sweep_sync_30Hz_20kHz_" + describeLevel(level), description, fitLength(sweep.signal, length), {}});
    }
}

// The stepped sine 20 kHz -> 20 Hz: the most steps per octave (3, 2 or 1) and the longest measurement time that fit into the file
void addSteppedSines(std::vector<SignalFile>& files, double sampleRate, int length)
{
    for (const double level : {kHighLevelDbfs, kLowLevelDbfs})
    {
        sig::SteppedSineSettings settings;
        settings.sampleRate = sampleRate;
        settings.levelDbfsPeak = level;
        settings.minimumPeriods = kSteppedSineMinimumPeriods;
        sig::SteppedSine best;
        for (const int stepsPerOctave : {3, 2, 1})
        {
            settings.stepsPerOctave = stepsPerOctave;
            double low = 0.0;
            double high = 2.0;
            settings.measureSeconds = low;
            if (sig::makeSteppedSine(settings, 1).signal.getNumSamples() > length)
            {
                continue;
            }
            for (int iteration = 0; iteration < 30; ++iteration)
            {
                settings.measureSeconds = (low + high) / 2.0;
                if (sig::makeSteppedSine(settings, 1).signal.getNumSamples() > length)
                {
                    high = settings.measureSeconds;
                }
                else
                {
                    low = settings.measureSeconds;
                }
            }
            settings.measureSeconds = low;
            best = sig::makeSteppedSine(settings, kChannels);
            break;
        }
        if (best.steps.empty())
        {
            continue; // the file is too short even for one step per octave
        }
        juce::String steps = "frequency_Hz,start_sample,measure_start_sample,measure_length_samples,step_length_samples\n";
        for (const sig::SineStep& step : best.steps)
        {
            steps << juce::String(step.frequencyHz, 4) << "," << step.start << "," << step.measureStart << "," << step.measureLength << "," << step.length << "\n";
        }
        const juce::String description = "stepped sine (Audio Precision style) 20 kHz -> 20 Hz, " + juce::String(best.settings.stepsPerOctave)
                                         + " steps per octave (" + juce::String(static_cast<int>(best.steps.size())) + " steps), " + juce::String(level)
                                         + " dBFS peak, L = R; each step: Hann fade-in " + juce::String(best.settings.fadeSeconds * 1000.0) + " ms, settling "
                                         + juce::String(best.settings.settleSeconds) + " s, measurement >= "
                                         + juce::String(best.settings.measureSeconds, 3) + " s and >= 4 periods (whole periods), Hann fade-out "
                                         + juce::String(best.settings.fadeSeconds * 1000.0) + " ms; the steps in the CSV file";
        files.push_back({"steppedsine_20kHz_20Hz_" + describeLevel(level), description, fitLength(best.signal, length), steps});
    }
}

void addBursts(std::vector<SignalFile>& files, double sampleRate, double seconds, int length)
{
    sig::BurstSettings settings;
    settings.sampleRate = sampleRate;
    settings.levelsDbfsPeak.clear();
    const int count = static_cast<int>(seconds / kBurstCycleSeconds);
    juce::String levels;
    // the loudest burst at -5 dBFS, steps of 5 or 10 dB (as large as fits) down to no lower than -50 dBFS
    const double range = kLoudestBurstDbfs - kQuietestBurstDbfs;
    const double step = std::max(kBurstStepDb, std::floor(range / std::max(1, count - 1) / kBurstStepDb) * kBurstStepDb);
    for (int index = 0; index < count; ++index)
    {
        const double level = kLoudestBurstDbfs - step * (count - 1 - index);
        settings.levelsDbfsPeak.push_back(level);
        levels << juce::String(level) << " ";
    }
    files.push_back({"bursts_1kHz_rising",
                     "sine bursts 1 kHz, 0.5 s on / 0.5 s off, rising levels (dBFS peak): " + levels.trim() + "; L = R",
                     fitLength(sig::makeBursts(settings, kChannels), length), {}});
}

std::vector<SignalFile> makeSignals(double sampleRate, double seconds)
{
    const int length = static_cast<int>(std::round(seconds * sampleRate));
    const int onset = static_cast<int>(std::round(kOnsetSeconds * sampleRate));
    const double highGain = sig::dbToGain(kHighLevelDbfs);
    std::vector<SignalFile> files;
    files.push_back({"silence", "digital silence", sig::makeSilence(length, kChannels), {}});
    files.push_back({"impulse_" + describeLevel(kHighLevelDbfs), "unit impulse of amplitude 0.5 (-6 dBFS) at sample " + juce::String(onset) + " (0.1 s), L = R",
                     sig::makeImpulse(length, onset, highGain, kChannels), {}});
    files.push_back({"step_" + describeLevel(kHighLevelDbfs), "step to 0.5 (-6 dBFS, DC) at sample " + juce::String(onset) + " (0.1 s), L = R",
                     sig::makeStep(length, onset, highGain, kChannels), {}});
    addSines(files, sampleRate, length);
    addTwoTones(files, sampleRate, length);
    addMultitone(files, sampleRate, length);
    addNoises(files, length);
    addSweeps(files, sampleRate, seconds, length);
    addSteppedSines(files, sampleRate, length);
    addBursts(files, sampleRate, seconds, length);
    return files;
}

std::vector<double> parseSeconds(const juce::String& text)
{
    std::vector<double> seconds;
    for (const juce::String& part : juce::StringArray::fromTokens(text, ",", ""))
    {
        if (part.getDoubleValue() > 0.0)
        {
            seconds.push_back(part.getDoubleValue());
        }
    }
    return seconds;
}
}

// Usage: PluginLabSignals <folder> [--seconds 5,10] [--rate 48000]
int main(int argc, char* argv[])
{
    const juce::StringArray arguments(argv + 1, argc - 1);
    if (arguments.isEmpty() || arguments[0].startsWith("-"))
    {
        std::cerr << kUsage;
        return kExitWrongArguments;
    }
    const juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile(arguments[0]);
    std::vector<double> lengths = {5.0, 10.0};
    double sampleRate = kDefaultSampleRate;
    for (int index = 1; index + 1 < arguments.size(); ++index)
    {
        if (arguments[index] == "--seconds")
        {
            lengths = parseSeconds(arguments[index + 1]);
        }
        if (arguments[index] == "--rate")
        {
            sampleRate = arguments[index + 1].getDoubleValue();
        }
    }
    if (lengths.empty() || sampleRate <= 0.0 || !folder.createDirectory())
    {
        std::cerr << kUsage;
        return kExitWrongArguments;
    }

    juce::String overview;
    overview << "Test signals of pluginlab (PluginLabSignals " << JUCE_APPLICATION_VERSION_STRING << "), src/signals, docs/design/W6-plan.md.\n"
             << "All files: stereo, " << juce::String(sampleRate, 0) << " Hz, 32-bit float WAV, deterministic (the same files every time).\n\n";
    int written = 0;
    for (const double seconds : lengths)
    {
        for (const SignalFile& signal : makeSignals(sampleRate, seconds))
        {
            const juce::String fileName = signal.name + "_" + formatSeconds(seconds) + ".wav";
            if (!sig::writeWav(folder.getChildFile(fileName), signal.buffer, sampleRate))
            {
                std::cerr << "Cannot write " << folder.getChildFile(fileName).getFullPathName() << "\n";
                return kExitCannotWrite;
            }
            ++written;
            overview << fileName << "\n    " << signal.description << "\n";
            if (signal.steps.isNotEmpty())
            {
                const juce::String stepsName = signal.name + "_" + formatSeconds(seconds) + "_steps.csv";
                folder.getChildFile(stepsName).replaceWithText(signal.steps);
                overview << "    steps: " << stepsName << "\n";
            }
        }
    }
    folder.getChildFile("signals.txt").replaceWithText(overview);
    std::cout << written << " files written to " << folder.getFullPathName() << "\n";
    return kExitOk;
}
