#include "pluginlab/measure/FrequencyResponse.h"

#include <cmath>

#include "pluginlab/signals/Signals.h"
#include "pluginlab/signals/SteppedSine.h"
#include "pluginlab/signals/SweptSine.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesPerRadian = 180.0 / kPi;
constexpr int kMinimumPeriods = 10;
constexpr double kMultitonePeriodSeconds = 1.3;     // the multitone period: the power of two at or above this
constexpr double kSweepStopFraction = 0.95;         // of Nyquist
constexpr double kSweepHighestStopHz = 40000.0;
constexpr double kSweepPreSilenceSeconds = 0.1;
constexpr double kSweepPostSilenceSeconds = 1.0;
constexpr double kGridLowestHz = 20.0;
constexpr int kGridPointsPerOctave = 24;
constexpr int kRenormaliseEvery = 1024;             // the rotating phasor of the DTFT is renormalised this often
// the standard third-octave frequencies of AES17 table 3 (IEC 61260-1 nominal values), 20 Hz ... 20 kHz; 1 kHz is replaced by 997 Hz
constexpr double kThirdOctaves[] = {20.0,   25.0,   31.5,   40.0,   50.0,   63.0,   80.0,   100.0,   125.0,   160.0,   200.0,
                                    250.0,  315.0,  400.0,  500.0,  630.0,  800.0,  997.0,   1250.0,  1600.0,  2000.0,  2500.0,
                                    3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0, 20000.0};

std::vector<double> toVector(const juce::AudioBuffer<float>& buffer, int channel)
{
    return std::vector<double>(buffer.getReadPointer(channel), buffer.getReadPointer(channel) + buffer.getNumSamples());
}

size_t findNearest(const std::vector<double>& frequencies, double wanted)
{
    size_t nearest = 0;
    for (size_t index = 1; index < frequencies.size(); ++index)
    {
        if (std::abs(frequencies[index] - wanted) < std::abs(frequencies[nearest] - wanted))
        {
            nearest = index;
        }
    }
    return nearest;
}

// magnitude, phase and the level relative to the reference, from the complex response
void finish(ChannelResponse& channel, std::complex<double> reference)
{
    channel.referenceGainDb = 20.0 * std::log10(std::max(std::abs(reference), 1.0e-30));
    for (const std::complex<double>& value : channel.response)
    {
        const double magnitude = 20.0 * std::log10(std::max(std::abs(value), 1.0e-30));
        channel.magnitudeDb.push_back(magnitude);
        channel.relativeDb.push_back(magnitude - channel.referenceGainDb);
        channel.phaseDegrees.push_back(std::arg(value) * kDegreesPerRadian);
    }
}

// The DTFT of a segment whose first sample is at time `firstTime` (samples): sum x[n] e^(-j 2 pi f (firstTime + n) / fs)
std::complex<double> getDtft(const std::vector<double>& segment, int firstTime, double frequencyHz, double sampleRate)
{
    const double omega = 2.0 * kPi * frequencyHz / sampleRate;
    const std::complex<double> step = std::polar(1.0, -omega);
    std::complex<double> phasor = std::polar(1.0, -omega * firstTime);
    std::complex<double> sum = 0.0;
    for (size_t index = 0; index < segment.size(); ++index)
    {
        sum += segment[index] * phasor;
        phasor *= step;
        if (index % kRenormaliseEvery == kRenormaliseEvery - 1)
        {
            phasor = std::polar(1.0, -omega * (firstTime + static_cast<double>(index) + 1.0));
        }
    }
    return sum;
}

int getPowerOfTwoAtLeast(double samples)
{
    int size = 1;
    while (size < samples)
    {
        size *= 2;
    }
    return size;
}

// A raised-cosine taper: 0 at the first sample, 1 after `length` samples
double getTaper(int sample, int length)
{
    if (length <= 0 || sample >= length)
    {
        return 1.0;
    }
    return 0.5 - 0.5 * std::cos(kPi * sample / length);
}
}

const char* getResponseMethodName(ResponseMethod method)
{
    switch (method)
    {
        case ResponseMethod::SteppedSine:
            return "stepped sine";
        case ResponseMethod::Multitone:
            return "multitone";
        case ResponseMethod::SweptSine:
            return "synchronized swept sine";
    }
    return "";
}

std::vector<double> getStandardThirdOctaveFrequencies()
{
    return std::vector<double>(std::begin(kThirdOctaves), std::end(kThirdOctaves));
}

