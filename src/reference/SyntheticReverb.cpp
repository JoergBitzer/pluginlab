#include "pluginlab/reference/SyntheticReverb.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

namespace pluginlab::reference
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kSubBandsPerOctave = 12.0;
constexpr double kLowestSubBandHz = 20.0;
constexpr double kLongestSeconds = 30.0;
constexpr double kDecayToDb = 120.0;

int getOrderAtLeast(int samples)
{
    int order = 0;
    while ((1 << order) < samples)
    {
        ++order;
    }
    return order;
}
}

SyntheticReverb::SyntheticReverb(const SyntheticReverbSettings& settings)
    : m_settings(settings)
{
    const double fs = settings.sampleRate;
    double longest = 0.0;
    for (const double t60 : settings.t60Seconds)
    {
        longest = std::max(longest, t60);
    }
    double seconds = settings.lengthSeconds;
    if (seconds <= 0.0)
    {
        seconds = std::min(kLongestSeconds, settings.preDelaySeconds + kDecayToDb / 60.0 * longest);
    }
    const int length = static_cast<int>(seconds * fs);
    const int start = static_cast<int>(std::lround(settings.preDelaySeconds * fs));
    const int tailLength = std::max(0, length - start);
    m_response.assign(static_cast<size_t>(length), 0.0);
    if (! m_response.empty())
    {
        m_response[0] = settings.directGain;
    }

    // the sub-bands: 1/12 octave from 20 Hz to Nyquist (the first from DC)
    m_edges.push_back(0.0);
    for (double edge = kLowestSubBandHz; edge < 0.5 * fs; edge *= std::pow(2.0, 1.0 / kSubBandsPerOctave))
    {
        m_edges.push_back(edge);
    }
    m_edges.push_back(0.5 * fs);
    for (size_t band = 0; band + 1 < m_edges.size(); ++band)
    {
        const double low = std::max(m_edges[band], kLowestSubBandHz * 0.5);
        m_decays.push_back(getDecayTime(std::sqrt(low * m_edges[band + 1])));
    }

    if (tailLength > 0)
    {
        // white noise of the tail's length, its spectrum split into the sub-bands; each back to time, decayed, added
        const int order = getOrderAtLeast(tailLength);
        const int size = 1 << order;
        juce::dsp::FFT fft(order);
        juce::Random random(settings.seed);
        std::vector<juce::dsp::Complex<float>> noise(static_cast<size_t>(size)), spectrum(static_cast<size_t>(size));
        for (int sample = 0; sample < tailLength; ++sample)
        {
            // Gaussian (Box-Muller)
            const double u1 = std::max(1.0e-12, static_cast<double>(random.nextDouble()));
            const double u2 = random.nextDouble();
            noise[static_cast<size_t>(sample)] = static_cast<float>(std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2));
        }
        fft.perform(noise.data(), spectrum.data(), false);
        // a flat magnitude, the noise's phases: every sub-band carries exactly its share of the energy (a Gaussian spectrum's magnitudes would
        // scatter the sub-bands' energies by a few per cent and with them the decay times of narrow bands); the signal stays noise-like
        double power = 0.0;
        for (const juce::dsp::Complex<float>& value : spectrum)
        {
            power += std::norm(std::complex<double>(value.real(), value.imag()));
        }
        const float magnitude = static_cast<float>(std::sqrt(power / size));
        for (juce::dsp::Complex<float>& value : spectrum)
        {
            const float modulus = std::abs(value);
            if (modulus > 0.0f)
            {
                value *= magnitude / modulus;
            }
        }
        fft.perform(spectrum.data(), noise.data(), true); // the flat-magnitude noise in time

        // the decay per sub-band by a short-time spectrum (Hann analysis and synthesis, 75 % overlap): each frame gets the gains of its centre's
        // time; an exponential smoothed over a frame stays the same exponential, so the decay rates are exact
        const int frameOrder = 11;
        const int frame = 1 << frameOrder;
        const int hop = frame / 4;
        juce::dsp::FFT frameFft(frameOrder);
        std::vector<double> window(static_cast<size_t>(frame));
        for (int index = 0; index < frame; ++index)
        {
            window[static_cast<size_t>(index)] = 0.5 - 0.5 * std::cos(2.0 * kPi * index / frame);
        }
        const double overlapGain = 1.5 * frame / (4.0 * hop); // sum of w^2 over the overlapping frames: 3/8 * frame / hop
        // the sub-band of every bin (by its centre)
        std::vector<double> binRate(static_cast<size_t>(frame / 2 + 1));
        size_t band = 0;
        for (int bin = 0; bin <= frame / 2; ++bin)
        {
            const double frequency = bin * fs / frame;
            while (band + 2 < m_edges.size() && frequency >= m_edges[band + 1])
            {
                ++band;
            }
            binRate[static_cast<size_t>(bin)] = 3.0 * std::log(10.0) / (m_decays[band] * fs); // amplitude 10^(-3 t / T60) per sample
        }
        std::vector<double> tail(static_cast<size_t>(tailLength + frame), 0.0);
        std::vector<juce::dsp::Complex<float>> frameTime(static_cast<size_t>(frame)), frameSpectrum(static_cast<size_t>(frame));
        for (int position = -frame + hop; position < tailLength; position += hop)
        {
            for (int index = 0; index < frame; ++index)
            {
                const int sample = position + index;
                float value = 0.0f;
                if (sample >= 0 && sample < tailLength)
                {
                    value = noise[static_cast<size_t>(sample)].real();
                }
                frameTime[static_cast<size_t>(index)] = static_cast<float>(value * window[static_cast<size_t>(index)]);
            }
            frameFft.perform(frameTime.data(), frameSpectrum.data(), false);
            const double centre = std::max(0.0, position + 0.5 * frame);
            for (int bin = 0; bin <= frame / 2; ++bin)
            {
                const float gain = static_cast<float>(std::exp(-binRate[static_cast<size_t>(bin)] * centre));
                frameSpectrum[static_cast<size_t>(bin)] *= gain;
                if (bin > 0 && bin < frame / 2)
                {
                    frameSpectrum[static_cast<size_t>(frame - bin)] *= gain;
                }
            }
            frameFft.perform(frameSpectrum.data(), frameTime.data(), true);
            for (int index = 0; index < frame; ++index)
            {
                const int sample = position + index;
                if (sample >= 0 && sample < tailLength)
                {
                    tail[static_cast<size_t>(sample)] += frameTime[static_cast<size_t>(index)].real() * window[static_cast<size_t>(index)] / overlapGain;
                }
            }
        }
        // the tail's start at tailLevel RMS (the noise before the decay has unit variance; JUCE's inverse FFT is normalised)
        for (int sample = 0; sample < tailLength; ++sample)
        {
            m_response[static_cast<size_t>(start + sample)] += settings.tailLevel * tail[static_cast<size_t>(sample)];
        }
    }
    for (const ReflectionTap& tap : settings.reflections)
    {
        const size_t at = static_cast<size_t>(std::lround(tap.delaySeconds * fs));
        if (at < m_response.size())
        {
            m_response[at] += tap.gain;
        }
    }
    if (settings.noiseFloor > 0.0)
    {
        juce::Random random(settings.seed + 1000);
        for (double& value : m_response)
        {
            const double u1 = std::max(1.0e-12, static_cast<double>(random.nextDouble()));
            const double u2 = random.nextDouble();
            value += settings.noiseFloor * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
        }
    }
}

const SyntheticReverbSettings& SyntheticReverb::getSettings() const
{
    return m_settings;
}

const std::vector<double>& SyntheticReverb::getImpulseResponse() const
{
    return m_response;
}

double SyntheticReverb::getDecayTime(double frequencyHz) const
{
    const std::vector<double>& bands = m_settings.bandHz;
    const std::vector<double>& times = m_settings.t60Seconds;
    if (bands.empty())
    {
        return 1.0;
    }
    if (frequencyHz <= bands.front())
    {
        return times.front();
    }
    for (size_t index = 1; index < bands.size(); ++index)
    {
        if (frequencyHz <= bands[index])
        {
            const double fraction = std::log(frequencyHz / bands[index - 1]) / std::log(bands[index] / bands[index - 1]);
            return times[index - 1] + fraction * (times[index] - times[index - 1]);
        }
    }
    return times.back();
}

const std::vector<double>& SyntheticReverb::getSubBandEdges() const
{
    return m_edges;
}

const std::vector<double>& SyntheticReverb::getSubBandDecayTimes() const
{
    return m_decays;
}
}
