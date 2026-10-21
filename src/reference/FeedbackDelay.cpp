#include "pluginlab/reference/FeedbackDelay.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "pluginlab/reference/Designs.h"

namespace pluginlab::reference
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kLagrangeTaps = 4;
constexpr int kLineMargin = 4;           // the line holds the longest tap plus the newest sample

BiquadCoefficients makeLoopFilter(Pass pass, double cornerHz, double sampleRate)
{
    if (cornerHz <= 0.0)
    {
        return {};
    }
    return designButterworth(pass, 1, sampleRate, cornerHz).front();
}
}

FeedbackDelay::FeedbackDelay(double sampleRate, const FeedbackDelaySettings& settings, double maximumDelaySamples)
    : m_sampleRate(sampleRate), m_settings(settings)
{
    const double longest = std::max(settings.delaySamples + settings.lfoDepthSamples, maximumDelaySamples);
    m_size = static_cast<size_t>(std::floor(longest)) + kLineMargin;
    configure();
}

void FeedbackDelay::setSettings(const FeedbackDelaySettings& settings)
{
    m_settings = settings;
    const double longest = static_cast<double>(m_size - kLineMargin);
    m_settings.delaySamples = std::min(settings.delaySamples, longest);
    // the LFO stays inside the line and above the shortest delay the interpolator needs
    m_settings.lfoDepthSamples = std::clamp(settings.lfoDepthSamples, 0.0, std::max(0.0, std::min(m_settings.delaySamples - 3.0, longest - m_settings.delaySamples)));
    configure();
}

void FeedbackDelay::configure()
{
    const FeedbackDelaySettings& settings = m_settings;
    jassert(settings.delaySamples >= 3.0);
    jassert(std::abs(settings.feedback) < 1.0 || (settings.saturationDrive > 0.0 && std::abs(settings.feedback) <= 1.5));
    if (settings.interpolation == DelayInterpolation::Lagrange || settings.lfoDepthSamples > 0.0)
    {
        setLagrange(settings.delaySamples);
    }
    else
    {
        // the all-pass takes a delay in [0.5, 1.5) (Thiran of order 1: a = (1 - d) / (1 + d)), the line the rest
        m_integerDelay = static_cast<int>(std::floor(settings.delaySamples - 0.5));
        const double delay = settings.delaySamples - m_integerDelay;
        m_allPassCoefficient = (1.0 - delay) / (1.0 + delay);
    }
    m_lowCut = makeLoopFilter(Pass::High, settings.lowCutHz, m_sampleRate);
    m_highCut = makeLoopFilter(Pass::Low, settings.highCutHz, m_sampleRate);
}

// the taps at Dint - 1 ... Dint + 2 samples back, the delay d = 1 + fraction inside them: h_k = prod_(n != k) (d - n) / (k - n)
void FeedbackDelay::setLagrange(double delaySamples)
{
    m_integerDelay = static_cast<int>(std::floor(delaySamples));
    const double delay = 1.0 + delaySamples - m_integerDelay;
    m_lagrange.assign(kLagrangeTaps, 1.0);
    for (int k = 0; k < kLagrangeTaps; ++k)
    {
        for (int n = 0; n < kLagrangeTaps; ++n)
        {
            if (n != k)
            {
                m_lagrange[static_cast<size_t>(k)] *= (delay - n) / (k - n);
            }
        }
    }
}

double FeedbackDelay::saturate(double value) const
{
    if (m_settings.saturationDrive <= 0.0)
    {
        return value;
    }
    return std::tanh(m_settings.saturationDrive * value) / m_settings.saturationDrive;
}

std::vector<double> FeedbackDelay::getSaturatedEchoes(double height, int count) const
{
    std::vector<double> echoes;
    double level = saturate(height);
    for (int echo = 0; echo < count; ++echo)
    {
        echoes.push_back(m_settings.mix * level);
        level = saturate(m_settings.feedback * level);
    }
    return echoes;
}

double FeedbackDelay::getDelayAt(juce::int64 sample) const
{
    if (m_settings.lfoDepthSamples <= 0.0)
    {
        return m_settings.delaySamples;
    }
    const double phase = 2.0 * kPi * m_settings.lfoRateHz * static_cast<double>(sample) / m_sampleRate;
    double shape = std::sin(phase);
    if (m_settings.lfoShape == LfoShape::Triangle)
    {
        shape = 2.0 / kPi * std::asin(shape);
    }
    return m_settings.delaySamples + m_settings.lfoDepthSamples * shape;
}

