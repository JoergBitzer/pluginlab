#include "pluginlab/reference/Designs.h"

#include <algorithm>
#include <complex>

#include "ReferenceMath.h"

namespace pluginlab::reference
{
namespace
{
constexpr double kShelfDecibelsPerDecade = 40.0;
constexpr double kSquareRootOfTwo = 1.41421356237309504880;
constexpr double kNoGainDb = 1.0e-9; // below this the Orfanidis design is the identity (its formulas divide by G^2 - GB^2)

BiquadCoefficients invert(const BiquadCoefficients& section)
{
    return makeBiquad(1.0, section.a1, section.a2, section.b0, section.b1, section.b2);
}

// A boost is designed; a cut is the inverse of the boost with the opposite gain
bool isCut(double gainDb)
{
    return gainDb < 0.0;
}

BiquadCoefficients designZoelzerBoost(ZoelzerType type, double k, double v, double q)
{
    const double kSquared = k * k;
    switch (type)
    {
        case ZoelzerType::LowShelfFirstOrder:
        case ZoelzerType::HighShelfFirstOrder:
        {
            // H(z) = 1 + H0/2 (1 +- A(z)), A(z) = (z^-1 + c) / (1 + c z^-1), c = (K - 1) / (K + 1), H0 = V - 1
            const double c = (k - 1.0) / (k + 1.0);
            const double halfH0 = (v - 1.0) / 2.0;
            double sign = 1.0;
            if (type == ZoelzerType::HighShelfFirstOrder)
            {
                sign = -1.0;
            }
            return makeBiquad(1.0 + halfH0 * (1.0 + sign * c), c + halfH0 * (c + sign), 0.0, 1.0, c, 0.0);
        }
        case ZoelzerType::LowShelf:
        {
            const double root = std::sqrt(2.0 * v);
            return makeBiquad(1.0 + root * k + v * kSquared, 2.0 * (v * kSquared - 1.0), 1.0 - root * k + v * kSquared,
                              1.0 + kSquareRootOfTwo * k + kSquared, 2.0 * (kSquared - 1.0), 1.0 - kSquareRootOfTwo * k + kSquared);
        }
        case ZoelzerType::HighShelf:
        {
            const double root = std::sqrt(2.0 * v);
            return makeBiquad(v + root * k + kSquared, 2.0 * (kSquared - v), v - root * k + kSquared, 1.0 + kSquareRootOfTwo * k + kSquared,
                              2.0 * (kSquared - 1.0), 1.0 - kSquareRootOfTwo * k + kSquared);
        }
        case ZoelzerType::Peak:
            return makeBiquad(1.0 + v / q * k + kSquared, 2.0 * (kSquared - 1.0), 1.0 - v / q * k + kSquared, 1.0 + k / q + kSquared,
                              2.0 * (kSquared - 1.0), 1.0 - k / q + kSquared);
    }
    return BiquadCoefficients();
}

std::complex<double> getZoelzerAnalogBoost(ZoelzerType type, std::complex<double> s, double v, double q)
{
    const double root = std::sqrt(2.0 * v);
    switch (type)
    {
        case ZoelzerType::LowShelfFirstOrder:
            return (s + v) / (s + 1.0);
        case ZoelzerType::HighShelfFirstOrder:
            return (v * s + 1.0) / (s + 1.0);
        case ZoelzerType::LowShelf:
            return (s * s + root * s + v) / (s * s + kSquareRootOfTwo * s + 1.0);
        case ZoelzerType::HighShelf:
            return (v * s * s + root * s + 1.0) / (s * s + kSquareRootOfTwo * s + 1.0);
        case ZoelzerType::Peak:
            return (s * s + v / q * s + 1.0) / (s * s + s / q + 1.0);
    }
    return 1.0;
}

std::vector<double> getButterworthQs(int order)
{
    std::vector<double> qs;
    for (int section = 0; section < order / 2; ++section)
    {
        qs.push_back(1.0 / (2.0 * std::sin((2.0 * section + 1.0) * detail::kPi / (2.0 * order))));
    }
    return qs;
}

bool hasFirstOrderSection(int order)
{
    return order % 2 == 1;
}
}

BiquadCoefficients designRbj(FilterType type, double sampleRate, double frequencyHz, double gainDb, double q)
{
    const double a = std::pow(10.0, gainDb / kShelfDecibelsPerDecade);
    const double w0 = detail::kTwoPi * frequencyHz / sampleRate;
    const double cosW0 = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double shelfTerm = 2.0 * std::sqrt(a) * alpha;
    switch (type)
    {
        case FilterType::LowPass:
            return makeBiquad((1.0 - cosW0) / 2.0, 1.0 - cosW0, (1.0 - cosW0) / 2.0, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
        case FilterType::HighPass:
            return makeBiquad((1.0 + cosW0) / 2.0, -(1.0 + cosW0), (1.0 + cosW0) / 2.0, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
        case FilterType::BandPass:
            return makeBiquad(q * alpha, 0.0, -q * alpha, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
        case FilterType::BandPassUnity:
            return makeBiquad(alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
        case FilterType::Notch:
            return makeBiquad(1.0, -2.0 * cosW0, 1.0, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
        case FilterType::AllPass:
            return makeBiquad(1.0 - alpha, -2.0 * cosW0, 1.0 + alpha, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
        case FilterType::Peak:
            return makeBiquad(1.0 + alpha * a, -2.0 * cosW0, 1.0 - alpha * a, 1.0 + alpha / a, -2.0 * cosW0, 1.0 - alpha / a);
        case FilterType::LowShelf:
            return makeBiquad(a * ((a + 1.0) - (a - 1.0) * cosW0 + shelfTerm), 2.0 * a * ((a - 1.0) - (a + 1.0) * cosW0),
                              a * ((a + 1.0) - (a - 1.0) * cosW0 - shelfTerm), (a + 1.0) + (a - 1.0) * cosW0 + shelfTerm,
                              -2.0 * ((a - 1.0) + (a + 1.0) * cosW0), (a + 1.0) + (a - 1.0) * cosW0 - shelfTerm);
        case FilterType::HighShelf:
            return makeBiquad(a * ((a + 1.0) + (a - 1.0) * cosW0 + shelfTerm), -2.0 * a * ((a - 1.0) + (a + 1.0) * cosW0),
                              a * ((a + 1.0) + (a - 1.0) * cosW0 - shelfTerm), (a + 1.0) - (a - 1.0) * cosW0 + shelfTerm,
                              2.0 * ((a - 1.0) - (a + 1.0) * cosW0), (a + 1.0) - (a - 1.0) * cosW0 - shelfTerm);
    }
    return BiquadCoefficients();
}

double getQFromBandwidth(double octaves, double frequencyHz, double sampleRate)
{
    const double w0 = detail::kTwoPi * frequencyHz / sampleRate;
    return 1.0 / (2.0 * std::sinh(std::log(2.0) / 2.0 * octaves * w0 / std::sin(w0)));
}

double getQFromShelfSlope(double slope, double gainDb)
{
    const double a = std::pow(10.0, gainDb / kShelfDecibelsPerDecade);
    return 1.0 / std::sqrt((a + 1.0 / a) * (1.0 / slope - 1.0) + 2.0);
}

BiquadCoefficients designOrfanidisPeak(double sampleRate, double frequencyHz, double gainDb, double q)
{
    // The notation of the paper (and of Orfanidis' peq.m): reference gain G0 = 1, peak gain G, bandwidth gain GB (half the gain in dB),
    // w0 and the bandwidth Dw in radians per sample, G1 the gain of the analog prototype at Nyquist
    if (std::abs(gainDb) < kNoGainDb)
    {
        return BiquadCoefficients();
    }
    const double g0 = 1.0;
    const double g = detail::dbToGain(gainDb);
    const double gB = std::sqrt(g * g0);
    const double w0 = detail::kTwoPi * frequencyHz / sampleRate;
    const double dw = w0 / q;
    const double piSquared = detail::kPi * detail::kPi;

    const double f = std::abs(g * g - gB * gB);
    const double g00 = std::abs(g * g - g0 * g0);
    const double f00 = std::abs(gB * gB - g0 * g0);
    const double distance = (w0 * w0 - piSquared) * (w0 * w0 - piSquared);
    const double numerator = g0 * g0 * distance + g * g * f00 * piSquared * dw * dw / f;
    const double denominator = distance + f00 * piSquared * dw * dw / f;
    const double g1 = std::sqrt(numerator / denominator);

    const double g01 = std::abs(g * g - g0 * g1);
    const double g11 = std::abs(g * g - g1 * g1);
    const double f01 = std::abs(gB * gB - g0 * g1);
    const double f11 = std::abs(gB * gB - g1 * g1);
    const double tanHalfW0 = std::tan(w0 / 2.0);
    const double w2 = std::sqrt(g11 / g00) * tanHalfW0 * tanHalfW0;
    const double warpedWidth = (1.0 + std::sqrt(f00 / f11) * w2) * std::tan(dw / 2.0);
    const double c = f11 * warpedWidth * warpedWidth - 2.0 * w2 * (f01 - std::sqrt(f00 * f11));
    const double d = 2.0 * w2 * (g01 - std::sqrt(g00 * g11));
    const double a = std::sqrt((c + d) / f);
    const double b = std::sqrt((g * g * c + gB * gB * d) / f);
    return makeBiquad(g1 + g0 * w2 + b, -2.0 * (g1 - g0 * w2), g1 - b + g0 * w2, 1.0 + w2 + a, -2.0 * (1.0 - w2), 1.0 + w2 - a);
}

const char* getZoelzerTypeName(ZoelzerType type)
{
    switch (type)
    {
        case ZoelzerType::LowShelfFirstOrder:
            return "low shelf 1st order";
        case ZoelzerType::HighShelfFirstOrder:
            return "high shelf 1st order";
        case ZoelzerType::LowShelf:
            return "low shelf";
        case ZoelzerType::HighShelf:
            return "high shelf";
        case ZoelzerType::Peak:
            return "peak";
    }
    return "";
}

BiquadCoefficients designZoelzer(ZoelzerType type, double sampleRate, double frequencyHz, double gainDb, double q)
{
    const double k = std::tan(detail::kPi * frequencyHz / sampleRate);
    const BiquadCoefficients boost = designZoelzerBoost(type, k, detail::dbToGain(std::abs(gainDb)), q);
    if (isCut(gainDb))
    {
        return invert(boost);
    }
    return boost;
}

std::complex<double> getZoelzerAnalogResponse(ZoelzerType type, double frequencyHz, double cornerHz, double gainDb, double q)
{
    const std::complex<double> s(0.0, frequencyHz / cornerHz);
    const std::complex<double> boost = getZoelzerAnalogBoost(type, s, detail::dbToGain(std::abs(gainDb)), q);
    if (isCut(gainDb))
    {
        return 1.0 / boost;
    }
    return boost;
}

std::vector<BiquadCoefficients> designButterworthBandPass(int order, double sampleRate, double lowEdgeHz, double highEdgeHz)
{
    const double twoFs = 2.0 * sampleRate;
    const double w1 = twoFs * std::tan(detail::kPi * lowEdgeHz / sampleRate);
    const double w2 = twoFs * std::tan(detail::kPi * highEdgeHz / sampleRate);
    const double w0Squared = w1 * w2;
    const double bandwidth = w2 - w1;
    // all 2 x order poles of the band-pass (two per prototype pole), digital by the bilinear transform
    std::vector<std::complex<double>> complexPoles;
    std::vector<double> realPoles;
    for (int k = 0; k < order; ++k)
    {
        const std::complex<double> prototype = std::polar(1.0, detail::kPi * (2.0 * k + order + 1.0) / (2.0 * order));
        const std::complex<double> scaled = prototype * bandwidth;
        const std::complex<double> root = std::sqrt(scaled * scaled - 4.0 * w0Squared);
        for (const std::complex<double> pole : {(scaled + root) / 2.0, (scaled - root) / 2.0})
        {
            const std::complex<double> z = (twoFs + pole) / (twoFs - pole);
            constexpr double kReal = 1.0e-12;
            if (std::abs(z.imag()) <= kReal)
            {
                realPoles.push_back(z.real()); // a wide band near Nyquist: the real prototype pole gives two real poles
            }
            else if (z.imag() > 0.0)
            {
                complexPoles.push_back(z);    // its conjugate comes from the conjugate prototype pole
            }
        }
    }
    // one section per conjugate pair, and one per two real poles; each with a zero at z = 1 and one at z = -1
    std::vector<BiquadCoefficients> sections;
    for (const std::complex<double>& z : complexPoles)
    {
        sections.push_back(makeBiquad(1.0, 0.0, -1.0, 1.0, -2.0 * z.real(), std::norm(z)));
    }
    std::sort(realPoles.begin(), realPoles.end());
    for (size_t index = 0; index + 1 < realPoles.size(); index += 2)
    {
        const double p1 = realPoles[index];
        const double p2 = realPoles[index + 1];
        sections.push_back(makeBiquad(1.0, 0.0, -1.0, 1.0, -(p1 + p2), p1 * p2));
    }
    // 0 dB at the digital image of w0, shared by the sections
    const double centreHz = sampleRate / detail::kPi * std::atan(std::sqrt(w0Squared) / twoFs);
    const double gain = std::abs(getCascadeResponse(sections, centreHz, sampleRate));
    const double perSection = std::pow(gain, -1.0 / static_cast<double>(sections.size()));
    for (BiquadCoefficients& section : sections)
    {
        section.b0 *= perSection;
        section.b1 *= perSection;
        section.b2 *= perSection;
    }
    return sections;
}

std::vector<BiquadCoefficients> designButterworth(Pass pass, int order, double sampleRate, double cornerHz)
{
    std::vector<BiquadCoefficients> sections;
    FilterType type = FilterType::LowPass;
    if (pass == Pass::High)
    {
        type = FilterType::HighPass;
    }
    for (const double q : getButterworthQs(order))
    {
        sections.push_back(designRbj(type, sampleRate, cornerHz, 0.0, q));
    }
    if (hasFirstOrderSection(order))
    {
        // the bilinear transform of 1 / (s + 1) and s / (s + 1), K = tan(pi fc / fs)
        const double k = std::tan(detail::kPi * cornerHz / sampleRate);
        if (pass == Pass::Low)
        {
            sections.push_back(makeBiquad(k, k, 0.0, 1.0 + k, k - 1.0, 0.0));
        }
        else
        {
            sections.push_back(makeBiquad(1.0, -1.0, 0.0, 1.0 + k, k - 1.0, 0.0));
        }
    }
    return sections;
}

std::complex<double> getAnalogButterworthResponse(Pass pass, int order, double frequencyHz, double cornerHz)
{
    const std::complex<double> s(0.0, frequencyHz / cornerHz);
    std::complex<double> numerator = 1.0;
    if (pass == Pass::High)
    {
        numerator = s;
    }
    std::complex<double> response = 1.0;
    for (const double q : getButterworthQs(order))
    {
        response *= numerator * numerator / (s * s + s / q + 1.0);
    }
    if (hasFirstOrderSection(order))
    {
        response *= numerator / (s + 1.0);
    }
    return response;
}

std::vector<BiquadCoefficients> designLinkwitzRiley(Pass pass, int order, double sampleRate, double cornerHz)
{
    std::vector<BiquadCoefficients> sections = designButterworth(pass, order / 2, sampleRate, cornerHz);
    const std::vector<BiquadCoefficients> half = sections;
    sections.insert(sections.end(), half.begin(), half.end());
    return sections;
}

std::complex<double> getAnalogLinkwitzRileyResponse(Pass pass, int order, double frequencyHz, double cornerHz)
{
    const std::complex<double> half = getAnalogButterworthResponse(pass, order / 2, frequencyHz, cornerHz);
    return half * half;
}
}