FrequencyResponse measureSteppedResponse(const Device& device, const SteppedResponseSettings& settings)
{
    FrequencyResponse result;
    result.method = ResponseMethod::SteppedSine;
    result.sampleRate = settings.sampleRate;
    result.frequencyHz = settings.frequencies;

    signals::SteppedSineSettings stepSettings;
    stepSettings.sampleRate = settings.sampleRate;
    stepSettings.frequencies = settings.frequencies;
    stepSettings.levelDbfsPeak = settings.levelDbfs;
    stepSettings.latencySamples = settings.latencySamples;
    stepSettings.settleSeconds = settings.settleSeconds;
    stepSettings.measureSeconds = settings.measureSeconds;
    stepSettings.minimumPeriods = kMinimumPeriods;
    const signals::SteppedSine stepped = signals::makeSteppedSine(stepSettings, settings.channels);
    const juce::AudioBuffer<float> output = device(stepped.signal, settings.sampleRate);

    const StandardLowPass lowPass(settings.sampleRate);
    const size_t reference = findNearest(result.frequencyHz, kStandardFrequencyHz);
    result.referenceHz = result.frequencyHz[reference];
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const std::vector<double> in = lowPass.process(stepped.signal.getReadPointer(channel), stepped.signal.getNumSamples());
        const std::vector<double> out = lowPass.process(output.getReadPointer(channel), output.getNumSamples());
        ChannelResponse response;
        for (const signals::SineStep& step : stepped.steps)
        {
            const std::complex<double> x = getToneAmplitude(in, step.measureStart, step.measureLength, step.frequencyHz, settings.sampleRate);
            const std::complex<double> y = getToneAmplitude(out, step.measureStart, step.measureLength, step.frequencyHz, settings.sampleRate);
            response.response.push_back(y / x);
            const double broadband = getRms(out, step.measureStart, step.measureLength) / getRms(in, step.measureStart, step.measureLength);
            response.broadbandDb.push_back(20.0 * std::log10(std::max(broadband, 1.0e-30)));
        }
        finish(response, response.response[reference]);
        result.channels.push_back(response);
    }
    return result;
}

FrequencyResponse measureMultitoneResponse(const Device& device, const MultitoneResponseSettings& settings)
{
    FrequencyResponse result;
    result.method = ResponseMethod::Multitone;
    result.sampleRate = settings.sampleRate;
    result.validFromHz = settings.lowestHz;
    result.validToHz = settings.highestHz;

    int period = settings.periodSamples;
    if (period <= 0)
    {
        period = getPowerOfTwoAtLeast(kMultitonePeriodSeconds * settings.sampleRate);
    }
    signals::MultitoneSettings tones;
    tones.sampleRate = settings.sampleRate;
    tones.lowestHz = settings.lowestHz;
    tones.highestHz = settings.highestHz;
    tones.numberOfTones = settings.numberOfTones;
    tones.levelDbfsRms = settings.levelDbfs;
    tones.fftSize = period;
    tones.length = 2 * period; // the first period lets the device reach its periodic steady state, the second is analysed
    const juce::AudioBuffer<float> input = signals::makeMultitone(tones, settings.channels);
    const juce::AudioBuffer<float> output = device(input, settings.sampleRate);
    result.frequencyHz = signals::getMultitoneFrequencies(tones);
    const size_t reference = findNearest(result.frequencyHz, kStandardFrequencyHz);
    result.referenceHz = result.frequencyHz[reference];

    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const std::vector<double> in = toVector(input, channel);
        const std::vector<double> out = toVector(output, channel);
        ChannelResponse response;
        for (const double frequency : result.frequencyHz)
        {
            // synchronous analysis (AES17 annex A.3): every tone lies on a bin of the period, no window is needed
            const std::complex<double> x = getToneAmplitude(in, period, period, frequency, settings.sampleRate);
            const std::complex<double> y = getToneAmplitude(out, period, period, frequency, settings.sampleRate);
            response.response.push_back(y / x);
        }
        finish(response, response.response[reference]);
        result.channels.push_back(response);
    }
    return result;
}

