#include "pluginlab/measure/SweptImpulse.h"

#include <cmath>

#include "pluginlab/signals/SweptSine.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kSweepStopFraction = 0.95;         // of Nyquist
constexpr double kSweepHighestStopHz = 40000.0;
constexpr double kSweepPreSilenceSeconds = 0.1;
constexpr double kSweepPostSilenceSeconds = 1.0;
constexpr double kValidStopFraction = 1.05;         // valid up to the stop frequency / 1.05
constexpr int kRenormaliseEvery = 1024;             // the rotating phasor of the DTFT is renormalised this often

// A raised-cosine taper: 0 at the first sample, 1 after `length` samples
double getTaper(int sample, int length)
{
    if (length <= 0 || sample >= length)
    {
        return 1.0;
    }
    return 0.5 - 0.5 * std::cos(kPi * sample / length);
}

// The window around a peak of a circular impulse response: before it `before` samples, after it `after`; raised-cosine tapers over the first half
// of the part before and the last half of the part after the peak
std::vector<double> makeWindowedSegment(const std::vector<float>& impulse, int peak, int before, int after)
{
    const int size = static_cast<int>(impulse.size());
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
    return segment;
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

SweptImpulses measureSweptImpulses(const Device& device, const SweepResponseSettings& settings)
{
    SweptImpulses result;
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
    result.rateSeconds = sweep.rate;
    result.validFromHz = std::max(2.0 * sweepSettings.startHz, 20.0);
    result.validToHz = std::min(sweepSettings.stopHz / kValidStopFraction, kUpperBandEdgeHz);

    // the window: before the peak at most half of the distance to the 2nd harmonic response (-L ln 2), after it windowAfterSeconds
    const int harmonicDistance = static_cast<int>(signals::getHarmonicAdvanceSamples(sweep, 2));
    const int before = std::min(static_cast<int>(settings.windowBeforeSeconds * settings.sampleRate), harmonicDistance / 2);

    // the reference channel: the stimulus itself through the same deconvolution and window. Its band-edge ripple (the sweep starts and stops
    // abruptly) is the same as in the device's response and cancels in the ratio (as in a dual-channel FFT analyser)
    const std::vector<float> stimulus(sweep.signal.getReadPointer(0), sweep.signal.getReadPointer(0) + sweep.signal.getNumSamples());
    const std::vector<float> referenceImpulse = signals::deconvolveSweptSine(sweep, stimulus);
    const int after = std::min(static_cast<int>(settings.windowAfterSeconds * settings.sampleRate), static_cast<int>(referenceImpulse.size()) / 2 - 1);
    result.referenceSegment = makeWindowedSegment(referenceImpulse, 0, before, after);
    result.referenceFirstTime = -before;

    for (int channel = 0; channel < settings.channels; ++channel)
    {
        SweptChannel swept;
        const std::vector<float> response(output.getReadPointer(channel), output.getReadPointer(channel) + output.getNumSamples());
        swept.impulse = signals::deconvolveSweptSine(sweep, response);
        // the linear impulse response: its peak in the first half (the harmonic responses lie at negative times, i.e. at the end of the buffer)
        for (int index = 1; index < static_cast<int>(swept.impulse.size()) / 2; ++index)
        {
            if (std::abs(swept.impulse[static_cast<size_t>(index)]) > std::abs(swept.impulse[static_cast<size_t>(swept.peak)]))
            {
                swept.peak = index;
            }
        }
        swept.segment = makeWindowedSegment(swept.impulse, swept.peak, before, after);
        swept.firstTime = swept.peak - before;
        result.channels.push_back(swept);
    }
    return result;
}

std::complex<double> getSweptResponse(const SweptImpulses& impulses, int channel, double frequencyHz)
{
    const SweptChannel& swept = impulses.channels[static_cast<size_t>(channel)];
    return getDtft(swept.segment, swept.firstTime, frequencyHz, impulses.sampleRate)
           / getDtft(impulses.referenceSegment, impulses.referenceFirstTime, frequencyHz, impulses.sampleRate);
}
}
