#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/ui/GuiFormats.h"
#include "pluginlab/reference/Delays.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Nonlinear.h"

namespace
{
namespace ref = pluginlab::reference;

constexpr double kPi = 3.14159265358979323846;
constexpr int kBlockSize = 512;
constexpr int kImpulseLength = 1 << 15;
// a sine whose aliases do not fall on its harmonics, a whole number of periods in the window (as in NonlinearReferenceTests)
constexpr double kSamplesPerPeriod = 4755.0 / 101.0;
constexpr int kWindow = 47550;

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-300));
}

std::complex<double> getSpectrum(const float* data, int length, double frequency, double sampleRate)
{
    std::complex<double> sum = 0.0;
    for (int index = 0; index < length; ++index)
    {
        sum += static_cast<double>(data[index]) * std::polar(1.0, -2.0 * kPi * frequency * index / sampleRate);
    }
    return sum;
}
}

// The reference plugins end to end (W6.4): the host loads the VST3, sets the parameters by their text, processes, and the output is compared
// with the answer of pluginlab_reference
class ReferencePluginTests : public juce::UnitTest
{
public:
    ReferencePluginTests()
        : juce::UnitTest("Reference plugins", "pluginlab")
    {
    }

    void runTest() override
    {
        pluginlab::hosting::addHeadlessFormats(m_formats);
        pluginlab::ui::addGuiFormats(m_guiFormats);
        testEq();
        testEqTypeChoice();
        testNonlinear();
        testUtility();
    }

private:
    juce::AudioPluginFormatManager& getFormats(bool withEditor)
    {
        if (withEditor)
        {
            return m_guiFormats;
        }
        return m_formats;
    }

    // Loads a reference plugin; with withEditor through the formats with editor support (the headless ones cannot open an editor)
    std::unique_ptr<juce::AudioPluginInstance> load(const juce::String& pluginName, double sampleRate, bool withEditor = false)
    {
        juce::AudioPluginFormatManager& formats = getFormats(withEditor);
        const juce::File file = testpaths::getReferencePlugin(pluginName);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formats, file);
        expect(!scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (scan.descriptions.isEmpty())
        {
            return nullptr;
        }
        juce::String error;
        std::unique_ptr<juce::AudioPluginInstance> plugin = formats.createPluginInstance(scan.descriptions[0], sampleRate, kBlockSize, error);
        expect(plugin != nullptr, error);
        return plugin;
    }

    // Sets a parameter (by its name) from a text in its own units; the text the plugin shows afterwards must start with the wanted value
    void setParameter(juce::AudioPluginInstance& plugin, const juce::String& parameterName, const juce::String& text)
    {
        for (juce::AudioProcessorParameter* parameter : plugin.getParameters())
        {
            if (parameter->getName(100) == parameterName)
            {
                parameter->setValueNotifyingHost(parameter->getValueForText(text));
                expect(parameter->getCurrentValueAsText().startsWith(text.upToFirstOccurrenceOf(" ", false, false)),
                       parameterName + ": wanted " + text + ", the plugin shows " + parameter->getCurrentValueAsText());
                return;
            }
        }
        expect(false, "no parameter " + parameterName);
    }

    // Sets a choice parameter (by its name) to an entry
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

