#pragma once

#include <complex>
#include <vector>

#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Processor.h"

namespace pluginlab::reference
{
// The fractional part of the delay: a Lagrange interpolator of order 3 (the author's tape glide: nodes -1, 0, 1, 2 around the read position; its
// magnitude falls towards Nyquist and leaves its mark in the loop filter) or a first-order Thiran all-pass (|H| = 1, the delay depends on frequency)
enum class DelayInterpolation
{
    Lagrange,
    AllPass
};

// The order inside the loop: Exact reads the delayed sample before the new input is written (one trip around the loop takes D samples); AuthorsOrder
// is the author's original (jade::BasicDelayEffect): the input written now gets the previous sample's output, one trip takes D + 1 samples, the
// echoes come at D, 2D + 1, 3D + 2, ... (a teaching example for the echo response: the spacing is one sample longer than the first echo)
enum class LoopOrder
{
    Exact,
    AuthorsOrder
};

struct FeedbackDelaySettings
{
    double delaySamples = 4800.0;     // >= 3 (the interpolators need their neighbours before the newest sample)
    double feedback = 0.5;            // the gain of one trip around the loop (negative: inverted on every trip); |feedback| < 1
    double mix = 0.5;                 // y = (1 - mix) x + mix wet
    double lowCutHz = 0.0;            // a first-order Butterworth high-pass in the loop (0: none)
    double highCutHz = 0.0;           // a first-order Butterworth low-pass in the loop (0: none)
    DelayInterpolation interpolation = DelayInterpolation::Lagrange;
    LoopOrder order = LoopOrder::Exact;
};

// The reference delay of W11.1 (docs/design/W11-delay-plan.md 7), ported from the author's jade::BasicDelayEffect (BSD 3-Clause, 2024) with the
// corrections of the plan (every channel gets its input, the loop order): per channel a circular buffer read at D samples (fractional), the read
// sample through the loop filters is the wet signal and goes back, times the feedback, into the buffer with the input:
//   w(n) = F(z) I(z) z^-Dint x_loop(n),  x_loop(n) = x(n) + g w(n)  (Exact)     y(n) = (1 - mix) x(n) + mix w(n)
// F the loop filters, I the interpolator. The answer: T(f) = F I z^-Dint is one trip, H = (1 - mix) + mix T / (1 - g T) (AuthorsOrder: g T z^-1 in
// the denominator); echo k (k >= 1) is mix g^(k-1) T^k (z^-(k-1) more in the author's order).
class FeedbackDelay : public Processor
{
public:
    FeedbackDelay(double sampleRate, const FeedbackDelaySettings& settings);

    const FeedbackDelaySettings& getSettings() const;

    // One trip through the delay line and the loop filters (without the feedback gain): F(f) I(f) e^(-j w Dint)
    std::complex<double> getTripResponse(double frequencyHz) const;
    // What colours each trip: the loop filters and the interpolator, |F(f) I(f)| (the magnitude of one trip)
    double getLoopFilterMagnitude(double frequencyHz) const;
    // The whole transfer function
    std::complex<double> getResponse(double frequencyHz) const;
    // Echo k (k >= 1): its arrival in samples (k D, plus k - 1 in the author's order) and its broadband gain mix g^(k-1)
    double getEchoTime(int echo) const;
    double getEchoGain(int echo) const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    double readDelayed(size_t channel);
    double filter(size_t channel, double value);
    void prepareChannels(int channels);

    struct FilterState
    {
        double z1 = 0.0;
    };

    struct ChannelState
    {
        std::vector<double> line;
        double lastOutput = 0.0;          // the author's order: the previous sample's wet output
        double allPassInput = 0.0;        // x(n - 1) of the all-pass
        double allPassOutput = 0.0;       // y(n - 1) of the all-pass
        FilterState lowCut;
        FilterState highCut;
    };

    double m_sampleRate;
    FeedbackDelaySettings m_settings;
    int m_integerDelay = 0;               // Lagrange: floor(D), the fraction between Dint and Dint + 1; all-pass: D minus the all-pass's delay
    std::vector<double> m_lagrange;       // the weights of the samples Dint - 1, Dint, Dint + 1, Dint + 2 samples back
    double m_allPassCoefficient = 0.0;
    BiquadCoefficients m_lowCut;          // first-order sections (b2 = a2 = 0); identity when off
    BiquadCoefficients m_highCut;
    size_t m_size = 0;
    size_t m_writePosition = 0;
    std::vector<ChannelState> m_channels;
};
}
