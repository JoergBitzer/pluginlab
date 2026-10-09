#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/PluginMeasurement.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/measure/Delay.h"
#include "pluginlab/measure/Distortion.h"
#include "pluginlab/measure/FrequencyResponse.h"
#include "pluginlab/measure/Gain.h"
#include "pluginlab/reference/Delays.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Nonlinear.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;
namespace engine = pluginlab::engine;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-300));
}
}

// W7.11: the measurement units on hosted plugins (docs/measurements/plugins-and-host.md). The reference plugins are measured through the plugin
// device (fresh instance per render, the careful delivery of the fingerprint); the results must equal the answers of pluginlab_reference.
class PluginMeasurementTests : public juce::UnitTest
{
public:
    PluginMeasurementTests()
        : juce::UnitTest("Measure: plugins through the host", "pluginlab")
    {
    }

    void runTest() override
    {
        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        testEq();
        testUtility();
        testNonlinear();
        testReport();
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

private:
    struct Loaded
    {
        juce::PluginDescription description;
        std::unique_ptr<juce::AudioPluginInstance> instance;
    };

    Loaded load(const juce::String& pluginName)
    {
        Loaded loaded;
        const juce::File file = testpaths::getReferencePlugin(pluginName);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formats(), file);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (scan.descriptions.isEmpty())
        {
            return loaded;
        }
        loaded.description = scan.descriptions[0];
        juce::String error;
        loaded.instance = formats().createPluginInstance(loaded.description, kSampleRate, kBlockSize, error);
        expect(loaded.instance != nullptr, error);
        return loaded;
    }

    void setText(juce::AudioPluginInstance& plugin, const juce::String& parameterName, const juce::String& text)
    {
        for (juce::AudioProcessorParameter* parameter : plugin.getParameters())
        {
            if (parameter->getName(100) == parameterName)
            {
                parameter->setValueNotifyingHost(parameter->getValueForText(text));
                return;
            }
        }
        expect(false, "no parameter " + parameterName);
    }

    void setChoice(juce::AudioPluginInstance& plugin, const juce::String& parameterName, int index, int count)
    {
        for (juce::AudioProcessorParameter* parameter : plugin.getParameters())
        {
            if (parameter->getName(100) == parameterName)
            {
                parameter->setValueNotifyingHost(static_cast<float>(index) / static_cast<float>(count - 1));
                return;
            }
        }
        expect(false, "no parameter " + parameterName);
    }

    void testEq()
    {
        beginTest("Reference EQ (RBJ peak 1 kHz +6 dB Q 2) through the plugin device: the swept response and the gain at 997 Hz equal H(e^jw)");
        Loaded eq = load("Eq");
        if (eq.instance == nullptr)
        {
            return;
        }
        setChoice(*eq.instance, "Algorithm", 0, 6);
        setChoice(*eq.instance, "Type", 6, 9);
        setText(*eq.instance, "Frequency", "1000 Hz");
        setText(*eq.instance, "Gain", "6.0 dB");
        setText(*eq.instance, "Q", "2.00");
        const measure::Device device = engine::makePluginDevice(formats(), eq.description, engine::getSetting(*eq.instance));
        expectEquals(engine::getPluginChannels(formats(), eq.description, kSampleRate), 2);
        const ref::BiquadCoefficients peak = ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 2.0);

        measure::SweepResponseSettings sweep;
        sweep.frequencies = measure::getStandardThirdOctaveFrequencies();
        const measure::FrequencyResponse response = measure::measureSweptResponse(device, sweep);
        double largest = 0.0;
        for (size_t index = 0; index < sweep.frequencies.size(); ++index)
        {
            const double expected = toDb(std::abs(ref::getBiquadResponse(peak, sweep.frequencies[index], kSampleRate)));
            for (const measure::ChannelResponse& channel : response.channels)
            {
                largest = std::max(largest, std::abs(channel.magnitudeDb[index] - expected));
            }
        }
        logMessage("Reference EQ through the plugin device: swept response within " + juce::String(largest, 5) + " dB of H(e^jw)");
        expect(largest < 0.01, "swept response " + juce::String(largest));