    // Processes the buffer in blocks of kBlockSize (prepare first, release after)
    void process(juce::AudioPluginInstance& plugin, juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        plugin.setPlayConfigDetails(buffer.getNumChannels(), buffer.getNumChannels(), sampleRate, kBlockSize);
        plugin.prepareToPlay(sampleRate, kBlockSize);
        juce::MidiBuffer midi;
        for (int start = 0; start < buffer.getNumSamples(); start += kBlockSize)
        {
            const int length = std::min(kBlockSize, buffer.getNumSamples() - start);
            juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);
            plugin.processBlock(block, midi);
        }
        plugin.releaseResources();
    }

    juce::AudioBuffer<float> makeImpulse(int channels)
    {
        juce::AudioBuffer<float> buffer(channels, kImpulseLength);
        buffer.clear();
        for (int channel = 0; channel < channels; ++channel)
        {
            buffer.setSample(channel, 0, 1.0f);
        }
        return buffer;
    }

    // The impulse response of the plugin against an expected response, 40 frequencies from 20 Hz to 0.45 fs
    void expectResponse(const juce::AudioBuffer<float>& response, double sampleRate, const std::function<std::complex<double>(double)>& expected,
                        const juce::String& what)
    {
        double worstDb = 0.0;
        double worstDegrees = 0.0;
        for (int point = 0; point < 40; ++point)
        {
            const double frequency = 20.0 * std::pow(0.45 * sampleRate / 20.0, point / 39.0);
            const std::complex<double> measured = getSpectrum(response.getReadPointer(1), response.getNumSamples(), frequency, sampleRate);
            const std::complex<double> wanted = expected(frequency);
            if (std::abs(wanted) > 1.0e-3)
            {
                worstDb = std::max(worstDb, std::abs(toDb(std::abs(measured)) - toDb(std::abs(wanted))));
                worstDegrees = std::max(worstDegrees, std::abs(std::arg(measured / wanted)) * 180.0 / kPi);
            }
        }
        logMessage(what + ": largest difference " + juce::String(worstDb, 6) + " dB, " + juce::String(worstDegrees, 5) + " degrees");
        expect(worstDb < 0.001 && worstDegrees < 0.01, what);
    }

    void testEq()
    {
        beginTest("Reference EQ: the impulse response through the plugin equals H(e^jw) of the library (RBJ peak, SVF high shelf, Butterworth, unsupported)");
        struct EqCase
        {
            double sampleRate;
            int algorithm;
            int type;
            const char* frequency;
            const char* gain;
            const char* q;
            const char* order;
            const char* name;
        };
        constexpr int kAlgorithms = 6;
        constexpr int kTypes = 9;
        const EqCase cases[] = {{48000.0, 0, 6, "1000", "6.0", "2.00", "2", "RBJ peak 1 kHz +6 dB Q 2, 48 kHz"},
                                {44100.0, 3, 8, "5000", "-9.0", "0.71", "2", "SVF high shelf 5 kHz -9 dB, 44.1 kHz"},
                                {96000.0, 4, 0, "2000", "0.0", "0.71", "5", "Butterworth LP order 5, 2 kHz, 96 kHz"},
                                {48000.0, 1, 7, "1000", "6.0", "1.00", "2", "Orfanidis low shelf (not offered: passes unchanged)"}};
        for (const EqCase& eqCase : cases)
        {
            std::unique_ptr<juce::AudioPluginInstance> plugin = load("Eq", eqCase.sampleRate);
            if (plugin == nullptr)
            {
                return;
            }
            setChoice(*plugin, "Algorithm", eqCase.algorithm, kAlgorithms);
            setChoice(*plugin, "Type", eqCase.type, kTypes);
            setParameter(*plugin, "Frequency", juce::String(eqCase.frequency) + " Hz");
            setParameter(*plugin, "Gain", juce::String(eqCase.gain) + " dB");
            setParameter(*plugin, "Q", eqCase.q);
            setParameter(*plugin, "Order", eqCase.order);
            juce::AudioBuffer<float> buffer = makeImpulse(2);
            process(*plugin, buffer, eqCase.sampleRate);
            const double fs = eqCase.sampleRate;
            const double f0 = juce::String(eqCase.frequency).getDoubleValue();
            const double gain = juce::String(eqCase.gain).getDoubleValue();
            const double q = juce::String(eqCase.q).getDoubleValue();
            std::function<std::complex<double>(double)> expected;
            if (eqCase.algorithm == 0)
            {
                expected = [=](double f) { return ref::getBiquadResponse(ref::designRbj(ref::FilterType::Peak, fs, f0, gain, q), f, fs); };
            }
            else if (eqCase.algorithm == 3)
            {
                expected = [=](double f) { return ref::getBiquadResponse(ref::designRbj(ref::FilterType::HighShelf, fs, f0, gain, q), f, fs); };
            }
            else if (eqCase.algorithm == 4)
            {
                expected = [=](double f) { return ref::getCascadeResponse(ref::designButterworth(ref::Pass::Low, 5, fs, f0), f, fs); };
            }
            else
            {
                expected = [](double) { return std::complex<double>(1.0, 0.0); };
            }
            expectResponse(buffer, fs, expected, eqCase.name);
            expectEquals(plugin->getLatencySamples(), 0);
        }
    }

    // The value text of a parameter (by its name)
    juce::String getText(juce::AudioPluginInstance& plugin, const juce::String& parameterName)
    {
        for (juce::AudioProcessorParameter* parameter : plugin.getParameters())
        {
            if (parameter->getName(100) == parameterName)
            {
                return parameter->getCurrentValueAsText();
            }
        }
        return {};
    }

    void testEqTypeChoice()
    {
        beginTest("Reference EQ editor: a type the algorithm does not offer is moved to the algorithm's default (Orfanidis -> peak, Butterworth -> low-pass)");
        constexpr int kAlgorithms = 6;
        constexpr int kTypes = 9;
        constexpr int kWaitMs = 300;
        std::unique_ptr<juce::AudioPluginInstance> plugin = load("Eq", 48000.0, true);
        if (plugin == nullptr)
        {
            return;
        }
        std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorAndMakeActive());
        expect(editor != nullptr, "the editor opens");
        setChoice(*plugin, "Type", 0, kTypes);       // low-pass
        setChoice(*plugin, "Algorithm", 1, kAlgorithms); // Orfanidis: peak only
        juce::MessageManager::getInstance()->runDispatchLoopUntil(kWaitMs);
        expectEquals(getText(*plugin, "Type"), juce::String("Peak"));
        setChoice(*plugin, "Algorithm", 4, kAlgorithms); // Butterworth: low-pass and high-pass
        juce::MessageManager::getInstance()->runDispatchLoopUntil(kWaitMs);
        expectEquals(getText(*plugin, "Type"), juce::String("Low-pass"));
        setChoice(*plugin, "Algorithm", 0, kAlgorithms); // RBJ: everything, the type stays
        juce::MessageManager::getInstance()->runDispatchLoopUntil(kWaitMs);
        expectEquals(getText(*plugin, "Type"), juce::String("Low-pass"));
        editor.reset();
    }

    void testNonlinear()
    {
        beginTest("Reference Nonlinear: polynomial harmonics through the plugin equal the closed form; the 8-bit quantizer gives 6.02 N + 1.76 dB");
        constexpr double kSampleRate = 48000.0;
        constexpr double kAmplitude = 0.5;
        std::unique_ptr<juce::AudioPluginInstance> plugin = load("Nonlinear", kSampleRate);
        if (plugin == nullptr)
        {
            return;
        }
        setChoice(*plugin, "Curve", 1, 4);
        setParameter(*plugin, "a2", "0.100");
        setParameter(*plugin, "a3", "0.050");
        juce::AudioBuffer<float> buffer(2, kWindow);
        for (int index = 0; index < kWindow; ++index)
        {
            const float value = static_cast<float>(kAmplitude * std::sin(2.0 * kPi * index / kSamplesPerPeriod));
            buffer.setSample(0, index, value);
            buffer.setSample(1, index, value);
        }
        process(*plugin, buffer, kSampleRate);
        const std::vector<double> expected = ref::getPolynomialHarmonics({0.0, 1.0, 0.1, 0.05}, kAmplitude, 3);
        for (int harmonic = 1; harmonic <= 3; ++harmonic)
        {
            const double frequency = harmonic * kSampleRate / kSamplesPerPeriod;
            const double measured = 2.0 * std::abs(getSpectrum(buffer.getReadPointer(0), kWindow, frequency, kSampleRate)) / kWindow;
            expectWithinAbsoluteError(toDb(measured), toDb(expected[static_cast<size_t>(harmonic)]), 0.001, "harmonic " + juce::String(harmonic));
        }

        std::unique_ptr<juce::AudioPluginInstance> quantizer = load("Nonlinear", kSampleRate);
        if (quantizer == nullptr)
        {
            return;
        }
        setChoice(*quantizer, "Quantizer", 1, 3);
        setParameter(*quantizer, "Bits", "8");
        constexpr int kLength = 1 << 17;
        const double amplitude = 1.0 - 4.0 / 128.0;
        juce::AudioBuffer<float> sine(1, kLength);
        for (int index = 0; index < kLength; ++index)
        {
            sine.setSample(0, index, static_cast<float>(amplitude * std::sin(2.0 * kPi * 997.0 * index / kSampleRate)));
        }
        juce::AudioBuffer<float> input;
        input.makeCopyOf(sine);
        process(*quantizer, sine, kSampleRate);
        double errorPower = 0.0;
        for (int index = 0; index < kLength; ++index)
        {
            const double error = static_cast<double>(sine.getSample(0, index)) - input.getSample(0, index);
            errorPower += error * error;
        }
        const double snr = 10.0 * std::log10(amplitude * amplitude / 2.0 / (errorPower / kLength));
        logMessage("Reference Nonlinear, 8 bits: SNR " + juce::String(snr, 2) + " dB, expected " + juce::String(ref::Quantizer::getExpectedSnrDb(8, false, amplitude), 2));
        expectWithinAbsoluteError(snr, ref::Quantizer::getExpectedSnrDb(8, false, amplitude), 0.3);
    }

    void testUtility()
    {
        beginTest("Reference Utility: gain -6 dB inverted with a Thiran delay of 37.5 samples equals the library; width 0 gives L = R");
        constexpr double kSampleRate = 48000.0;
        std::unique_ptr<juce::AudioPluginInstance> plugin = load("Utility", kSampleRate);
        if (plugin == nullptr)
        {
            return;
        }
        setParameter(*plugin, "Gain", "-6.0 dB");
        setChoice(*plugin, "Polarity", 1, 2);
        setParameter(*plugin, "Delay", "37.50 samples");
        juce::AudioBuffer<float> buffer = makeImpulse(2);
        process(*plugin, buffer, kSampleRate);
        const ref::DirectFormFilter thiran = ref::makeThiranDelay(37.5, 3, kSampleRate);
        const double gain = -std::pow(10.0, -6.0 / 20.0);
        expectResponse(buffer, kSampleRate, [&](double f) { return gain * thiran.getResponse(f); }, "Utility: -6 dB, inverted, Thiran 37.5 samples");
        expectEquals(plugin->getLatencySamples(), 0);

        std::unique_ptr<juce::AudioPluginInstance> mono = load("Utility", kSampleRate);
        if (mono == nullptr)
        {
            return;
        }
        setParameter(*mono, "Width", "0.00");
        juce::AudioBuffer<float> stereo(2, 4800);
        juce::Random random(3);
        for (int index = 0; index < 4800; ++index)
        {
            stereo.setSample(0, index, random.nextFloat() - 0.5f);
            stereo.setSample(1, index, random.nextFloat() - 0.5f);
        }
        process(*mono, stereo, kSampleRate);
        double largest = 0.0;
        for (int index = 0; index < 4800; ++index)
        {
            largest = std::max(largest, static_cast<double>(std::abs(stereo.getSample(0, index) - stereo.getSample(1, index))));
        }
        expect(largest < 1.0e-7, "width 0: L = R, " + juce::String(largest));
    }

    juce::AudioPluginFormatManager m_formats;
    juce::AudioPluginFormatManager m_guiFormats;
};

static ReferencePluginTests referencePluginTests;
