#include <cmath>
#include <complex>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace sig = pluginlab::signals;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
// A sine whose aliases do not fall on its harmonics: fs / f0 = 4755 / 101, so the alias of harmonic k around n fs lies at (n 4755/101 - k) f0, which
// is a harmonic only for n = 101, 202, ..., i.e. for harmonics near 4755 (about -130 dB even for the hard clipper, whose harmonics fall with 1/k^2).
// With a simpler ratio (612/13) the hard clipper's harmonics near 600 folded onto the low ones by 0.004 dB: aliasing of a sampled clipper.
// A window of 47550 samples holds exactly 1010 periods (every harmonic and every alias on a bin, no leakage).
constexpr double kSamplesPerPeriod = 4755.0 / 101.0;
constexpr int kWindow = 47550;
constexpr double kFrequency = kSampleRate / kSamplesPerPeriod;

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-300));
}

// The amplitude at a frequency in a window (one DFT bin, exact for a whole number of periods)
double amplitudeAt(const float* data, int length, double frequency)
{
    std::complex<double> sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        sum += static_cast<double>(data[index]) * std::polar(1.0, -2.0 * kPi * frequency * index / kSampleRate);
    }
    return 2.0 * std::abs(sum) / length;
}

double meanOf(const float* data, int length)
{
    double sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        sum += data[index];
    }
    return sum / length;
}

juce::AudioBuffer<float> makeSine(double amplitude, int channels)
{
    juce::AudioBuffer<float> buffer(channels, kWindow);
    for (int index = 0; index < kWindow; ++index)
    {
        const float value = static_cast<float>(amplitude * std::sin(2.0 * kPi * index / kSamplesPerPeriod));
        for (int channel = 0; channel < channels; ++channel)
        {
            buffer.setSample(channel, index, value);
        }
    }
    return buffer;
}

juce::AudioBuffer<float> makeNoiseBuffer(int length, int seed)
{
    sig::NoiseSettings settings;
    settings.length = length;
    settings.seed = seed;
    return sig::makeNoise(settings, 2, sig::ChannelRelation::Uncorrelated);
}
}

// The nonlinear and utility reference processors of W6.3, each against its known answer
class NonlinearReferenceTests : public juce::UnitTest
{
public:
    NonlinearReferenceTests()
        : juce::UnitTest("Nonlinear reference processors", "pluginlab")
    {
    }

    void runTest() override
    {
        testPolynomial();
        testHardClip();
        testSoftClip();
        testQuantizer();
        testGainAndMatrix();
        testDcHumNoise();
        testTremolo();
    }

private:
    // The harmonics of the processed sine against the expected ones: those above -80 dB re the fundamental within the tolerance; those expected
    // below -120 dB must stay below -100 dB; in between the float rounding of the samples (a periodic error, it lands on the harmonics) is
    // as large as the harmonic, so they are not compared
    void expectHarmonics(ref::Waveshaper& shaper, double amplitude, const std::vector<double>& expected, double toleranceDb, const juce::String& what)
    {
        constexpr double kCompared = 1.0e-4;
        constexpr double kAbsent = 1.0e-6;
        constexpr double kAbsentLimit = 1.0e-5;
        juce::AudioBuffer<float> buffer = makeSine(amplitude, 1);
        shaper.process(buffer);
        const double fundamental = expected[1];
        double worst = 0.0;
        for (size_t harmonic = 1; harmonic < expected.size(); ++harmonic)
        {
            const double measured = amplitudeAt(buffer.getReadPointer(0), kWindow, kFrequency * static_cast<double>(harmonic));
            if (expected[harmonic] > fundamental * kCompared)
            {
                worst = std::max(worst, std::abs(toDb(measured) - toDb(expected[harmonic])));
            }
            if (expected[harmonic] < fundamental * kAbsent)
            {
                expect(measured < fundamental * kAbsentLimit, what + ": harmonic " + juce::String(static_cast<int>(harmonic)) + " should be absent, "
                                                                  + juce::String(toDb(measured / fundamental), 1) + " dB");
            }
        }
        expect(worst < toleranceDb, what + ": largest difference " + juce::String(worst, 5) + " dB");
        expectWithinAbsoluteError(meanOf(buffer.getReadPointer(0), kWindow), expected[0], 1.0e-6, what + ": DC");
    }

