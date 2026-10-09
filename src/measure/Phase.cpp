#include "pluginlab/measure/Phase.h"

#include <algorithm>
#include <cmath>

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesPerRadian = 180.0 / kPi;
constexpr double kGridLowestHz = 20.0;
constexpr int kGridPointsPerOctave = 48;
constexpr int kRenormaliseEvery = 1024;
constexpr double kPassbandFloorDb = -40.0;   // the fit and the summary of the phase deviation use only frequencies within 40 dB of the largest gain

std::vector<double> makeGrid()
{
    std::vector<double> grid;
    for (double frequency = kGridLowestHz; frequency <= kUpperBandEdgeHz * 1.0001; frequency *= std::pow(2.0, 1.0 / kGridPointsPerOctave))
    {
        grid.push_back(frequency);
    }
    return grid;
}

// Unwraps phases (radians) along the frequency grid: each step is predicted from the group delay (the mean of both points), and the 2 pi multiple
// nearest to the prediction is chosen, so that even a steep phase between two grid points is followed
std::vector<double> unwrap(const std::vector<double>& wrapped, const std::vector<double>& frequencies, const std::vector<double>& groupDelay,
                           double sampleRate)
{
    std::vector<double> phase(wrapped.size());
    if (wrapped.empty())
    {
        return phase;
    }
    phase[0] = wrapped[0];
    for (size_t index = 1; index < wrapped.size(); ++index)
    {
        const double step = 2.0 * kPi * (frequencies[index] - frequencies[index - 1]) / sampleRate;
        const double predicted = phase[index - 1] - 0.5 * (groupDelay[index] + groupDelay[index - 1]) * step;
        phase[index] = wrapped[index] + 2.0 * kPi * std::round((predicted - wrapped[index]) / (2.0 * kPi));
    }
    return phase;
}
}

double getSegmentGroupDelay(const std::vector<double>& segment, int firstTime, double frequencyHz, double sampleRate)
{
    const double omega = 2.0 * kPi * frequencyHz / sampleRate;
    const std::complex<double> step = std::polar(1.0, -omega);
    std::complex<double> phasor = std::polar(1.0, -omega * firstTime);
    std::complex<double> sum = 0.0;
    std::complex<double> weighted = 0.0;
    for (size_t index = 0; index < segment.size(); ++index)
    {
        const double time = firstTime + static_cast<double>(index);
        sum += segment[index] * phasor;
        weighted += time * segment[index] * phasor;
        phasor *= step;
        if (index % kRenormaliseEvery == kRenormaliseEvery - 1)
        {
            phasor = std::polar(1.0, -omega * (time + 1.0));
        }
    }
    return (weighted / sum).real();
}

