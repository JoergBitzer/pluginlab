#pragma once

#include <complex>

#include "pluginlab/reference/Processor.h"

namespace pluginlab::reference
{
// A linear time-invariant reference processor: it processes audio and knows its own exact frequency response H(e^jw).
// The processing runs in double precision with a state per channel.
class LinearProcessor : public Processor
{
public:
    explicit LinearProcessor(double sampleRate);

    double getSampleRate() const;

    // The number of channels with a state of their own (clears the state); process() sets it to the channels of the buffer
    void setNumChannels(int channels);
    int getNumChannels() const;

    virtual double processSample(int channel, double input) = 0;

    // The exact frequency response at a frequency (the definition the processing is tested against)
    virtual std::complex<double> getResponse(double frequencyHz) const = 0;

    void process(juce::AudioBuffer<float>& buffer) override;
    void process(juce::AudioBuffer<double>& buffer);

protected:
    // Resizes and clears the state for the number of channels
    virtual void prepareChannels(int channels) = 0;

private:
    double m_sampleRate;
    int m_channels = 0;
};
}