    void testPolynomial()
    {
        beginTest("polynomial: the closed-form harmonics equal the numerical ones and the processed sine");
        const std::vector<double> coefficients = {0.0, 1.0, 0.1, 0.05, 0.0, 0.02};
        ref::Waveshaper shaper = ref::Waveshaper::makePolynomial(coefficients);
        for (const double amplitude : {0.1, 0.5, 0.9})
        {
            const std::vector<double> closed = ref::getPolynomialHarmonics(coefficients, amplitude, 7);
            const std::vector<double> numeric = ref::getShaperHarmonics(shaper, amplitude, 7);
            double difference = 0.0;
            for (size_t harmonic = 0; harmonic < closed.size(); ++harmonic)
            {
                difference = std::max(difference, std::abs(closed[harmonic] - numeric[harmonic]));
            }
            expect(difference < 1.0e-12, "closed form = numerical, A = " + juce::String(amplitude) + ": " + juce::String(difference));
            expectHarmonics(shaper, amplitude, closed, 0.001, "polynomial A = " + juce::String(amplitude));
            if (amplitude > 0.4 && amplitude < 0.6)
            {
                logMessage("polynomial at A = 0.5: H2 " + juce::String(toDb(closed[2] / closed[1]), 2) + " dB, H3 " + juce::String(toDb(closed[3] / closed[1]), 2)
                           + " dB, H5 " + juce::String(toDb(closed[5] / closed[1]), 2) + " dB re H1, DC " + juce::String(closed[0], 6));
            }
        }
        // the textbook values: x + a2 x^2 gives H2/H1 = a2 A / 2 (for a2 A small: the x^3 term changes H1 a little)
        const std::vector<double> square = ref::getPolynomialHarmonics({0.0, 1.0, 0.1}, 0.5, 3);
        expectWithinAbsoluteError(square[2] / square[1], 0.1 * 0.5 / 2.0, 1.0e-12);
        expectWithinAbsoluteError(square[0], 0.1 * 0.25 / 2.0, 1.0e-12);
    }

    void testHardClip()
    {
        beginTest("hard clipper: the Fourier series of the clipped sine equals the numerical harmonics and the processed sine (aliases between the harmonics)");
        constexpr double kThreshold = 0.5;
        ref::Waveshaper shaper = ref::Waveshaper::makeHardClip(kThreshold);
        for (const double amplitude : {0.4, 0.6, 1.0, 2.0})
        {
            const std::vector<double> closed = ref::getHardClipHarmonics(amplitude, kThreshold, 15);
            const std::vector<double> numeric = ref::getShaperHarmonics(shaper, amplitude, 15, 1 << 18);
            double difference = 0.0;
            for (size_t harmonic = 0; harmonic < closed.size(); ++harmonic)
            {
                difference = std::max(difference, std::abs(closed[harmonic] - numeric[harmonic]));
            }
            expect(difference < 1.0e-9, "closed form = numerical, A = " + juce::String(amplitude) + ": " + juce::String(difference));
            // the processed sine: the harmonics above 23 alias back, but between the harmonics; only the low harmonics are compared tightly
            std::vector<double> low(closed.begin(), closed.begin() + 8);
            expectHarmonics(shaper, amplitude, low, 0.001, "hard clipper A = " + juce::String(amplitude));
            if (amplitude > 1.5)
            {
                logMessage("hard clipper at 6 dB over the threshold: H3 " + juce::String(toDb(closed[3] / closed[1]), 2) + " dB, H5 "
                           + juce::String(toDb(closed[5] / closed[1]), 2) + " dB re H1, H1 " + juce::String(toDb(closed[1] / amplitude), 2) + " dB re the input");
            }
        }
        // below the threshold: no distortion
        const std::vector<double> clean = ref::getHardClipHarmonics(0.4, kThreshold, 5);
        expectEquals(clean[1], 0.4);
        expectEquals(clean[3], 0.0);
    }