PhaseResponse measurePhaseResponse(const Device& device, const PhaseSettings& settings)
{
    PhaseResponse result;
    result.settings = settings;
    result.frequencyHz = settings.frequencies;
    if (result.frequencyHz.empty())
    {
        result.frequencyHz = makeGrid();
    }
    SweepResponseSettings sweep;
    sweep.sampleRate = settings.sampleRate;
    sweep.levelDbfs = settings.levelDbfs;
    sweep.channels = settings.channels;
    const SweptImpulses impulses = measureSweptImpulses(device, sweep);
    result.validFromHz = impulses.validFromHz;
    result.validToHz = impulses.validToHz;
    const size_t count = result.frequencyHz.size();
    for (size_t index = 1; index < count; ++index)
    {
        result.differenceFrequencyHz.push_back(std::sqrt(result.frequencyHz[index] * result.frequencyHz[index - 1]));
    }

    std::vector<std::vector<std::complex<double>>> responses;
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const SweptChannel& swept = impulses.channels[static_cast<size_t>(channel)];
        ChannelPhase phase;
        phase.delaySamples = swept.peak;
        std::vector<std::complex<double>> response;
        std::vector<double> wrapped;
        for (const double frequency : result.frequencyHz)
        {
            const std::complex<double> h = getSweptResponse(impulses, channel, frequency);
            response.push_back(h);
            // the delay at the peak removed (AES17 6.8.3 b), so that the rest of the phase changes slowly
            wrapped.push_back(std::arg(h * std::polar(1.0, 2.0 * kPi * frequency * swept.peak / settings.sampleRate)));
            // the group delay of the ratio device / reference = that of the device window minus that of the reference window
            phase.groupDelaySamples.push_back(getSegmentGroupDelay(swept.segment, swept.firstTime, frequency, settings.sampleRate)
                                              - getSegmentGroupDelay(impulses.referenceSegment, impulses.referenceFirstTime, frequency, settings.sampleRate));
        }
        std::vector<double> residualDelay(count);
        for (size_t index = 0; index < count; ++index)
        {
            residualDelay[index] = phase.groupDelaySamples[index] - swept.peak;
        }
        const std::vector<double> unwrapped = unwrap(wrapped, result.frequencyHz, residualDelay, settings.sampleRate);

        // the passband: within the band of validity and within 40 dB of the largest gain (in a deep stop band the phase is noise)
        double largestGain = 0.0;
        for (size_t index = 0; index < count; ++index)
        {
            largestGain = std::max(largestGain, std::abs(response[index]));
        }
        const double floorGain = largestGain * std::pow(10.0, kPassbandFloorDb / 20.0);
        std::vector<bool> inPassband(count);
        for (size_t index = 0; index < count; ++index)
        {
            const double f = result.frequencyHz[index];
            inPassband[index] = f >= result.validFromHz && f <= result.validToHz && std::abs(response[index]) >= floorGain;
        }
        // the straight line a + b f through the phase (least squares over the passband; AES17 6.8.3 a)
        double sumF = 0.0;
        double sumP = 0.0;
        double sumFF = 0.0;
        double sumFP = 0.0;
        double points = 0.0;
        for (size_t index = 0; index < count; ++index)
        {
            if (!inPassband[index])
            {
                continue;
            }
            const double f = result.frequencyHz[index];
            const double p = unwrapped[index] * kDegreesPerRadian;
            sumF += f;
            sumP += p;
            sumFF += f * f;
            sumFP += f * p;
            points += 1.0;
        }
        const double slope = (points * sumFP - sumF * sumP) / (points * sumFF - sumF * sumF);
        phase.fitInterceptDegrees = (sumP - slope * sumF) / points;
        phase.fitDelaySamples = swept.peak - slope * settings.sampleRate / 360.0;
        bool first = true;
        for (size_t index = 0; index < count; ++index)
        {
            const double degrees = unwrapped[index] * kDegreesPerRadian;
            phase.phaseDegrees.push_back(degrees);
            const double deviation = degrees - (phase.fitInterceptDegrees + slope * result.frequencyHz[index]);
            phase.deviationFromLinearDegrees.push_back(deviation);
            if (inPassband[index])
            {
                if (first)
                {
                    phase.deviationMaxDegrees = deviation;
                    phase.deviationMinDegrees = deviation;
                    first = false;
                }
                phase.deviationMaxDegrees = std::max(phase.deviationMaxDegrees, deviation);
                phase.deviationMinDegrees = std::min(phase.deviationMinDegrees, deviation);
            }
        }
        // AES17 6.8.4: the group delay as the difference of neighbouring (unwrapped) phases, including the removed delay
        for (size_t index = 1; index < count; ++index)
        {
            const double step = 2.0 * kPi * (result.frequencyHz[index] - result.frequencyHz[index - 1]) / settings.sampleRate;
            phase.groupDelayDifferenceSamples.push_back(swept.peak - (unwrapped[index] - unwrapped[index - 1]) / step);
        }
        responses.push_back(response);
        result.channels.push_back(phase);
    }

    // AES17 6.2.7: the phase of every channel relative to the first one
    for (size_t channel = 0; channel < result.channels.size(); ++channel)
    {
        for (size_t index = 0; index < count; ++index)
        {
            result.channels[channel].interChannelDegrees.push_back(std::arg(responses[channel][index] / responses[0][index]) * kDegreesPerRadian);
        }
    }
    return result;
}
}
