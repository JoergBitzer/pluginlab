#include <cmath>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/LivePluginDevice.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/eqmodel/EqModel.h"
#include "pluginlab/match/EqDomain.h"

namespace
{
namespace engine = pluginlab::engine;
namespace match = pluginlab::match;

constexpr double kSampleRate = 48000.0;
constexpr double kSameDb = 0.05;           // fast path against careful path (the sweep's short window is the less exact one: up to 0.047 dB at Q 8)
constexpr double kExactDb = 0.001;         // fast path against the exact curve, where it is known
constexpr double kFastSeconds = 0.25;      // per render (the author's budget: about 40 renders in 10 ... 15 s)
constexpr double kCompareFloorDb = -60.0;  // below, both measurements are noise (a stop band)
}

// the Test EQ's band from its normalised gain, frequency and Q (its code: gain -24 ... 24 dB, frequency 20 ... 20000 Hz with JUCE's skew 0.3,
// Q 0.1 ... 10): the exact RBJ peak
std::vector<double> getTestEqCurve(const std::vector<float>& values, const std::vector<double>& grid)
{
    const juce::NormalisableRange<double> frequency(20.0, 20000.0, 0.0, 0.3);
    pluginlab::eqmodel::Band band;
    band.family = pluginlab::eqmodel::DesignFamily::Rbj;
    band.type = pluginlab::reference::FilterType::Peak;
    band.gainDb = -24.0 + 48.0 * values[0];
    band.frequencyHz = frequency.convertFrom0to1(values[1]);
    band.q = 0.1 + 9.9 * values[2];
    return match::getCurveMagnitude(pluginlab::eqmodel::EqModel{{band}, 0.0}, grid, kSampleRate);
}

// W9.3: the fast render path of the matcher (docs/matching/fast-render-path.md): one live instance and an impulse give the same magnitude as
// a fresh instance and a sweep, much faster; non-LTI targets are refused.
class MatchPluginTests : public juce::UnitTest
{
public:
    MatchPluginTests()
        : juce::UnitTest("Fast render path (W9.3)", "pluginlab")
    {
    }

