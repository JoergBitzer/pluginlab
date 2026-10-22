#include <cmath>
#include <complex>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Reverb.h"
#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/SyntheticReverb.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDecayTolerance = 0.02;     // the plan's "done when" of W11.2b: T60 per band within 2 % ...
constexpr double kPreDelayTolerance = 0.0001; // 0.1 ms
constexpr double kDrrTolerance = 0.1;        // dB
constexpr double kLn10Times6 = 13.815510557964274; // 6 ln 10: energy falls by 60 dB in T60

double toDb(double magnitude)
{
    return 20.0 * std::log10(magnitude);
}

// The expected decay of a band of the synthetic tail (no noise): the sub-bands' energies through the band filter |H(f)|^2 (white noise: equal
// energy per Hz), each decaying with its T60; the backward integral in closed form; then the same regressions as the measurement
measure::DecayTimes getExpectedDecay(const ref::SyntheticReverb& reverb, const measure::Band& band, double seconds)
{
    const std::vector<ref::BiquadCoefficients> filter = ref::designButterworthBandPass(3, kSampleRate, band.lowHz, band.highHz);
    const std::vector<double>& edges = reverb.getSubBandEdges();
    const std::vector<double>& decays = reverb.getSubBandDecayTimes();
    std::vector<double> weights;
    for (size_t sub = 0; sub + 1 < edges.size(); ++sub)
    {
        constexpr int kPoints = 32;
        double weight = 0.0;
        for (int point = 0; point < kPoints; ++point)
        {
            const double frequency = edges[sub] + (point + 0.5) * (edges[sub + 1] - edges[sub]) / kPoints;
            weight += std::norm(ref::getCascadeResponse(filter, frequency, kSampleRate));
        }
        weights.push_back(weight * (edges[sub + 1] - edges[sub]) / kPoints);
    }
    // on a grid of 1 ms (the curve is smooth; per sample it would cost minutes in a Debug build)
    constexpr double kGridRate = 1000.0;
    const int length = static_cast<int>(seconds * kGridRate);
    std::vector<double> curve(static_cast<size_t>(length));
    for (int sample = 0; sample < length; ++sample)
    {
        const double t = sample / kGridRate;
        double remaining = 0.0;
        for (size_t sub = 0; sub < weights.size(); ++sub)
        {
            remaining += weights[sub] * decays[sub] / kLn10Times6 * std::exp(-kLn10Times6 * t / decays[sub]);
        }
        curve[static_cast<size_t>(sample)] = remaining;
    }
    const double total = curve.front();
    for (double& value : curve)
    {
        value = 10.0 * std::log10(value / total);
    }
    return measure::getDecayTimes(curve, kGridRate);
}

// The mean over seeds (one noise decay scatters by roughly 0.88 sqrt(1 / (B T)), ISO 3382-2: the 2 % of the plan judge the method, not one draw):
// T30 broadband, per octave and per third octave, and one analysis to look at
struct MeanDecay
{
    double broadband = 0.0;
    std::vector<measure::BandDecay> octaves;      // T30 averaged
    std::vector<measure::BandDecay> thirdOctaves;
    measure::ReverbAnalysis first;
};

MeanDecay getMeanDecay(ref::SyntheticReverbSettings settings, int seeds)
{
    MeanDecay mean;
    for (int seed = 1; seed <= seeds; ++seed)
    {
        settings.seed = seed;
        const ref::SyntheticReverb reverb(settings);
        const measure::ReverbAnalysis analysis = measure::analyseReverb(reverb.getImpulseResponse(), {});
        if (seed == 1)
        {
            mean.first = analysis;
            mean.octaves = analysis.octaves;
            mean.thirdOctaves = analysis.thirdOctaves;
            for (measure::BandDecay& band : mean.octaves)
            {
                band.times.t30 = 0.0;
            }
            for (measure::BandDecay& band : mean.thirdOctaves)
            {
                band.times.t30 = 0.0;
            }
        }
        mean.broadband += analysis.broadband.t30 / seeds;
        for (size_t band = 0; band < mean.octaves.size(); ++band)
        {
            mean.octaves[band].times.t30 += analysis.octaves[band].times.t30 / seeds;
        }
        for (size_t band = 0; band < mean.thirdOctaves.size(); ++band)
        {
            mean.thirdOctaves[band].times.t30 += analysis.thirdOctaves[band].times.t30 / seeds;
        }
    }
    return mean;
}