const FeedbackDelaySettings& FeedbackDelay::getSettings() const
{
    return m_settings;
}

double FeedbackDelay::getLoopFilterMagnitude(double frequencyHz) const
{
    return std::abs(getTripResponse(frequencyHz));
}

std::complex<double> FeedbackDelay::getTripResponse(double frequencyHz) const
{
    const double omega = 2.0 * kPi * frequencyHz / m_sampleRate;
    const std::complex<double> z1 = std::polar(1.0, -omega); // z^-1
    std::complex<double> interpolator;
    if (m_settings.interpolation == DelayInterpolation::Lagrange)
    {
        for (int k = 0; k < kLagrangeTaps; ++k)
        {
            interpolator += m_lagrange[static_cast<size_t>(k)] * std::polar(1.0, -omega * (k - 1));
        }
    }
    else
    {
        interpolator = (m_allPassCoefficient + z1) / (1.0 + m_allPassCoefficient * z1);
    }
    const std::complex<double> filters = getBiquadResponse(m_lowCut, frequencyHz, m_sampleRate) * getBiquadResponse(m_highCut, frequencyHz, m_sampleRate);
    return filters * interpolator * std::polar(1.0, -omega * m_integerDelay);
}

std::complex<double> FeedbackDelay::getResponse(double frequencyHz) const
{
    const std::complex<double> trip = getTripResponse(frequencyHz);
    std::complex<double> loop = m_settings.feedback * trip;
    if (m_settings.order == LoopOrder::AuthorsOrder)
    {
        loop *= std::polar(1.0, -2.0 * kPi * frequencyHz / m_sampleRate);
    }
    return (1.0 - m_settings.mix) + m_settings.mix * trip / (1.0 - loop);
}

double FeedbackDelay::getEchoTime(int echo) const
{
    double time = echo * m_settings.delaySamples;
    if (m_settings.order == LoopOrder::AuthorsOrder)
    {
        time += echo - 1;
    }
    return time;
}

double FeedbackDelay::getEchoGain(int echo) const
{
    return m_settings.mix * std::pow(m_settings.feedback, echo - 1);
}

void FeedbackDelay::reset()
{
    for (ChannelState& state : m_channels)
    {
        std::fill(state.line.begin(), state.line.end(), 0.0);
        state.lastOutput = 0.0;
        state.wet = 0.0;
        state.allPassInput = 0.0;
        state.allPassOutput = 0.0;
        state.lowCut = {};
        state.highCut = {};
    }
    m_writePosition = 0;
    m_lfoSample = 0;
}

void FeedbackDelay::prepareChannels(int channels)
{
    m_channels.assign(static_cast<size_t>(channels), ChannelState{});
    for (ChannelState& state : m_channels)
    {
        state.line.assign(m_size, 0.0);
    }
    m_writePosition = 0;
    m_lfoSample = 0;
}

// the sample written `back` samples ago (0: the one written at this position in this sample period)
double FeedbackDelay::readDelayed(size_t channel)
{
    ChannelState& state = m_channels[channel];
    auto at = [this, &state](int back)
    {
        return state.line[(m_writePosition + m_size - static_cast<size_t>(back)) % m_size];
    };
    if (m_settings.interpolation == DelayInterpolation::Lagrange || m_settings.lfoDepthSamples > 0.0)
    {
        double value = 0.0;
        for (int k = 0; k < kLagrangeTaps; ++k)
        {
            value += m_lagrange[static_cast<size_t>(k)] * at(m_integerDelay - 1 + k);
        }
        return value;
    }
    // y(n) = a x(n) + x(n - 1) - a y(n - 1)
    const double input = at(m_integerDelay);
    const double output = m_allPassCoefficient * input + state.allPassInput - m_allPassCoefficient * state.allPassOutput;
    state.allPassInput = input;
    state.allPassOutput = output;
    return output;
}

// the loop filters (transposed direct form II, first order)
double FeedbackDelay::filter(size_t channel, double value)
{
    ChannelState& state = m_channels[channel];
    for (auto [section, filterState] : {std::pair{&m_lowCut, &state.lowCut}, std::pair{&m_highCut, &state.highCut}})
    {
        const double output = section->b0 * value + filterState->z1;
        filterState->z1 = section->b1 * value - section->a1 * output;
        value = output;
    }
    return value;
}

