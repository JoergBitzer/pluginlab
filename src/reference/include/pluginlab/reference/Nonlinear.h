#pragma once

#include <functional>
#include <vector>

#include "pluginlab/reference/Processor.h"

namespace pluginlab::reference
{
// A memoryless curve y = f(x), the same on every channel: polynomial, hard clipper, tanh soft clipper.
// The answer: the harmonics of a sine through it (closed form for polynomial and hard clipper, numerical for any curve).
class Waveshaper : public Processor
{
public:
    // y = c0 + c1 x + c2 x^2 + ... (coefficients from c0)
    static Waveshaper makePolynomial(std::vector<double> coefficients);
    // y = x limited to +-threshold
    static Waveshaper makeHardClip(double threshold);
    // y = tanh(drive x) (small-signal gain = drive, limit +-1)
    static Waveshaper makeSoftClip(double drive);

    double processSample(double input) const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    explicit Waveshaper(std::function<double(double)> curve);

    std::function<double(double)> m_curve;
};

// The amplitudes of the harmonics 0 (DC, signed) ... count of A sin(wt) through the shaper, by numerical integration over one period
// (points samples; exact up to rounding for polynomials and smooth curves, converging with 1/points^2 for curves with kinks)
std::vector<double> getShaperHarmonics(const Waveshaper& shaper, double amplitude, int count, int points = 1 << 16);

// Closed form for the polynomial: x^n = (A sin)^n expands into harmonics n, n - 2, ... (cos^n = 2^-n sum C(n, j) cos((n - 2j) w t)),
// amplitudes of the harmonics 0 (DC, signed) ... count
std::vector<double> getPolynomialHarmonics(const std::vector<double>& coefficients, double amplitude, int count);

// Closed form for the hard clipper (Fourier series of the clipped sine; only odd harmonics), amplitudes 0 ... count;
// with the clipping angle a = asin(c / A): b1 = 4/pi (A (a/2 - sin(2a)/4) + c cos a),
// b_n = 4/pi (A/2 (sin((n-1)a)/(n-1) - sin((n+1)a)/(n+1)) + c cos(n a)/n) for odd n >= 3
std::vector<double> getHardClipHarmonics(double amplitude, double threshold, int count);

// Rounds to N bits (full scale +-1, step q = 2^(1-N), mid-tread, limited to -1 ... 1 - q), optionally with TPDF dither of +-1 q
// (the sum of two uniform values in +-q/2), independent per channel and seeded.
class Quantizer : public Processor
{
public:
    Quantizer(int bits, bool dither, int seed = 1);

    double getStep() const;

    // SNR of a sine of the given peak amplitude (1 = full scale) against the quantization noise q^2/12, plus the dither q^2/6:
    // 6.02 N + 1.76 dB + 20 log10(A), 4.77 dB less with dither (the noise power is three times as large)
    static double getExpectedSnrDb(int bits, bool dither, double amplitude);

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    int m_bits;
    bool m_dither;
    int m_seed;
    double m_step;
    juce::Random m_random;
};
}