constexpr int kSeeds = 4;

// ... or the statistical scatter of the mean of the seeds' decays where that is larger (narrow low bands: 1 / sqrt(B T seeds))
double getTolerance(const measure::Band& band, double t60)
{
    return std::max(kDecayTolerance, 1.0 / std::sqrt((band.highHz - band.lowHz) * t60 * kSeeds));
}

ref::SyntheticReverbSettings makeSettings()
{
    ref::SyntheticReverbSettings settings;
    settings.sampleRate = kSampleRate;
    settings.preDelaySeconds = 0.02;
    return settings;
}
}

// W11.2a, W11.2b: the band filters, the decay (ISO 3382-1) of the synthetic reference tail with known T60 per band, pre-delay, DRR, Lundeby
class ReverbTests : public juce::UnitTest
{
public:
    ReverbTests()
        : juce::UnitTest("Reverb decay (W11.2a, b)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the bands of IEC 61260-1 (base 10) and their Butterworth band-passes: 0 dB in the middle, -3.01 dB at the edges");
        {
            const std::vector<measure::Band> octaves = measure::getBands(1, 125.0, 8000.0);
            expectEquals(static_cast<int>(octaves.size()), 7);
            expectWithinAbsoluteError(octaves.front().centreHz, 1000.0 * std::pow(10.0, -0.9), 1.0e-9);
            expectEquals(static_cast<int>(measure::getBands(3, 100.0, 10000.0).size()), 21);
            double worstEdge = 0.0;
            double worstCentre = 0.0;
            for (const int fraction : {1, 3})
            {
                for (const measure::Band& band : measure::getBands(fraction, 31.5, 16000.0))
                {
                    if (band.highHz >= 0.5 * kSampleRate)
                    {
                        continue;
                    }
                    const std::vector<ref::BiquadCoefficients> filter = ref::designButterworthBandPass(3, kSampleRate, band.lowHz, band.highHz);
                    for (const double edge : {band.lowHz, band.highHz})
                    {
                        const double error = std::abs(toDb(std::abs(ref::getCascadeResponse(filter, edge, kSampleRate))) + 3.0103);
                        worstEdge = std::max(worstEdge, error);
                    }
                    // the digital image of the pre-warped geometric centre
                    const double w1 = std::tan(kPi * band.lowHz / kSampleRate);
                    const double w2 = std::tan(kPi * band.highHz / kSampleRate);
                    const double centre = kSampleRate / kPi * std::atan(std::sqrt(w1 * w2));
                    worstCentre = std::max(worstCentre, std::abs(toDb(std::abs(ref::getCascadeResponse(filter, centre, kSampleRate)))));
                }
            }
            expectLessThan(worstEdge, 0.001, "-3.01 dB at every edge");
            expectLessThan(worstCentre, 0.001, "0 dB in the middle");
        }

        beginTest("a flat T60 of 2 s: broadband and every band within 2 % (or its statistical scatter; the mean of 4 noise seeds), the pre-delay within 0.1 ms, the DRR within 0.1 dB");
        {
            const MeanDecay mean = getMeanDecay(makeSettings(), kSeeds);
            logAnalysis(mean.first);
            expect(mean.first.hasDirect);
            expectWithinAbsoluteError(mean.broadband, 2.0, 2.0 * kDecayTolerance, "broadband T30");
            for (const std::vector<measure::BandDecay>* bands : {&mean.octaves, &mean.thirdOctaves})
            {
                for (const measure::BandDecay& band : *bands)
                {
                    expectWithinAbsoluteError(band.times.t30, 2.0, 2.0 * getTolerance(band.band, 2.0), "T30 at " + juce::String(band.band.centreHz, 1) + " Hz");
                }
            }
            expectWithinAbsoluteError(mean.first.preDelaySeconds, 0.02, kPreDelayTolerance, "the pre-delay");
            const ref::SyntheticReverb reverb(makeSettings());
            const std::vector<double>& response = reverb.getImpulseResponse();
            double reverberant = 0.0;
            for (size_t sample = 1; sample < response.size(); ++sample)
            {
                reverberant += response[sample] * response[sample];
            }
            expectWithinAbsoluteError(mean.first.directToReverberantDb, 10.0 * std::log10(1.0 / reverberant), kDrrTolerance, "the DRR");
        }