        measure::GainSettings gain;
        const measure::GainResult gainResult = measure::measureGain(device, gain);
        const double expectedGain = toDb(std::abs(ref::getBiquadResponse(peak, 997.0, kSampleRate)));
        expectWithinAbsoluteError(gainResult.channels[0].selectiveGainDb, expectedGain, 0.001, "selective gain at 997 Hz");
        expectWithinAbsoluteError(gainResult.matchingDb, 0.0, 1.0e-4, "gain matching");
    }

    void testUtility()
    {
        beginTest("Reference Utility (-6 dB, inverted, Thiran 37.5 samples): delay, polarity and gain through the plugin device equal the library");
        Loaded utility = load("Utility");
        if (utility.instance == nullptr)
        {
            return;
        }
        setText(*utility.instance, "Gain", "-6.0 dB");
        setChoice(*utility.instance, "Polarity", 1, 2);
        setText(*utility.instance, "Delay", "37.50 samples");
        const measure::Device device = engine::makePluginDevice(formats(), utility.description, engine::getSetting(*utility.instance));
        const measure::DelayResult delay = measure::measureDelay(device, measure::DelaySettings());
        const ref::DirectFormFilter thiran = ref::makeThiranDelay(37.5, 3, kSampleRate);
        const double omega = 2.0 * kPi * 100.0 / kSampleRate;
        // the phase delay of the Thiran filter at 100 Hz, unwrapped around 37 samples (as the unit does)
        const double expectedPhaseDelay = 37.0 - std::arg(thiran.getResponse(100.0) * std::polar(1.0, omega * 37.0)) / omega;
        for (const measure::ChannelDelay& channel : delay.channels)
        {
            expect(channel.invertingByImpulse, "inverted");
            expectWithinAbsoluteError(channel.phaseDelaySamples, expectedPhaseDelay, 0.001, "phase delay at 100 Hz");
        }
        const measure::GainResult gain = measure::measureGain(device, measure::GainSettings());
        expectWithinAbsoluteError(gain.channels[0].selectiveGainDb, -6.0, 0.001, "gain -6 dB (the all-pass keeps the magnitude)");
        logMessage("Reference Utility through the plugin device: phase delay " + juce::String(delay.channels[0].phaseDelaySamples, 4) + " samples (expected "
                   + juce::String(expectedPhaseDelay, 4) + "), gain " + juce::String(gain.channels[0].selectiveGainDb, 4) + " dB, inverted");
    }

    void testNonlinear()
    {
        beginTest("Reference Nonlinear (x + 0.1 x^2 + 0.05 x^3) through the plugin device: harmonics and THD equal the closed form");
        Loaded nonlinear = load("Nonlinear");
        if (nonlinear.instance == nullptr)
        {
            return;
        }
        setChoice(*nonlinear.instance, "Curve", 1, 4);
        setText(*nonlinear.instance, "a2", "0.100");
        setText(*nonlinear.instance, "a3", "0.050");
        const measure::Device device = engine::makePluginDevice(formats(), nonlinear.description, engine::getSetting(*nonlinear.instance));
        measure::DistortionSettings settings;
        settings.channels = 2;
        const measure::DistortionResult result = measure::measureDistortion(device, settings);
        const std::vector<double> closed = ref::getPolynomialHarmonics({0.0, 1.0, 0.1, 0.05}, std::pow(10.0, settings.levelDbfs / 20.0), 3);
        for (int order = 2; order <= 3; ++order)
        {
            expectWithinAbsoluteError(result.channels[0].harmonicDb[static_cast<size_t>(order - 2)], toDb(closed[static_cast<size_t>(order)] / closed[1]), 0.01,
                                      "harmonic " + juce::String(order));
        }
        const double thd = 10.0 * std::log10(std::pow(closed[2] / closed[1], 2.0) + std::pow(closed[3] / closed[1], 2.0));
        expectWithinAbsoluteError(result.channels[0].thdDb, thd, 0.01, "THD");
        logMessage("Reference Nonlinear through the plugin device: THD " + juce::String(result.channels[0].thdDb, 3) + " dB (closed form " + juce::String(thd, 3) + ")");
    }

    void testReport()
    {
        beginTest("the measurement report of a plugin (all units with AES17 defaults) for the Reference EQ");
        Loaded eq = load("Eq");
        if (eq.instance == nullptr)
        {
            return;
        }
        setChoice(*eq.instance, "Algorithm", 0, 6);
        setChoice(*eq.instance, "Type", 6, 9);
        setText(*eq.instance, "Frequency", "1000 Hz");
        setText(*eq.instance, "Gain", "6.0 dB");
        setText(*eq.instance, "Q", "2.00");
        engine::MeasurementOptions options;
        options.linearity = false;
        const engine::MeasurementSummary summary = engine::measurePlugin(formats(), eq.description, engine::getSetting(*eq.instance), options);
        expectEquals(summary.channels, 2);
        const engine::MeasurementRow* response = engine::findRow(summary, "response 20 Hz ... 20 kHz re 997 Hz (third octaves, channel 1)");
        expect(response != nullptr, "frequency response row");
        if (response != nullptr)
        {
            // the RBJ peak at the third-octave frequencies: from 0 dB (far away) to its value at 1 kHz relative to 997 Hz ... the range is the peak's
            const ref::BiquadCoefficients peak = ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 2.0);
            double lowest = 1000.0;
            double highest = -1000.0;
            const double reference = toDb(std::abs(ref::getBiquadResponse(peak, 997.0, kSampleRate)));
            for (const double f : measure::getStandardThirdOctaveFrequencies())
            {
                const double value = toDb(std::abs(ref::getBiquadResponse(peak, f, kSampleRate))) - reference;
                lowest = std::min(lowest, value);
                highest = std::max(highest, value);
            }
            expectWithinAbsoluteError(response->value, highest - lowest, 0.01, "frequency response range");
        }
        const engine::MeasurementRow* level = engine::findRow(summary, "maximum input level (THD+N -40 dB, channel 1)");
        expect(level != nullptr && std::isnan(level->value), "a linear EQ has no maximum input level up to +24 dBFS");
        const juce::String report = engine::createMeasurementReport(summary);
        expect(report.contains("## Measurements (AES17)") && report.contains("AES17 6.4.1"), "report text");
        logMessage("measurement report of the Reference EQ (" + juce::String(summary.seconds, 1) + " s):\n" + report);
    }

    juce::AudioPluginFormatManager& formats()
    {
        return *m_formatManager;
    }

    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static PluginMeasurementTests pluginMeasurementTests;
