#include "pluginlab/measure/Analyzer.h"

#include <algorithm>
#include <cmath>

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kSquareRootOfTwo = 1.41421356237309504880;
constexpr double kStopBandStartHz = 24000.0;     // AES17 5.2.5
constexpr double kDesignAttenuationDb = 70.0;    // 10 dB more than the 60 dB AES17 asks for
constexpr double kKaiserOffsetDb = 8.0;          // Kaiser's estimate of the length: N = (A - 8) / (2.285 dw)
constexpr double kKaiserWidthFactor = 2.285;
constexpr double kKaiserBetaSlope = 0.1102;      // beta = 0.1102 (A - 8.7) for A > 50 dB
constexpr double kKaiserBetaOffsetDb = 8.7;
constexpr int kBesselTerms = 40;

// The modified Bessel function of the first kind, order 0 (series)
double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < kBesselTerms; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}
}

double rmsToDbfs(double rms)
{
    return 20.0 * std::log10(std::max(rms * kSquareRootOfTwo, 1.0e-30));
}

double dbfsToRms(double dbfs)
{
    return std::pow(10.0, dbfs / 20.0) / kSquareRootOfTwo;
}

StandardLowPass::StandardLowPass(double sampleRate)
    : m_sampleRate(sampleRate)
{
    if (sampleRate / 2.0 <= kStopBandStartHz)
    {
        return; // nothing above 24 kHz can exist: the identity meets the specification
    }
    // Kaiser window design (Kaiser 1974; Oppenheim and Schafer, Discrete-Time Signal Processing, ch. 7): cutoff in the middle of the transition band
    const double transition = 2.0 * kPi * (kStopBandStartHz - kUpperBandEdgeHz) / sampleRate;
    int taps = static_cast<int>(std::ceil((kDesignAttenuationDb - kKaiserOffsetDb) / (kKaiserWidthFactor * transition))) + 1;
    if (taps % 2 == 0)
    {
        ++taps;
    }
    const double beta = kKaiserBetaSlope * (kDesignAttenuationDb - kKaiserBetaOffsetDb);
    const double cutoff = 2.0 * kPi * (kUpperBandEdgeHz + kStopBandStartHz) / 2.0 / sampleRate;
    const int centre = (taps - 1) / 2;
    m_taps.resize(static_cast<size_t>(taps));
    double sum = 0.0;
    for (int tap = 0; tap < taps; ++tap)
    {
        const int m = tap - centre;
        double ideal = cutoff / kPi;
        if (m != 0)
        {
            ideal = std::sin(cutoff * m) / (kPi * m);
        }
        const double ratio = static_cast<double>(m) / centre;
        const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / besselI0(beta);
        m_taps[static_cast<size_t>(tap)] = ideal * window;
        sum += ideal * window;
    }
    for (double& tap : m_taps)
    {
        tap /= sum; // exactly 0 dB at DC
    }
}

bool StandardLowPass::isIdentity() const
{
    return m_taps.empty();
}

int StandardLowPass::getTaps() const
{
    return static_cast<int>(m_taps.size());
}

std::complex<double> StandardLowPass::getResponse(double frequencyHz) const
{
    if (isIdentity())
    {
        return 1.0;
    }
    std::complex<double> sum = 0.0;
    for (size_t tap = 0; tap < m_taps.size(); ++tap)
    {
        sum += m_taps[tap] * std::polar(1.0, -2.0 * kPi * frequencyHz * static_cast<double>(tap) / m_sampleRate);
    }
    return sum;
}

std::vector<double> StandardLowPass::process(const float* data, int length) const
{
    std::vector<double> output(static_cast<size_t>(length));
    if (isIdentity())
    {
        for (int index = 0; index < length; ++index)
        {
            output[static_cast<size_t>(index)] = data[index];
        }
        return output;
    }
    const int taps = getTaps();
    for (int index = 0; index < length; ++index)
    {
        double sum = 0.0;
        const int first = std::max(0, index - taps + 1);
        for (int source = first; source <= index; ++source)
        {
            sum += m_taps[static_cast<size_t>(index - source)] * data[source];
        }
        output[static_cast<size_t>(index)] = sum;
    }
    return output;
}

double getRms(const std::vector<double>& data, int start, int length)
{
    double sum = 0.0;
    for (int index = start; index < start + length; ++index)
    {
        sum += data[static_cast<size_t>(index)] * data[static_cast<size_t>(index)];
    }
    return std::sqrt(sum / std::max(1, length));
}

std::complex<double> getToneAmplitude(const std::vector<double>& data, int start, int length, double frequencyHz, double sampleRate)
{
    std::complex<double> sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        sum += data[static_cast<size_t>(start + index)] * std::polar(1.0, -2.0 * kPi * frequencyHz * index / sampleRate);
    }
    return 2.0 * sum / static_cast<double>(std::max(1, length));
}

int getWholePeriodLength(double frequencyHz, double sampleRate, double seconds)
{
    const double wanted = std::max(seconds, kMinimumIntegrationSeconds);
    const double periods = std::ceil(wanted * frequencyHz);
    return static_cast<int>(std::round(periods * sampleRate / frequencyHz));
}
}
