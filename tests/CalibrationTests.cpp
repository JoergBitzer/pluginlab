#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/Calibration.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/ParameterSurvey.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
namespace engine = pluginlab::engine;

constexpr double kFsFaultRate = 44100.0;     // the Test EQ Fs Fault designs its band for 44.1 kHz (the PeakEQ of W5)
constexpr double kFactorTolerance = 0.005;   // the measured factors against the known ones
constexpr double kKnobFrequencyFactor = 1.05; // the Test EQ Knob Faults: 5 % high, gain half the dB shown, "Q" a bandwidth in octaves
constexpr double kKnobGainFactor = 0.5;

const engine::ParameterCalibration* findParameter(const engine::CalibrationResult& result, engine::MappedQuantity quantity)
{
    for (const engine::ParameterCalibration& parameter : result.parameters)
    {
        if (parameter.quantity == quantity)
        {
            return &parameter;
        }
    }
    return nullptr;
}
}

// W8.6: the calibration, "myth and reality" (docs/eqmodel/calibration.md). The confirmed mappings of mappings/ give the band parameters; the
// plugins with known knob errors must show exactly these, the correct ones none. One test per plugin (PluginLabCalibration_*), side by side.
class CalibrationTests : public juce::UnitTest
{
public:
    using Check = std::function<void(CalibrationTests&, const engine::CalibrationResult&)>;

    CalibrationTests(const juce::String& pluginName, std::function<juce::File()> file, Check check)
        : juce::UnitTest("Calibration (W8.6): " + pluginName, "pluginlab"), m_pluginName(pluginName), m_file(std::move(file)), m_check(std::move(check))
    {
    }

    void runTest() override
    {
        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        beginTest(m_pluginName + ": the deviations the code has, and no others");
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(*m_formatManager, m_file());
        juce::String error;
        const std::optional<engine::Mapping> mapping =
            engine::Mapping::read(juce::File(PLUGINLAB_MAPPING_DIR).getChildFile(engine::getMappingFileName("PluginLab " + m_pluginName)), error);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + m_file().getFullPathName());
        expect(mapping.has_value(), error);
        if (! scan.descriptions.isEmpty() && mapping)
        {
            const engine::SurveySubject subject = engine::makePluginSubject(*m_formatManager, scan.descriptions[0], 48000.0);
            const engine::CalibrationResult result = engine::calibrate(subject, *mapping);
            logMessage(engine::createCalibrationReport(result));
            expect(! result.parameters.empty(), "the mapping has band parameters");
            m_check(*this, result);
        }
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

    void expectNoDeviation(const engine::CalibrationResult& result)
    {
        for (const engine::ParameterCalibration& parameter : result.parameters)
        {
            expect(! parameter.deviates, parameter.name + ": " + parameter.conclusion);
            for (const engine::CalibrationSeries& series : parameter.rates)
            {
                expect(series.measuredPoints >= 3, parameter.name + ": measured at " + juce::String(series.measuredPoints) + " positions at "
                                                       + juce::String(series.sampleRate));
            }
        }
    }

    void expectFsFault(const engine::CalibrationResult& result)
    {
        const engine::ParameterCalibration* frequency = findParameter(result, engine::MappedQuantity::Frequency);
        expect(frequency != nullptr && frequency->deviates, "the frequency must deviate");
        if (frequency == nullptr)
        {
            return;
        }
        expect(frequency->conclusion.contains("designed for 44.1 kHz"), frequency->conclusion);
        for (const engine::CalibrationSeries& series : frequency->rates)
        {
            expect(series.constant, juce::String(series.sampleRate) + ": " + series.pattern);
            expectWithinAbsoluteError(series.factor, series.sampleRate / kFsFaultRate, kFactorTolerance * series.sampleRate / kFsFaultRate,
                                      "factor at " + juce::String(series.sampleRate));
            expect(series.deviates == (series.sampleRate != kFsFaultRate), "deviates at " + juce::String(series.sampleRate));
        }
        const engine::ParameterCalibration* gain = findParameter(result, engine::MappedQuantity::Gain);
        expect(gain != nullptr && ! gain->deviates, "the gain is as shown (the centre gain of a peak does not depend on the rate)");
    }

    void expectKnobFaults(const engine::CalibrationResult& result)
    {
        const engine::ParameterCalibration* frequency = findParameter(result, engine::MappedQuantity::Frequency);
        const engine::ParameterCalibration* gain = findParameter(result, engine::MappedQuantity::Gain);
        const engine::ParameterCalibration* q = findParameter(result, engine::MappedQuantity::Q);
        expect(frequency != nullptr && gain != nullptr && q != nullptr, "frequency, gain and Q are mapped");
        if (frequency == nullptr || gain == nullptr || q == nullptr)
        {
            return;
        }
        expect(frequency->conclusion.contains("the same at every rate"), frequency->conclusion);
        for (const engine::CalibrationSeries& series : frequency->rates)
        {
            expect(series.deviates && series.constant, "frequency " + series.pattern);
            expectWithinAbsoluteError(series.factor, kKnobFrequencyFactor, kFactorTolerance, "frequency factor at " + juce::String(series.sampleRate));
        }
        for (const engine::CalibrationSeries& series : gain->rates)
        {
            expect(series.deviates && series.constant, "gain " + series.pattern);
            expectWithinAbsoluteError(series.factor, kKnobGainFactor, kFactorTolerance, "gain slope at " + juce::String(series.sampleRate));
            expectWithinAbsoluteError(series.offset, 0.0, 0.05, "gain offset at " + juce::String(series.sampleRate));
        }
        for (const engine::CalibrationSeries& series : q->rates)
        {
            expect(series.pattern.contains("bandwidth in octaves"), "Q at " + juce::String(series.sampleRate) + ": " + series.pattern);
        }
    }

private:
    juce::String m_pluginName;
    std::function<juce::File()> m_file;
    Check m_check;
    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static CalibrationTests calibrationEq("Test EQ", [] { return testpaths::getEqPlugin(); },
                                      [](CalibrationTests& test, const engine::CalibrationResult& result) { test.expectNoDeviation(result); });
static CalibrationTests calibrationEqFs("Test EQ Fs Fault", [] { return testpaths::getEqFsFaultPlugin(); },
                                        [](CalibrationTests& test, const engine::CalibrationResult& result) { test.expectFsFault(result); });
static CalibrationTests calibrationEqKnobs("Test EQ Knob Faults", [] { return testpaths::getEqKnobFaultPlugin(); },
                                           [](CalibrationTests& test, const engine::CalibrationResult& result) { test.expectKnobFaults(result); });
static CalibrationTests calibrationReferenceEq("Reference EQ", [] { return testpaths::getReferencePlugin("Eq"); },
                                               [](CalibrationTests& test, const engine::CalibrationResult& result) { test.expectNoDeviation(result); });
