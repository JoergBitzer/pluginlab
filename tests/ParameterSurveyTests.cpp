#include <functional>
#include <map>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/ParameterSurvey.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
namespace engine = pluginlab::engine;
using Role = engine::ParameterRole;

struct Expected
{
    Role role;
    bool coupled;
    bool classChange = false;
};
}

// W8.4: the parameter survey (docs/eqmodel/parameter-survey.md). Every parameter of the reference plugins and of the test plugins with a known
// behaviour is surveyed through the host; its role (and whether it acts only together with another parameter) must be the one the code says.
// One test per plugin, so that CTest can run them side by side (PluginLabParameterSurvey_*).
class ParameterSurveyTests : public juce::UnitTest
{
public:
    ParameterSurveyTests(const juce::String& pluginName, std::function<juce::File()> file, std::map<juce::String, Expected> expected)
        : juce::UnitTest("Parameter survey (W8.4): " + pluginName, "pluginlab"),
          m_pluginName(pluginName),
          m_file(std::move(file)),
          m_expected(std::move(expected))
    {
    }

    void runTest() override
    {
        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        testPlugin(m_pluginName, m_file(), m_expected);
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

private:
    void testPlugin(const juce::String& pluginName, const juce::File& file, const std::map<juce::String, Expected>& expected)
    {
        beginTest(pluginName + ": every parameter classified as the code says");
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(*m_formatManager, file);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (scan.descriptions.isEmpty())
        {
            return;
        }
        const engine::SurveySubject subject = engine::makePluginSubject(*m_formatManager, scan.descriptions[0], 48000.0);
        const double start = juce::Time::getMillisecondCounterHiRes();
        const engine::SurveyResult result = engine::surveyParameters(subject, engine::SurveySettings{});
        const double seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        logMessage(engine::createSurveyReport(result, pluginName) + "\n(" + juce::String(seconds, 1) + " s)\n");
        for (const engine::ParameterSurvey& survey : result.parameters)
        {
            const auto found = expected.find(survey.parameter.name);
            if (found == expected.end())
            {
                logMessage("not checked: " + survey.parameter.name + ": " + survey.summary);
                continue;
            }
            const Expected& want = found->second;
            expectEquals(engine::getRoleName(survey.role), engine::getRoleName(want.role), pluginName + ", " + survey.parameter.name + ": " + survey.summary);
            expect(survey.coupled == want.coupled, pluginName + ", " + survey.parameter.name + (want.coupled ? ": should act only in a context" : ": should act at the default"));
            const bool classChange = survey.atDefault.changesSystemClass || (survey.inContext && survey.inContext->changesSystemClass);
            if (want.classChange)
            {
                expect(classChange, pluginName + ", " + survey.parameter.name + ": changes the system class");
            }
        }
        for (const auto& [parameterName, want] : expected)
        {
            juce::ignoreUnused(want);
            const bool present = std::any_of(result.parameters.begin(), result.parameters.end(),
                                             [&](const engine::ParameterSurvey& survey) { return survey.parameter.name == parameterName; });
            expect(present, pluginName + ": no parameter " + parameterName);
        }
    }

    juce::String m_pluginName;
    std::function<juce::File()> m_file;
    std::map<juce::String, Expected> m_expected;
    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static ParameterSurveyTests surveyEqDrive("Test EQ Drive", [] { return testpaths::getEqDrivePlugin(); },
                                   {{"Gain", {Role::Gain, false}},
                                    {"Frequency", {Role::Frequency, true}},
                                    {"Q", {Role::Width, true}},
                                    {"Drive", {Role::Distortion, false, true}},
                                    {"Output", {Role::Level, false}},
                                    {"Display", {Role::Nothing, false}},
                                    {"Bypass", {Role::Switch, true}}});

static ParameterSurveyTests surveyEqSwitch("Test EQ Switch", [] { return testpaths::getEqSwitchPlugin(); },
                                   {{"Gain", {Role::Gain, true}}, {"Frequency", {Role::Frequency, true}}, {"Q", {Role::Width, true}}, {"Enabled", {Role::Switch, true}}, {"Bypass", {Role::Switch, true}}});

static ParameterSurveyTests surveyReferenceEq("Reference EQ", [] { return testpaths::getReferencePlugin("Eq"); },
                                   {{"Algorithm", {Role::Switch, false}},
                                    {"Type", {Role::Switch, false}},
                                    {"Frequency", {Role::Frequency, true}},
                                    {"Gain", {Role::Gain, false}},
                                    {"Q", {Role::Width, true}},
                                    {"Order", {Role::Slope, true}},
                                    {"Bypass", {Role::Switch, true}}});

static ParameterSurveyTests surveyReferenceUtility("Reference Utility", [] { return testpaths::getReferencePlugin("Utility"); },
                                   {{"Gain", {Role::Level, false}},
                                    {"Polarity", {Role::Polarity, false}},
                                    {"Delay", {Role::Latency, false}},
                                    {"Interpolation", {Role::Switch, true}},
                                    {"Width", {Role::Stereo, false}},
                                    {"Crosstalk", {Role::Stereo, false}},
                                    {"DC offset", {Role::IdleOutput, false}},
                                    {"Tremolo rate", {Role::Modulation, true}},
                                    {"Tremolo depth", {Role::Modulation, false, true}},
                                    {"Bypass", {Role::Latency, true}}});

static ParameterSurveyTests surveyReferenceNonlinear("Reference Nonlinear", [] { return testpaths::getReferencePlugin("Nonlinear"); },
                                   {{"Drive", {Role::Level, false}},
                                    {"a2", {Role::Distortion, true}},
                                    {"a3", {Role::Distortion, true}},
                                    {"Threshold", {Role::Distortion, true}},
                                    {"Output", {Role::Level, false}},
                                    {"Bits", {Role::Distortion, true}},
                                    {"Noise level", {Role::IdleOutput, true}},
                                    {"Hum level", {Role::IdleOutput, true}},
                                    {"Curve", {Role::Switch, false, true}},
                                    {"Quantizer", {Role::Switch, false}},
                                    {"Noise", {Role::Switch, false}},
                                    {"Hum", {Role::Switch, false}},
                                    {"Bypass", {Role::Switch, true}}});