        beginTest("T60 falling with frequency (2.5 s at 125 Hz to 0.8 s at 8 kHz): every band within 2 % of its expected decay (the mean of 4 seeds)");
        {
            ref::SyntheticReverbSettings settings = makeSettings();
            settings.t60Seconds = {2.5, 2.3, 2.0, 1.7, 1.4, 1.1, 0.8};
            const ref::SyntheticReverb reverb(settings);
            const MeanDecay mean = getMeanDecay(settings, kSeeds);
            logAnalysis(mean.first);
            for (const std::vector<measure::BandDecay>* bands : {&mean.octaves, &mean.thirdOctaves})
            {
                for (const measure::BandDecay& band : *bands)
                {
                    const double expected = getExpectedDecay(reverb, band.band, 4.0).t30;
                    expectWithinAbsoluteError(band.times.t30, expected, expected * getTolerance(band.band, expected),
                                              "T30 at " + juce::String(band.band.centreHz, 1) + " Hz (expected " + juce::String(expected, 3) + " s)");
                }
            }
        }

        beginTest("a noise floor 55 dB below the tail's start: Lundeby's truncation and compensation keep T30 within 2 % (the mean of 4 seeds)");
        {
            ref::SyntheticReverbSettings settings = makeSettings();
            settings.noiseFloor = settings.tailLevel * std::pow(10.0, -55.0 / 20.0);
            settings.lengthSeconds = 6.0;
            const MeanDecay mean = getMeanDecay(settings, kSeeds);
            logAnalysis(mean.first);
            expect(mean.first.truncation.noiseFound, "the floor found");
            expectWithinAbsoluteError(mean.broadband, 2.0, 2.0 * kDecayTolerance, "broadband T30");
            for (const measure::BandDecay& band : mean.octaves)
            {
                expectWithinAbsoluteError(band.times.t30, 2.0, 2.0 * getTolerance(band.band, 2.0), "T30 at " + juce::String(band.band.centreHz, 0) + " Hz");
            }
        }

        beginTest("100 % wet with an early reflection: no direct path, the pre-delay from the input to the reflection");
        {
            ref::SyntheticReverbSettings settings = makeSettings();
            settings.directGain = 0.0;
            settings.preDelaySeconds = 0.03;
            settings.reflections = {{0.012, 0.5}};
            const ref::SyntheticReverb reverb(settings);
            const measure::ReverbAnalysis analysis = measure::analyseReverb(reverb.getImpulseResponse(), {});
            expect(! analysis.hasDirect, "no direct path");
            expectWithinAbsoluteError(analysis.preDelaySeconds, 0.012, kPreDelayTolerance, "the pre-delay to the first reflection");
        }
    }

private:
    void logAnalysis(const measure::ReverbAnalysis& analysis)
    {
        juce::String text = "broadband T30 " + juce::String(analysis.broadband.t30, 3) + " s, EDT " + juce::String(analysis.broadband.edt, 3) + " s, C80 "
                            + juce::String(analysis.c80Db, 2) + " dB, pre-delay " + juce::String(analysis.preDelaySeconds * 1000.0, 3) + " ms, DRR "
                            + juce::String(analysis.directToReverberantDb, 2) + " dB; octaves:";
        for (const measure::BandDecay& band : analysis.octaves)
        {
            text << " " << juce::String(band.band.centreHz, 0) << ": " << juce::String(band.times.t30, 3);
        }
        logMessage(text);
    }
};

static ReverbTests reverbTests;
