#pragma once

#include <complex>
#include <vector>

#include "pluginlab/reference/LinearProcessor.h"

namespace pluginlab::reference
{
// H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2); a first-order section has b2 = a2 = 0
struct BiquadCoefficients
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

// The coefficients divided by a0
BiquadCoefficients makeBiquad(double b0, double b1, double b2, double a0, double a1, double a2);

std::complex<double> getBiquadResponse(const BiquadCoefficients& section, double frequencyHz, double sampleRate);
std::complex<double> getCascadeResponse(const std::vector<BiquadCoefficients>& sections, double frequencyHz, double sampleRate);

// Both poles inside the unit circle (the stability triangle |a2| < 1, |a1| < 1 + a2)
bool isStable(const BiquadCoefficients& section);

// Biquad sections in series, each in transposed direct form II
class BiquadCascade : public LinearProcessor
{
public:
    BiquadCascade(std::vector<BiquadCoefficients> sections, double sampleRate);

    // New coefficients, the state is kept
    void setSections(std::vector<BiquadCoefficients> sections);
    const std::vector<BiquadCoefficients>& getSections() const;

    void reset() override;
    double processSample(int channel, double input) override;
    std::complex<double> getResponse(double frequencyHz) const override;

protected:
    void prepareChannels(int channels) override;

private:
    struct SectionState
    {
        double z1 = 0.0;
        double z2 = 0.0;
    };

    std::vector<BiquadCoefficients> m_sections;
    std::vector<std::vector<SectionState>> m_state; // [channel][section]
};
}