double FeedbackDelay::getFeedbackGain(size_t to, size_t from, bool stereo) const
{
    if (! stereo)
    {
        return m_settings.feedback;
    }
    if (m_settings.pingPong)
    {
        if (to == from)
        {
            return 0.0;
        }
        return m_settings.feedback;
    }
    if (to == from)
    {
        return m_settings.feedback;
    }
    return m_settings.crossFeedback;
}

double FeedbackDelay::getInputGain(size_t to, size_t from, bool stereo) const
{
    if (stereo && m_settings.pingPong)
    {
        if (to == 0)
        {
            return 0.5;
        }
        return 0.0;
    }
    if (to == from)
    {
        return 1.0;
    }
    return 0.0;
}

std::complex<double> FeedbackDelay::getResponse(int output, int input, double frequencyHz) const
{
    using Matrix = std::complex<double>[2][2];
    const std::complex<double> trip = getTripResponse(frequencyHz);
    std::complex<double> loopDelay = 1.0;
    if (m_settings.order == LoopOrder::AuthorsOrder)
    {
        loopDelay = std::polar(1.0, -2.0 * kPi * frequencyHz / m_sampleRate);
    }
    // A = I - T F z^(-1 in the author's order), its inverse, then (A^-1 T B)[output][input]
    Matrix a;
    for (size_t row = 0; row < 2; ++row)
    {
        for (size_t column = 0; column < 2; ++column)
        {
            a[row][column] = -trip * getFeedbackGain(row, column, true) * loopDelay;
            if (row == column)
            {
                a[row][column] += 1.0;
            }
        }
    }
    const std::complex<double> determinant = a[0][0] * a[1][1] - a[0][1] * a[1][0];
    Matrix inverse = {{a[1][1] / determinant, -a[0][1] / determinant}, {-a[1][0] / determinant, a[0][0] / determinant}};
    const size_t out = static_cast<size_t>(output);
    const size_t in = static_cast<size_t>(input);
    std::complex<double> wet;
    for (size_t k = 0; k < 2; ++k)
    {
        wet += inverse[out][k] * trip * getInputGain(k, in, true);
    }
    std::complex<double> dry = 0.0;
    if (out == in)
    {
        dry = 1.0 - m_settings.mix;
    }
    return dry + m_settings.mix * wet;
}

void FeedbackDelay::process(juce::AudioBuffer<float>& buffer)
{
    if (static_cast<int>(m_channels.size()) != buffer.getNumChannels())
    {
        prepareChannels(buffer.getNumChannels());
    }
    const size_t channels = m_channels.size();
    const bool stereo = channels == 2;
    const bool modulated = m_settings.lfoDepthSamples > 0.0;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        if (modulated)
        {
            setLagrange(getDelayAt(m_lfoSample)); // the taps of this sample's delay (the vector keeps its size: no allocation)
        }
        ++m_lfoSample;
        // Exact: every channel's wet signal is read first, then every line written (the feedback crosses between the channels);
        // the author's order writes with the previous sample's wet signals, then reads
        if (m_settings.order == LoopOrder::Exact)
        {
            for (size_t channel = 0; channel < channels; ++channel)
            {
                m_channels[channel].wet = filter(channel, readDelayed(channel));
            }
        }
        for (size_t to = 0; to < channels; ++to)
        {
            double value = 0.0;
            for (size_t from = 0; from < channels; ++from)
            {
                double fed = m_channels[from].wet;
                if (m_settings.order == LoopOrder::AuthorsOrder)
                {
                    fed = m_channels[from].lastOutput;
                }
                value += getInputGain(to, from, stereo) * buffer.getSample(static_cast<int>(from), sample) + getFeedbackGain(to, from, stereo) * fed;
            }
            m_channels[to].line[m_writePosition] = saturate(value);
        }
        if (m_settings.order == LoopOrder::AuthorsOrder)
        {
            for (size_t channel = 0; channel < channels; ++channel)
            {
                m_channels[channel].wet = filter(channel, readDelayed(channel));
                m_channels[channel].lastOutput = m_channels[channel].wet;
            }
        }
        for (size_t channel = 0; channel < channels; ++channel)
        {
            const double input = buffer.getSample(static_cast<int>(channel), sample);
            buffer.setSample(static_cast<int>(channel), sample, static_cast<float>((1.0 - m_settings.mix) * input + m_settings.mix * m_channels[channel].wet));
        }
        m_writePosition = (m_writePosition + 1) % m_size;
    }
}
}