    void testSoftClip()
    {
        beginTest("tanh soft clipper: odd symmetry (no even harmonics), small-signal gain = drive, THD rises with the level, processed = numerical");
        constexpr double kDrive = 4.0;
        ref::Waveshaper shaper = ref::Waveshaper::makeSoftClip(kDrive);
        const std::vector<double> small = ref::getShaperHarmonics(shaper, 1.0e-4, 3);
        expectWithinAbsoluteError(toDb(small[1] / 1.0e-4), toDb(kDrive), 1.0e-4);
        double previousThd = 0.0;
        for (const double amplitude : {0.05, 0.2, 0.5, 1.0})
        {
            const std::vector<double> numeric = ref::getShaperHarmonics(shaper, amplitude, 15);
            double distortion = 0.0;
            for (size_t harmonic = 2; harmonic < numeric.size(); ++harmonic)
            {
                distortion += numeric[harmonic] * numeric[harmonic];
                if (harmonic % 2 == 0)
                {
                    expect(numeric[harmonic] < 1.0e-12, "no even harmonic");
                }
            }
            const double thd = std::sqrt(distortion) / numeric[1];
            logMessage("tanh(4 x) at A = " + juce::String(amplitude) + ": THD " + juce::String(100.0 * thd, 3) + " %");
            expect(thd > previousThd, "THD rises with the level");
            previousThd = thd;
            std::vector<double> low(numeric.begin(), numeric.begin() + 8);
            expectHarmonics(shaper, amplitude, low, 0.001, "tanh A = " + juce::String(amplitude));
        }
    }

    void testQuantizer()
    {
        beginTest("quantizer: SNR = 6.02 N + 1.76 dB (+ 20 log A), 4.77 dB less with TPDF dither; silence stays silent without dither");
        constexpr int kLength = 1 << 18;
        for (const int bits : {8, 12, 16})
        {
            for (const bool dither : {false, true})
            {
                ref::Quantizer quantizer(bits, dither);
                const double amplitude = 1.0 - 4.0 * quantizer.getStep();
                juce::AudioBuffer<float> buffer(1, kLength);
                for (int index = 0; index < kLength; ++index)
                {
                    // 997 Hz: not related to the sample rate, the error looks like noise
                    buffer.setSample(0, index, static_cast<float>(amplitude * std::sin(2.0 * kPi * 997.0 * index / kSampleRate)));
                }
                juce::AudioBuffer<float> input;
                input.makeCopyOf(buffer);
                quantizer.process(buffer);
                double errorPower = 0.0;
                for (int index = 0; index < kLength; ++index)
                {
                    const double error = static_cast<double>(buffer.getSample(0, index)) - input.getSample(0, index);
                    errorPower += error * error;
                }
                errorPower /= kLength;
                const double snr = 10.0 * std::log10(amplitude * amplitude / 2.0 / errorPower);
                const double expected = ref::Quantizer::getExpectedSnrDb(bits, dither, amplitude);
                logMessage(juce::String(bits) + " bits, dither " + juce::String(static_cast<int>(dither)) + ": SNR "
                           + juce::String(snr, 2) + " dB, expected " + juce::String(expected, 2) + " dB");
                expectWithinAbsoluteError(snr, expected, 0.2, juce::String(bits) + " bits");
            }
        }
        ref::Quantizer plain(8, false);
        juce::AudioBuffer<float> silence(1, 1000);
        silence.clear();
        plain.process(silence);
        expectEquals(silence.getMagnitude(0, 0, 1000), 0.0f);

        beginTest("quantizer: a sine of 2 steps peak at 8 bits has strong harmonics without dither, none above the noise with TPDF dither");
        for (const bool dither : {false, true})
        {
            ref::Quantizer quantizer(8, dither);
            juce::AudioBuffer<float> buffer = makeSine(2.0 * quantizer.getStep(), 1);
            quantizer.process(buffer);
            const double fundamental = amplitudeAt(buffer.getReadPointer(0), kWindow, kFrequency);
            const double third = amplitudeAt(buffer.getReadPointer(0), kWindow, 3.0 * kFrequency);
            logMessage(juce::String("8 bits, sine of 2 steps, dither ") + juce::String(static_cast<int>(dither)) + ": H3 " + juce::String(toDb(third / fundamental), 1)
                       + " dB re H1");
            if (dither)
            {
                // one DFT bin of the dither noise lies near -53 dB re the fundamental here (and scatters by several dB)
                expect(toDb(third / fundamental) < -45.0);
            }
            else
            {
                expect(toDb(third / fundamental) > -40.0);
            }
        }
    }

