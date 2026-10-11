#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/KnobLaw.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/eqmodel/BandFit.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/measure/FrequencyResponse.h"

namespace
{
namespace engine = pluginlab::engine;
namespace eq = pluginlab::eqmodel;
namespace ref = pluginlab::reference;

constexpr double kSampleRate = 48000.0;
constexpr double kDriveToleranceDb = 0.05;    // the plugin set from the mapping against the wanted band
constexpr double kCompareFloorDb = -60.0;     // where the wanted response is below this, the comparison stops (stop bands)

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-15));
}

eq::Band makeBand(eq::DesignFamily family, ref::FilterType type, double frequencyHz, double gainDb, double q, int order = 2)
{
    eq::Band band;
    band.family = family;
    band.type = type;
    band.frequencyHz = frequencyHz;
    band.gainDb = gainDb;
    band.q = q;
    band.order = order;
    return band;
}
}

// W8.5: knob laws, mapping files, and the confirmed mappings in mappings/ driving the plugins (docs/eqmodel/mapping.md)
class MappingTests : public juce::UnitTest
{
public:
    MappingTests()
        : juce::UnitTest("Mapping (W8.5)", "pluginlab")
    {
    }

    void runTest() override
    {
        testParsing();
        testLaws();
        testJson();
        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        testDrive("PluginLab Reference EQ", testpaths::getReferencePlugin("Eq"),
                  {makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 1000.0, 6.0, 2.0),
                   makeBand(eq::DesignFamily::Orfanidis, ref::FilterType::Peak, 3000.0, -4.0, 1.0),
                   makeBand(eq::DesignFamily::Rbj, ref::FilterType::LowShelf, 200.0, 3.0, 0.71),
                   makeBand(eq::DesignFamily::Butterworth, ref::FilterType::LowPass, 2000.0, 0.0, 0.71, 4)});
        testDrive("PluginLab Test EQ Switch", testpaths::getEqSwitchPlugin(), {makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 500.0, 8.0, 3.0)});
        testDrive("PluginLab Test EQ Drive", testpaths::getEqDrivePlugin(), {makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 2000.0, -6.0, 1.5)});
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