    void runTest() override
    {
        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        match::MagnitudeProbeSettings careful;
        careful.sampleRate = kSampleRate;
        match::MagnitudeProbeSettings fast = careful;
        fast.stimulus = match::ProbeStimulus::Impulse;
        m_careful = std::make_unique<match::MagnitudeProbe>(careful);
        m_fast = std::make_unique<match::MagnitudeProbe>(fast);

        // settings: gain, frequency, Q (the first three parameters of the test EQs)
        m_exact = getTestEqCurve;
        compare("Test EQ", testpaths::getEqPlugin(), {{0.75f, 0.4f, 0.2f}, {0.3f, 0.7f, 0.5f}, {0.9f, 0.25f, 0.8f}});
        m_exact = nullptr;
        compare("Test EQ Prepare Fault (reads a parameter only when it changes)", testpaths::getEqPrepareFaultPlugin(), {{0.75f, 0.4f, 0.2f}, {0.3f, 0.7f, 0.5f}});
        compare("Test EQ Switch (band switched on)", testpaths::getEqSwitchPlugin(), {{0.75f, 0.4f, 0.2f, 1.0f}, {0.2f, 0.6f, 0.4f, 1.0f}});
        // Reference EQ: algorithm, type, frequency, gain, Q, order (normalised): RBJ peak; Butterworth low-pass order 4
        compare("Reference EQ", testpaths::getReferencePlugin("Eq"), {{0.0f, 0.75f, 0.5f, 0.7f, 0.1f, 0.2f}, {0.67f, 0.0f, 0.6f, 0.5f, 0.1f, 3.0f / 7.0f}});

        refuse("Test EQ Drive with the drive on", testpaths::getEqDrivePlugin(), 3, 1.0f);
        refuse("Reference Utility with the tremolo on", testpaths::getReferencePlugin("Utility"), 8, 0.5f);
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

private:
    std::optional<juce::PluginDescription> scan(const juce::File& file)
    {
        const pluginlab::hosting::PluginScanResult result = pluginlab::hosting::PluginScanner::scanFileInProcess(*m_formatManager, file);
        expect(! result.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (result.descriptions.isEmpty())
        {
            return std::nullopt;
        }
        return result.descriptions[0];
    }

    std::vector<float> withValues(std::vector<float> setting, const std::vector<float>& values)
    {
        for (size_t index = 0; index < values.size() && index < setting.size(); ++index)
        {
            setting[index] = values[index];
        }
        return setting;
    }

    void compare(const juce::String& label, const juce::File& file, const std::vector<std::vector<float>>& settings)
    {
        beginTest(label + ": the live instance and an impulse give the magnitude of a fresh instance and a sweep");
        const std::optional<juce::PluginDescription> description = scan(file);
        if (! description)
        {
            return;
        }
        const std::vector<float> defaults = engine::getDefaultSetting(*m_formatManager, *description, kSampleRate);
        engine::LivePluginTarget target(*m_formatManager, *description, kSampleRate);
        expect(target.isValid(), target.getError());
        const engine::LiveTargetCheck check = engine::checkLiveTarget(*m_formatManager, *description, target, withValues(defaults, settings.front()),
                                                                      [this](const pluginlab::measure::Device& device) { return m_fast->measure(device); });
        expect(check.usable, check.reason);
        double fastSeconds = 0.0;
        int fastRenders = 0;
        double worstOverall = 0.0;
        for (const std::vector<float>& values : settings)
        {
            const std::vector<float> setting = withValues(defaults, values);
            const std::vector<double> reference = m_careful->measure(engine::makePluginDevice(*m_formatManager, *description, setting));
            const double started = juce::Time::getMillisecondCounterHiRes();
            const std::vector<double> quick = m_fast->measure(target.makeDevice(setting));
            fastSeconds += (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
            ++fastRenders;
            double worst = 0.0;
            for (size_t index = 0; index < reference.size() && index < quick.size(); ++index)
            {
                if (reference[index] > kCompareFloorDb)
                {
                    worst = std::max(worst, std::abs(reference[index] - quick[index]));
                }
            }
            worstOverall = std::max(worstOverall, worst);
            if (m_exact)
            {
                // which of the two is right: both against the exact curve
                const std::vector<double> exact = m_exact(values, m_fast->getFrequencies());
                double fastError = 0.0;
                double carefulError = 0.0;
                for (size_t index = 0; index < exact.size(); ++index)
                {
                    fastError = std::max(fastError, std::abs(quick[index] - exact[index]));
                    carefulError = std::max(carefulError, std::abs(reference[index] - exact[index]));
                }
                logMessage(label + ": against the exact curve: fast " + juce::String(fastError, 5) + " dB, careful " + juce::String(carefulError, 5) + " dB");
                expectLessThan(fastError, kExactDb, label + ": fast against the exact curve");
            }
            expectLessThan(worst, kSameDb, label + ": fast against careful");
        }
        const double perRender = fastSeconds / std::max(fastRenders, 1);
        expectLessThan(perRender, kFastSeconds, label + ": seconds per fast render");
        logMessage(label + ": largest difference " + juce::String(worstOverall, 5) + " dB, " + juce::String(1000.0 * perRender, 0)
                   + " ms per fast render; repeat difference " + juce::String(check.repeatDifference, 6) + " dB");
    }

    void refuse(const juce::String& label, const juce::File& file, int parameter, float value)
    {
        beginTest(label + ": refused (not LTI)");
        const std::optional<juce::PluginDescription> description = scan(file);
        if (! description)
        {
            return;
        }
        std::vector<float> setting = engine::getDefaultSetting(*m_formatManager, *description, kSampleRate);
        setting[static_cast<size_t>(parameter)] = value;
        engine::LivePluginTarget target(*m_formatManager, *description, kSampleRate);
        const engine::LiveTargetCheck check = engine::checkLiveTarget(*m_formatManager, *description, target, setting,
                                                                      [this](const pluginlab::measure::Device& device) { return m_fast->measure(device); });
        expect(! check.usable, "must be refused");
        expect(check.reason.contains("not LTI"), check.reason);
        logMessage(label + ": " + check.reason);
    }

    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
    std::unique_ptr<match::MagnitudeProbe> m_careful;
    std::unique_ptr<match::MagnitudeProbe> m_fast;
    std::function<std::vector<double>(const std::vector<float>&, const std::vector<double>&)> m_exact; // the exact curve, where known
};

static MatchPluginTests matchPluginTests;
