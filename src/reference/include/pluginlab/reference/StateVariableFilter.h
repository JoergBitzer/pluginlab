#pragma once

#include <vector>

#include "pluginlab/reference/Analog.h"
#include "pluginlab/reference/LinearProcessor.h"

namespace pluginlab::reference
{
// The state-variable filter with trapezoidal integrators (TPT / zero-delay feedback, Zavalishin; the form of A. Simper, Cytomic,
// "SvfLinearTrapOptimised2"): g = tan(pi fc / fs), k = 1/Q, one structure for all types (the output is m0 v0 + m1 v1 + m2 v2).
// Its transfer function is that of the RBJ design of the same type (bilinear transform of the same analog prototype); unlike a direct form
// it stays well behaved when the parameters change while it runs.
class StateVariableFilter : public LinearProcessor
{
public:
    StateVariableFilter(FilterType type, double sampleRate, double frequencyHz, double gainDb, double q);

    // Any time, also while processing (the state is kept)
    void setParameters(FilterType type, double frequencyHz, double gainDb, double q);

    void reset() override;
    double processSample(int channel, double input) override;
    std::complex<double> getResponse(double frequencyHz) const override;

protected:
    void prepareChannels(int channels) override;

private:
    struct IntegratorState
    {
        double ic1eq = 0.0;
        double ic2eq = 0.0;
    };

    FilterType m_type;
    double m_frequencyHz;
    double m_gainDb;
    double m_q;
    double m_a1 = 0.0;
    double m_a2 = 0.0;
    double m_a3 = 0.0;
    double m_m0 = 0.0;
    double m_m1 = 0.0;
    double m_m2 = 0.0;
    std::vector<IntegratorState> m_state;
};
}