    void testGainAndMatrix()
    {
        beginTest("gain and polarity, channel matrix (crosstalk, width): exact");
        juce::AudioBuffer<float> noise = makeNoiseBuffer(1000, 3);
        juce::AudioBuffer<float> buffer;
        buffer.makeCopyOf(noise);
        ref::Gain gain(-6.0, true);
        gain.process(buffer);
        expectWithinAbsoluteError(gain.getFactor(), -std::pow(10.0, -6.0 / 20.0), 1.0e-15);
        expectWithinAbsoluteError(buffer.getSample(1, 500), static_cast<float>(gain.getFactor() * noise.getSample(1, 500)), 1.0e-7f);

        ref::ChannelMatrix crosstalk = ref::ChannelMatrix::makeCrosstalk(-40.0);
        juce::AudioBuffer<float> leftOnly(2, 1000);
        leftOnly.clear();
        leftOnly.copyFrom(0, 0, noise, 0, 0, 1000);
        crosstalk.process(leftOnly);
        const double crosstalkDb = toDb(sig::getRms(leftOnly, 1) / sig::getRms(leftOnly, 0));
        expectWithinAbsoluteError(crosstalkDb, -40.0, 1.0e-4);

        for (const double width : {0.0, 1.0, 2.0})
        {
            ref::ChannelMatrix matrix = ref::ChannelMatrix::makeWidth(width);
            buffer.makeCopyOf(noise);
            matrix.process(buffer);
            double largest = 0.0;
            for (int index = 0; index < 1000; ++index)
            {
                const double mid = (noise.getSample(0, index) + noise.getSample(1, index)) / 2.0;
                const double side = width * (noise.getSample(0, index) - noise.getSample(1, index)) / 2.0;
                largest = std::max(largest, std::abs(buffer.getSample(0, index) - (mid + side)));
                largest = std::max(largest, std::abs(buffer.getSample(1, index) - (mid - side)));
            }
            expect(largest < 1.0e-6, "width " + juce::String(width) + ": " + juce::String(largest));
        }
        juce::AudioBuffer<float> mono(1, 10);
        mono.clear();
        mono.setSample(0, 3, 0.5f);
        ref::ChannelMatrix::makeWidth(0.0).process(mono);
        expectEquals(mono.getSample(0, 3), 0.5f);
    }

