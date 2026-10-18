#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/ZipperNoise.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
namespace engine = pluginlab::engine;

constexpr double kSampleRate = 48000.0;
constexpr int kLength = 65536;
constexpr int kBlockSize = 512;
constexpr double kGuardHz = 45.0;
constexpr double kAmplitude = 0.1;
constexpr double kLowGain = 0.25;   // -12 dB
constexpr double kHighGain = 4.0;   // +12 dB
constexpr int kGainIndex = 0;
}

// The zipper noise test of the fingerprint (docs/reference/zipper-noise.md): the residual away from tones while a parameter ramps in steps
class ZipperNoiseTests : public juce::UnitTest
{
public:
    ZipperNoiseTests()
        : juce::UnitTest("Zipper noise", "pluginlab")
    {
    }

    void runTest() override
    {
        const std::vector<double> tones = engine::getZipperTones(kSampleRate, kLength);

        beginTest("the residual: steady tones have none; a gain in steps once per block has a lot; a gain ramped within the blocks has little");
        {
            const double still = engine::measureToneResidual(makeTones(tones, [](int) { return 1.0; }), kSampleRate, tones, kGuardHz).relativeDb;
            const double steps = engine::measureToneResidual(makeTones(tones, [](int index) { return gainAt(index / kBlockSize * kBlockSize); }), kSampleRate, tones,
                                                             kGuardHz).relativeDb;
            const double ramped = engine::measureToneResidual(makeTones(tones, [](int index) { return gainAt(index); }), kSampleRate, tones, kGuardHz).relativeDb;
            logMessage("residual re the tones: still " + juce::String(still, 1) + " dB, steps per block " + juce::String(steps, 1) + " dB, ramped "
                       + juce::String(ramped, 1) + " dB");
            expectLessThan(still, -120.0);
            expectGreaterThan(steps, -80.0);
            expectLessThan(ramped, -95.0);
        }

        beginTest("the tone level: five tones of -20 dBFS");
        {
            const engine::ToneResidual levels = engine::measureToneResidual(makeTones(tones, [](int) { return 1.0; }), kSampleRate, tones, kGuardHz);
            expectWithinAbsoluteError(levels.tonesDbfs, -20.0 + 10.0 * std::log10(static_cast<double>(tones.size())), 0.1);
        }

        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        check("Test Gain (no smoothing): zipper noise", testpaths::getGainPlugin(), true);
        check("Test Gain Ramp (ramped within each block): none", testpaths::getGainRampPlugin(), false);
        check("Test Block Smoothing (half the way per block: still a step at every block)", testpaths::getBlockSmoothingPlugin(), true);
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

private:
    // the gain of the test plugins' ramp from -12 to +12 dB over the length (in dB, linear in time)
    static double gainAt(int index)
    {
        const double fraction = static_cast<double>(index) / kLength;
        return std::pow(10.0, (20.0 * std::log10(kLowGain) + fraction * 20.0 * std::log10(kHighGain / kLowGain)) / 20.0);
    }

    template <typename Gain>
    static std::vector<float> makeTones(const std::vector<double>& tones, Gain gain)
    {
        std::vector<float> signal(static_cast<size_t>(kLength), 0.0f);
        for (size_t tone = 0; tone < tones.size(); ++tone)
        {
            const double step = 2.0 * juce::MathConstants<double>::pi * tones[tone] / kSampleRate;
            for (int index = 0; index < kLength; ++index)
            {
                signal[static_cast<size_t>(index)] += static_cast<float>(kAmplitude * std::sin(0.7 * static_cast<double>(tone) + step * index));
            }
        }
        for (int index = 0; index < kLength; ++index)
        {
            signal[static_cast<size_t>(index)] *= static_cast<float>(gain(index));
        }
        return signal;
    }

    void check(const juce::String& label, const juce::File& file, bool zipperExpected)
    {
        beginTest(label);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(*m_formatManager, file);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (scan.descriptions.isEmpty())
        {
            return;
        }
        const engine::FingerprintSettings settings;
        const std::vector<float> defaults = engine::getDefaultSetting(*m_formatManager, scan.descriptions[0], kSampleRate);
        const engine::ZipperFingerprint result = engine::measureZipperNoise(*m_formatManager, scan.descriptions[0], defaults, {kGainIndex}, settings);
        expectEquals(static_cast<int>(result.parameters.size()), 1);
        if (result.parameters.empty())
        {
            return;
        }
        const engine::ZipperParameterResult& gain = result.parameters.front();
        logMessage(gain.name + ": change " + juce::String(gain.changeDb, 1) + " dB, moving " + juce::String(gain.rampDb, 1) + " dB, still "
                   + juce::String(gain.stillDb, 1) + " dB, excess " + juce::String(gain.excessDb, 1) + " dB");
        expect(gain.judged, gain.note);
        expect(gain.zipper == zipperExpected, zipperExpected ? "zipper noise expected" : "no zipper noise expected");
    }

    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static ZipperNoiseTests zipperNoiseTests;
