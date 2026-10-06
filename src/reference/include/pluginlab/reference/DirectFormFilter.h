#pragma once

#include <vector>

#include "pluginlab/reference/LinearProcessor.h"

namespace pluginlab::reference
{
// A filter of any order: an integer delay, then H(z) = (b0 + b1 z^-1 + ...) / (a0 + a1 z^-1 + ...) in transposed direct form II.
// Used for FIR filters (denominator {1}), delays and all-passes of higher order; biquad cascades are better for high-order IIR filters.
class DirectFormFilter : public LinearProcessor
{
public:
    DirectFormFilter(std::vector<double> numerator, std::vector<double> denominator, double sampleRate, int delaySamples = 0,
                     int latencySamples = 0);

    const std::vector<double>& getNumerator() const;
    const std::vector<double>& getDenominator() const;
    int getDelaySamples() const;

    void reset() override;
    double processSample(int channel, double input) override;
    std::complex<double> getResponse(double frequencyHz) const override;
    int getLatencySamples() const override;

protected:
    void prepareChannels(int channels) override;

private:
    struct ChannelState
    {
        std::vector<double> delayLine;
        int delayPosition = 0;
        std::vector<double> registers;
    };

    std::vector<double> m_numerator;
    std::vector<double> m_denominator;
    int m_delaySamples;
    int m_latencySamples;
    std::vector<ChannelState> m_state;
};
}