private:
    void testParsing()
    {
        beginTest("the numbers in the texts plugins show");
        const std::vector<std::pair<juce::String, double>> cases = {
            {"1000 Hz", 1000.0}, {"1.2 kHz", 1200.0}, {"1k", 1000.0}, {"-3.0dB", -3.0}, {"Q 0.70", 0.7}, {"+6 dB", 6.0}, {".5", 0.5}, {"21.5999985", 21.5999985}};
        for (const auto& [text, value] : cases)
        {
            const std::optional<double> parsed = engine::parseDisplayedNumber(text);
            expect(parsed.has_value(), text);
            if (parsed)
            {
                expectWithinAbsoluteError(*parsed, value, 1.0e-9, text);
            }
        }
        expect(! engine::parseDisplayedNumber("Off").has_value(), "Off has no number");
    }

    void testLaws()
    {
        beginTest("knob laws: linear, log and JUCE's skew recovered exactly, inverse");
        // JUCE's NormalisableRange with a skew (as the test EQ's frequency: 20 ... 20000 Hz, skew 0.3)
        const juce::NormalisableRange<double> skewed(20.0, 20000.0, 0.0, 0.3);
        struct Case
        {
            juce::String name;
            std::function<double(double)> law;
            engine::LawKind kind;
        };
        const std::vector<Case> cases = {
            {"linear -24 ... 24 dB", [](double v) { return -24.0 + 48.0 * v; }, engine::LawKind::Linear},
            {"log 20 ... 20000 Hz", [](double v) { return 20.0 * std::pow(1000.0, v); }, engine::LawKind::Logarithmic},
            {"JUCE skew 0.3, 20 ... 20000 Hz", [skewed](double v) { return skewed.convertFrom0to1(v); }, engine::LawKind::Power},
        };
        for (const Case& item : cases)
        {
            for (const int count : {5, 101})
            {
                std::vector<engine::LawPoint> points;
                for (int index = 0; index < count; ++index)
                {
                    const double v = count == 5 ? 0.05 + 0.9 * index / 4.0 : index / 100.0; // the survey's points, or the sampled texts
                    points.push_back({v, item.law(v)});
                }
                const std::optional<engine::KnobLaw> law = engine::fitKnobLaw(points);
                expect(law.has_value(), item.name);
                if (! law)
                {
                    continue;
                }
                expectEquals(juce::String(engine::getLawKindName(law->kind)), juce::String(engine::getLawKindName(item.kind)), item.name + " (" + juce::String(count) + " points)");
                double worst = 0.0;
                double worstInverse = 0.0;
                for (int index = 0; index <= 100; ++index)
                {
                    const double v = index / 100.0;
                    const double x = item.law(v);
                    worst = std::max(worst, std::abs(law->toValue(v) - x) / std::max(std::abs(x), 1.0));
                    // the round trip in the value (near v = 0 a skewed law is so flat that v itself is ill-defined)
                    worstInverse = std::max(worstInverse, std::abs(law->toValue(law->toNormalised(x)) - x) / std::max(std::abs(x), 1.0));
                }
                expectLessThan(worst, 1.0e-6, item.name + ": relative error");
                expectLessThan(worstInverse, 1.0e-6, item.name + ": inverse (round trip of the value)");
                m_lines.add(item.name + ", " + juce::String(count) + " points: " + law->describe({}) + ", largest relative error " + juce::String(worst, 9));
            }
        }
        const engine::KnobLaw table = engine::makeTableLaw({{0.0, 1.0}, {1.0 / 7.0, 2.0}, {2.0 / 7.0, 3.0}, {3.0 / 7.0, 4.0}, {4.0 / 7.0, 5.0}, {5.0 / 7.0, 6.0}, {6.0 / 7.0, 7.0}, {1.0, 8.0}});
        expectWithinAbsoluteError(table.toNormalised(4.0), 3.0 / 7.0, 1.0e-9, "table: the step of 4");
        expectWithinAbsoluteError(table.toValue(5.0 / 7.0), 6.0, 1.0e-9, "table: the value of step 5");
        logMessage(m_lines.joinIntoString("\n"));
    }

    void testJson()
    {
        beginTest("a mapping survives the JSON file");
        engine::Mapping mapping;
        mapping.pluginName = "Example";
        mapping.sampleRates = {48000.0};
        mapping.bands = 1;
        engine::MappedParameter frequency;
        frequency.index = 2;
        frequency.name = "Frequency";
        frequency.quantity = engine::MappedQuantity::Frequency;
        frequency.band = 0;
        frequency.requirements.push_back({1, "Enabled", 1.0f, "On"});
        engine::KnobLaw law;
        law.kind = engine::LawKind::Power;
        law.minimum = 20.0;
        law.maximum = 20000.0;
        law.skew = 0.3;
        frequency.measuredLaw = law;
        mapping.parameters.push_back(frequency);
        engine::MappedParameter type;
        type.index = 0;
        type.name = "Type";
        type.quantity = engine::MappedQuantity::Type;
        type.band = 0;
        engine::SwitchStep step;
        step.value = 0.5f;
        step.text = "Peak";
        step.type = ref::FilterType::Peak;
        step.family = eq::DesignFamily::Orfanidis;
        step.bandPresent = true;
        type.steps.push_back(step);
        mapping.parameters.push_back(type);
        const juce::File file = juce::File::createTempFile(".json");
        expect(mapping.write(file));
        juce::String error;
        const std::optional<engine::Mapping> read = engine::Mapping::read(file, error);
        file.deleteFile();
        expect(read.has_value(), error);
        if (read)
        {
            expect(engine::compareMappings(*read, mapping).isEmpty(), engine::compareMappings(*read, mapping).joinIntoString("\n"));
            expectEquals(read->parameters[0].requirements[0].text, juce::String("On"));
        }
        expectEquals(engine::getMappingFileName("PluginLab Reference EQ"), juce::String("pluginlab_reference_eq.json"));
    }

    // the confirmed mapping sets the plugin to a band; the measured response must be that band's
    void testDrive(const juce::String& pluginName, const juce::File& file, const std::vector<eq::Band>& wanted)
    {
        beginTest(pluginName + ": the confirmed mapping drives the plugin to the wanted bands");
        juce::String error;
        const std::optional<engine::Mapping> mapping = engine::Mapping::read(juce::File(PLUGINLAB_MAPPING_DIR).getChildFile(engine::getMappingFileName(pluginName)), error);
        expect(mapping.has_value(), error);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(*m_formatManager, file);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (! mapping || scan.descriptions.isEmpty())
        {
            return;
        }
        const juce::PluginDescription& description = scan.descriptions[0];
        const std::vector<float> base = engine::getDefaultSetting(*m_formatManager, description, kSampleRate);
        const std::vector<double> grid = eq::makeLogGrid(20.0, 20000.0, 24.0);
        for (const eq::Band& band : wanted)
        {
            juce::StringArray missing;
            const std::vector<float> setting = engine::makeSettingForBand(*mapping, base, 0, band, missing);
            expect(missing.isEmpty(), pluginName + ": the mapping cannot set " + missing.joinIntoString("; "));
            pluginlab::measure::SweepResponseSettings sweep;
            sweep.channels = 1;
            sweep.frequencies = grid;
            const pluginlab::measure::FrequencyResponse measured =
                pluginlab::measure::measureSweptResponse(engine::makePluginDevice(*m_formatManager, description, setting), sweep);
            const eq::BandDesign design = eq::designBand(band, kSampleRate);
            double worst = 0.0;
            for (size_t index = 0; index < grid.size(); ++index)
            {
                const double wantedDb = toDb(std::abs(eq::getBandResponse(design, band, grid[index], kSampleRate)));
                if (wantedDb < kCompareFloorDb)
                {
                    continue;
                }
                worst = std::max(worst, std::abs(toDb(std::abs(measured.channels[0].response[index])) - wantedDb));
            }
            const juce::String bandName = juce::String(eq::getFamilyName(band.family)) + " " + ref::getFilterTypeName(band.type) + " " + juce::String(band.frequencyHz, 0) + " Hz";
            expectLessThan(worst, kDriveToleranceDb, pluginName + ", " + bandName);
            juce::StringArray values;
            for (const float value : setting)
            {
                values.add(juce::String(value, 4));
            }
            logMessage(pluginName + ", " + bandName + ": setting {" + values.joinIntoString(", ") + "}, largest deviation " + juce::String(worst, 4) + " dB");
        }
    }

    juce::StringArray m_lines;
    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static MappingTests mappingTests;