FrequencyResponse measureSweptResponse(const Device& device, const SweepResponseSettings& settings)
{
    FrequencyResponse result;
    result.method = ResponseMethod::SweptSine;
    result.sampleRate = settings.sampleRate;

    signals::SweptSineSettings sweepSettings;
    sweepSettings.sampleRate = settings.sampleRate;
    sweepSettings.startHz = settings.startHz;
    sweepSettings.stopHz = settings.stopHz;
    if (sweepSettings.stopHz <= 0.0)
    {
        sweepSettings.stopHz = std::min(kSweepStopFraction * settings.sampleRate / 2.0, kSweepHighestStopHz);
    }
    sweepSettings.approximateSeconds = settings.approximateSeconds;
    sweepSettings.levelDbfsPeak = settings.levelDbfs;
    sweepSettings.preSilenceSeconds = kSweepPreSilenceSeconds;
    sweepSettings.postSilenceSeconds = std::max(kSweepPostSilenceSeconds, settings.windowAfterSeconds + kSweepPreSilenceSeconds);
    const signals::SweptSine sweep = signals::makeSweptSine(sweepSettings, settings.channels);
    const juce::AudioBuffer<float> output = device(sweep.signal, settings.sampleRate);
    result.validFromHz = std::max(2.0 * sweepSettings.startHz, kGridLowestHz);
    result.validToHz = std::min(sweepSettings.stopHz / 1.05, kUpperBandEdgeHz);

    result.frequencyHz = settings.frequencies;
    if (result.frequencyHz.empty())
    {
        for (double frequency = kGridLowestHz; frequency <= kUpperBandEdgeHz * 1.0001; frequency *= std::pow(2.0, 1.0 / kGridPointsPerOctave))
        {
            result.frequencyHz.push_back(frequency);
        }
    }

    // The window around the peak of an impulse response: before it at most half of the distance to the 2nd harmonic response (-L ln 2),
    // after it windowAfterSeconds; raised-cosine tapers over the first half of the part before and the last half of the part after the peak
    const int harmonicDistance = static_cast<int>(signals::getHarmonicAdvanceSamples(sweep, 2));
    const auto window = [&](const std::vector<float>& impulse, int peak, int& firstTime)
    {
        const int size = static_cast<int>(impulse.size());
        const int before = std::min(static_cast<int>(settings.windowBeforeSeconds * settings.sampleRate), harmonicDistance / 2);
        const int after = std::min(static_cast<int>(settings.windowAfterSeconds * settings.sampleRate), size / 2 - 1);
        std::vector<double> segment;
        for (int offset = -before; offset <= after; ++offset)
        {
            const int index = ((peak + offset) % size + size) % size;
            double weight = getTaper(offset + before, before / 2);
            if (offset > after / 2)
            {
                weight = getTaper(after - offset, after / 2);
            }
            segment.push_back(weight * impulse[static_cast<size_t>(index)]);
        }
        firstTime = peak - before;
        return segment;
    };

    // The reference channel: the stimulus itself through the same deconvolution and window. Its band-edge ripple (the sweep starts and stops
    // abruptly) is the same as in the device's response and cancels in the ratio (as in a dual-channel FFT analyser).
    const std::vector<float> stimulus(sweep.signal.getReadPointer(0), sweep.signal.getReadPointer(0) + sweep.signal.getNumSamples());
    int referenceFirst = 0;
    const std::vector<double> referenceSegment = window(signals::deconvolveSweptSine(sweep, stimulus), 0, referenceFirst);
    std::vector<std::complex<double>> identity;
    for (const double frequency : result.frequencyHz)
    {
        identity.push_back(getDtft(referenceSegment, referenceFirst, frequency, settings.sampleRate));
    }
    const std::complex<double> identityAtReference = getDtft(referenceSegment, referenceFirst, kStandardFrequencyHz, settings.sampleRate);

    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const std::vector<float> response(output.getReadPointer(channel), output.getReadPointer(channel) + output.getNumSamples());
        const std::vector<float> impulse = signals::deconvolveSweptSine(sweep, response);
        // the linear impulse response: its peak in the first half (the harmonic responses lie at negative times, i.e. at the end of the buffer)
        int peak = 0;
        for (int index = 1; index < static_cast<int>(impulse.size()) / 2; ++index)
        {
            if (std::abs(impulse[static_cast<size_t>(index)]) > std::abs(impulse[static_cast<size_t>(peak)]))
            {
                peak = index;
            }
        }
        int firstTime = 0;
        const std::vector<double> segment = window(impulse, peak, firstTime);
        ChannelResponse channelResponse;
        for (size_t index = 0; index < result.frequencyHz.size(); ++index)
        {
            channelResponse.response.push_back(getDtft(segment, firstTime, result.frequencyHz[index], settings.sampleRate) / identity[index]);
        }
        finish(channelResponse, getDtft(segment, firstTime, kStandardFrequencyHz, settings.sampleRate) / identityAtReference);
        result.channels.push_back(channelResponse);
    }
    return result;
}
}