    void testDcHumNoise()
    {
        beginTest("DC offset, hum (50 Hz plus harmonics at their levels, continuous across blocks), noise adder (level, uncorrelated, seeded)");
        juce::AudioBuffer<float> buffer(2, 48000);
        buffer.clear();
        ref::DcOffset offset(0.01);
        offset.process(buffer);
        expectWithinAbsoluteError(meanOf(buffer.getReadPointer(1), 48000), 0.01, 1.0e-7);

        buffer.clear();
        ref::HumAdder hum(kSampleRate, 50.0, -40.0, {-6.0, -12.0});
        juce::AudioBuffer<float> block(2, 480);
        for (int start = 0; start < 48000; start += 480)
        {
            block.clear();
            hum.process(block);
            buffer.copyFrom(0, start, block, 0, 0, 480);
            buffer.copyFrom(1, start, block, 1, 0, 480);
        }
        expectWithinAbsoluteError(toDb(amplitudeAt(buffer.getReadPointer(0), 48000, 50.0)), -40.0, 1.0e-4);
        expectWithinAbsoluteError(toDb(amplitudeAt(buffer.getReadPointer(0), 48000, 100.0)), -46.0, 1.0e-4);
        expectWithinAbsoluteError(toDb(amplitudeAt(buffer.getReadPointer(1), 48000, 150.0)), -52.0, 1.0e-4);
        expectWithinAbsoluteError(static_cast<double>(buffer.getSample(0, 12345)), hum.getHum(12345), 1.0e-7);

        for (const sig::NoiseColour colour : {sig::NoiseColour::WhiteUniform, sig::NoiseColour::WhiteGaussian, sig::NoiseColour::Pink})
        {
            buffer.setSize(2, 480000);
            buffer.clear();
            ref::NoiseAdder noise(colour, -30.0, 11);
            noise.process(buffer);
            const double left = toDb(sig::getRms(buffer, 0));
            const double right = toDb(sig::getRms(buffer, 1));
            double correlation = 0.0;
            for (int index = 0; index < buffer.getNumSamples(); ++index)
            {
                correlation += static_cast<double>(buffer.getSample(0, index)) * buffer.getSample(1, index);
            }
            correlation /= buffer.getNumSamples() * sig::getRms(buffer, 0) * sig::getRms(buffer, 1);
            logMessage("noise adder " + juce::String(static_cast<int>(colour)) + " at -30 dBFS RMS: " + juce::String(left, 2) + " / " + juce::String(right, 2)
                       + " dBFS, correlation " + juce::String(correlation, 3));
            // pink noise has few independent values at the lowest frequencies (the slowest row changes every 2^15 samples, 15 times in 10 s):
            // its level and correlation scatter more
            double levelTolerance = 0.1;
            double correlationTolerance = 0.01;
            if (colour == sig::NoiseColour::Pink)
            {
                levelTolerance = 0.5;
                correlationTolerance = 0.15;
            }
            expectWithinAbsoluteError(left, -30.0, levelTolerance);
            expectWithinAbsoluteError(right, -30.0, levelTolerance);
            expect(std::abs(correlation) < correlationTolerance, "uncorrelated");
            const float sample = buffer.getSample(1, 777);
            buffer.clear();
            noise.reset();
            noise.process(buffer);
            expectEquals(buffer.getSample(1, 777), sample);
        }
    }

    void testTremolo()
    {
        beginTest("tremolo: y = g(n) x with the exact gain curve, between 1 - depth and 1, continuous across blocks, reset starts again");
        constexpr double kRate = 5.0;
        constexpr double kDepth = 0.6;
        ref::Tremolo tremolo(kSampleRate, kRate, kDepth);
        juce::AudioBuffer<float> noise = makeNoiseBuffer(48000, 5);
        juce::AudioBuffer<float> buffer;
        buffer.makeCopyOf(noise);
        juce::AudioBuffer<float> block(2, 512);
        int position = 0;
        while (position < buffer.getNumSamples())
        {
            const int length = std::min(512, buffer.getNumSamples() - position);
            for (int channel = 0; channel < 2; ++channel)
            {
                block.copyFrom(channel, 0, buffer, channel, position, length);
            }
            block.setSize(2, length, true, false, true);
            tremolo.process(block);
            for (int channel = 0; channel < 2; ++channel)
            {
                buffer.copyFrom(channel, position, block, channel, 0, length);
            }
            position += length;
            block.setSize(2, 512, false, false, true);
        }
        double largest = 0.0;
        for (int index = 0; index < 48000; ++index)
        {
            largest = std::max(largest, std::abs(buffer.getSample(0, index) - tremolo.getGain(index) * noise.getSample(0, index)));
        }
        expect(largest < 1.0e-6, juce::String(largest));
        expectWithinAbsoluteError(tremolo.getGain(0), 1.0, 1.0e-15);
        expectWithinAbsoluteError(tremolo.getGain(4800), 1.0 - kDepth, 1.0e-12); // half a period of 5 Hz
        expectWithinAbsoluteError(tremolo.getGain(9600), 1.0, 1.0e-12);
    }
};

static NonlinearReferenceTests nonlinearReferenceTests;
