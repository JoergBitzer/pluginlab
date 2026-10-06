#include "pluginlab/reference/StateVariableFilter.h"

#include "pluginlab/reference/Designs.h"

#include "ReferenceMath.h"

namespace pluginlab::reference
{
namespace
{
constexpr double kShelfDecibelsPerDecade = 40.0;
}

StateVariableFilter::StateVariableFilter(FilterType type, double sampleRate, double frequencyHz, double gainDb, double q)
    : LinearProcessor(sampleRate)
    , m_type(type)
    , m_frequencyHz(frequencyHz)
    , m_gainDb(gainDb)
    , m_q(q)
{
    setParameters(type, frequencyHz, gainDb, q);
    setNumChannels(1);
}

void StateVariableFilter::setParameters(FilterType type, double frequencyHz, double gainDb, double q)
{
    m_type = type;
    m_frequencyHz = frequencyHz;
    m_gainDb = gainDb;
    m_q = q;

    const double a = std::pow(10.0, gainDb / kShelfDecibelsPerDecade);
    double g = std::tan(detail::kPi * frequencyHz / getSampleRate());
    double k = 1.0 / q;
    m_m0 = 0.0;
    m_m1 = 0.0;
    m_m2 = 0.0;
    switch (type)
    {
        case FilterType::LowPass:
            m_m2 = 1.0;
            break;
        case FilterType::HighPass:
            m_m0 = 1.0;
            m_m1 = -k;
            m_m2 = -1.0;
            break;
        case FilterType::BandPass:
            m_m1 = 1.0;
            break;
        case FilterType::BandPassUnity:
            m_m1 = k;
            break;
        case FilterType::Notch:
            m_m0 = 1.0;
            m_m1 = -k;
            break;
        case FilterType::AllPass:
            m_m0 = 1.0;
            m_m1 = -2.0 * k;
            break;
        case FilterType::Peak:
            k = 1.0 / (q * a);
            m_m0 = 1.0;
            m_m1 = k * (a * a - 1.0);
            break;
        case FilterType::LowShelf:
            g = g / std::sqrt(a);
            m_m0 = 1.0;
            m_m1 = k * (a - 1.0);
            m_m2 = a * a - 1.0;
            break;
        case FilterType::HighShelf:
            g = g * std::sqrt(a);
            m_m0 = a * a;
            m_m1 = k * (1.0 - a) * a;
            m_m2 = 1.0 - a * a;
            break;
    }
    m_a1 = 1.0 / (1.0 + g * (g + k));
    m_a2 = g * m_a1;
    m_a3 = g * m_a2;
}

void StateVariableFilter::reset()
{
    prepareChannels(static_cast<int>(m_state.size()));
}

void StateVariableFilter::prepareChannels(int channels)
{
    m_state.assign(static_cast<size_t>(channels), IntegratorState());
}

double StateVariableFilter::processSample(int channel, double input)
{
    IntegratorState& state = m_state[static_cast<size_t>(channel)];
    const double v3 = input - state.ic2eq;
    const double v1 = m_a1 * state.ic1eq + m_a2 * v3;
    const double v2 = state.ic2eq + m_a2 * state.ic1eq + m_a3 * v3;
    state.ic1eq = 2.0 * v1 - state.ic1eq;
    state.ic2eq = 2.0 * v2 - state.ic2eq;
    return m_m0 * input + m_m1 * v1 + m_m2 * v2;
}

std::complex<double> StateVariableFilter::getResponse(double frequencyHz) const
{
    return getBiquadResponse(designRbj(m_type, getSampleRate(), m_frequencyHz, m_gainDb, m_q), frequencyHz, getSampleRate());
}
}
