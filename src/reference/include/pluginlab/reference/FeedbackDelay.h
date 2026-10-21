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
// The shape of the LFO on the delay time (W11.1d): s(phi) = sin(phi), or the triangle (2/pi) asin(sin(phi)) with the same peaks and zeros
enum class LfoShape
{
    Sine,
    Triangle
};

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
    // stereo (W11.1c, the author's cross feedback); a buffer with one channel ignores both
    double crossFeedback = 0.0;       // each line also gets this much of the other channel's wet signal (the author's m_Crossfeedback)
    bool pingPong = false;            // the input (L + R) / 2 goes into the left line only, the feedback goes across (left wet into the right
                                      // line and back, gain `feedback`; crossFeedback is not used): the echoes alternate L, R, L, ...
    // the LFO on the delay time (W11.1d): tau(n) = delaySamples + lfoDepthSamples s(2 pi lfoRateHz n / fs), n counted from the reset; the
    // interpolation is then always Lagrange (its taps per sample), and delaySamples - lfoDepthSamples >= 3. Depth 0: no LFO.
    double lfoRateHz = 0.0;
    double lfoDepthSamples = 0.0;
    LfoShape lfoShape = LfoShape::Sine;
    // a saturation in the loop (W11.1e): what is written into the line is s(u) = tanh(drive u) / drive (gain 1 for small signals, at most
    // 1 / drive); 0: linear. With a saturation |feedback| may reach 1.5: the loop then oscillates at a level the saturation holds.
    double saturationDrive = 0.0;
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
    // maximumDelaySamples: the longest delay setSettings may set later (0: the delay of these settings)
    FeedbackDelay(double sampleRate, const FeedbackDelaySettings& settings, double maximumDelaySamples = 0.0);

    // New settings; the delay line and the filters' states are kept (the echoes in the line go on with the new time, feedback and filters:
    // a jump, the "digital" switch of the author's class). The delay is limited to the maximum given at construction.
    void setSettings(const FeedbackDelaySettings& settings);
    const FeedbackDelaySettings& getSettings() const;

    // One trip through the delay line and the loop filters (without the feedback gain): F(f) I(f) e^(-j w Dint). The transfer functions
    // below assume no LFO (with one the delay is time-varying: getDelayAt gives its trajectory)
    std::complex<double> getTripResponse(double frequencyHz) const;
    // What colours each trip: the loop filters and the interpolator, |F(f) I(f)| (the magnitude of one trip)
    double getLoopFilterMagnitude(double frequencyHz) const;
    // The whole transfer function of one channel (a buffer with one channel; stereo without cross feedback and ping-pong)
    std::complex<double> getResponse(double frequencyHz) const;
    // Stereo: from input channel `input` to output channel `output` (0 left, 1 right). With the lines v = B x + F w, w = T v (one trip):
    // y = (1 - mix) x + mix (I - T F)^-1 T B x, B the input matrix (I, or [[1/2, 1/2], [0, 0]] for ping-pong), F the feedback matrix
    // ([[g, c], [c, g]], or [[0, g], [g, 0]] for ping-pong; times z^-1 in the author's order)
    std::complex<double> getResponse(int output, int input, double frequencyHz) const;
    // Echo k (k >= 1): its arrival in samples (k D, plus k - 1 in the author's order) and its broadband gain mix g^(k-1)
    double getEchoTime(int echo) const;
    // The delay at a sample counted from the reset (the LFO's trajectory; constant without LFO): output n reads the input of n - tau(n)
    double getDelayAt(juce::int64 sample) const;
    double getEchoGain(int echo) const;
    // With the saturation: the echoes of an impulse of the given height (one channel, whole-sample delay, no loop filters, no LFO): echo k is
    // mix a_k with a_1 = s(height), a_(k+1) = s(feedback a_k)
    std::vector<double> getSaturatedEchoes(double height, int count) const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    void configure();
    double saturate(double value) const;
    void setLagrange(double delaySamples);
    double readDelayed(size_t channel);
    // the feedback from the wet signal of channel `from` into the line of channel `to`, and the input of channel `from` into that line
    // (stereo: two channels; otherwise the cross feedback and ping-pong do not apply)
    double getFeedbackGain(size_t to, size_t from, bool stereo) const;
    double getInputGain(size_t to, size_t from, bool stereo) const;
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
        double wet = 0.0;                 // this sample's wet output
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
    juce::int64 m_lfoSample = 0;          // samples since the reset (the LFO's phase)
    std::vector<ChannelState> m_channels;
};
}
