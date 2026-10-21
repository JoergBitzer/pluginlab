#include <cmath>
#include <memory>
#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "BenchmarkProcessor.h"
#include "TestPluginPaths.h"
#include "pluginlab/engine/FixedPlayHead.h"
#include "pluginlab/engine/ImpulseRender.h"
#include "pluginlab/engine/MeasurementEngine.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
namespace engine = pluginlab::engine;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr double kStandardTempo = 120.0;
constexpr double kOddTempo = 97.0;             // the author: an odd second tempo shows a plugin hard-coded to one
constexpr double kSecondsPerMinute = 60.0;
constexpr float kEchoLevel = 0.5f;             // the test plugin: the input plus half of it one beat later
constexpr int kBlocksToRender = 160;           // 1.7 s: longer than a beat at 97 BPM (0.62 s)

int getBeatSamples(double tempo)
{
    return juce::roundToInt(kSecondsPerMinute / tempo * kSampleRate);
}

// the index of the echo: the largest sample after the first one (the dry impulse)
int findEcho(const std::vector<float>& signal, int from)
{
    int index = -1;
    float largest = 0.0f;
    for (int sample = from + 1; sample < static_cast<int>(signal.size()); ++sample)
    {
        if (std::abs(signal[static_cast<size_t>(sample)]) > largest)
        {
            largest = std::abs(signal[static_cast<size_t>(sample)]);
            index = sample;
        }
    }
    return index;
}

// a DAW's play head: a tempo, nothing else
class DawPlayHead : public juce::AudioPlayHead
{
public:
    explicit DawPlayHead(double tempo)
        : m_tempo(tempo)
    {
    }

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo position;
        position.setBpm(m_tempo);
        position.setIsPlaying(true);
        return position;
    }

private:
    double m_tempo;
};
}

// W11.1a: the host's tempo reaches the plugins (the rig of the measurements, the application's engine, the DAW's play head in Jade Benchmark),
// and the impulse response with the whole tail (docs/design/W11-delay-plan.md 4.1)
class TempoTests : public juce::UnitTest
{
public:
    TempoTests()
        : juce::UnitTest("Tempo and the impulse with its tail (W11.1a)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the play head: the tempo, 4/4, the position in beats from the samples processed");
        {
            engine::FixedPlayHead playHead(kOddTempo, kSampleRate);
            playHead.advance(static_cast<int>(kSampleRate)); // one second
            const juce::Optional<juce::AudioPlayHead::PositionInfo> position = playHead.getPosition();
            expect(position.hasValue());
            expectWithinAbsoluteError(*position->getBpm(), kOddTempo, 1.0e-12);
            expectWithinAbsoluteError(*position->getPpqPosition(), kOddTempo / kSecondsPerMinute, 1.0e-9);
            expectEquals(position->getTimeSignature()->numerator, 4);
        }

        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(*m_formatManager, testpaths::getTempoDelayPlugin());
        expect(! scan.descriptions.isEmpty(), "the tempo delay test plugin");
        if (scan.descriptions.isEmpty())
        {
            m_formatManager.reset();
            return;
        }
        const juce::PluginDescription description = scan.descriptions[0];
        const std::vector<float> defaults = engine::getDefaultSetting(*m_formatManager, description, kSampleRate);

        for (const double tempo : {kStandardTempo, kOddTempo})
        {
            beginTest("the impulse with its tail at " + juce::String(tempo, 0) + " BPM: the echo one beat later, then silence");
            engine::ImpulseRenderOptions options;
            options.tempoBpm = tempo;
            const engine::ImpulseRender render = engine::renderImpulse(*m_formatManager, description, defaults, options);
            expect(render.error.isEmpty(), render.error);
            if (render.output.empty())
            {
                continue;
            }
            const std::vector<float>& left = render.output.front();
            const float impulse = static_cast<float>(std::pow(10.0, options.levelDbfs / 20.0));
            expectWithinAbsoluteError(left[0], impulse, 1.0e-6f);
            const int echo = findEcho(left, 0);
            expectEquals(echo, getBeatSamples(tempo), "the echo one beat later");
            expectWithinAbsoluteError(left[static_cast<size_t>(echo)], kEchoLevel * impulse, 1.0e-6f);
            expect(render.decayed && ! render.overflowed, "decayed");
            expectWithinAbsoluteError(render.seconds, static_cast<double>(echo) / kSampleRate + options.holdSeconds, 0.05, "the render ends a second after the echo");
            expectLessThan(render.beforeImpulse.front(), 1.0e-9f, "nothing before the impulse");
        }

        beginTest("a render that does not decay in time is stopped at its limit and says so");
        {
            engine::ImpulseRenderOptions options;
            options.maxSeconds = 0.3; // shorter than the beat at 120 BPM plus the hold time
            const engine::ImpulseRender render = engine::renderImpulse(*m_formatManager, description, defaults, options);
            expect(! render.decayed, "not decayed within 0.3 s");
            expectLessOrEqual(render.seconds, options.maxSeconds + kBlockSize / kSampleRate);
        }

        beginTest("the application's engine: its own play head at 97 BPM reaches the plugin of a slot");
        {
            engine::MeasurementEngine audio;
            audio.prepare(kSampleRate, kBlockSize);
            audio.setTempo(kOddTempo);
            const int slot = addSlot(audio, description);
            audio.setActiveSlot(slot);
            expectEquals(processImpulse(audio, nullptr), getBeatSamples(kOddTempo));
        }

        beginTest("Jade Benchmark: the DAW's play head at 97 BPM reaches the plugin of a slot (not the engine's 120)");
        {
            BenchmarkProcessor processor;
            processor.prepareToPlay(kSampleRate, kBlockSize);
            DawPlayHead daw(kOddTempo);
            processor.setPlayHead(&daw);
            const int slot = addSlot(processor.getEngine(), description);
            processor.slotsChanged();
            processor.getEngine().setActiveSlot(slot);
            expectEquals(processImpulse(processor.getEngine(), &processor), getBeatSamples(kOddTempo));
            processor.setPlayHead(nullptr);
        }
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

private:
    int addSlot(engine::MeasurementEngine& audio, const juce::PluginDescription& description)
    {
        juce::String error;
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin = pluginlab::hosting::HostedPlugin::load(*m_formatManager, description, kSampleRate, kBlockSize, error);
        expect(plugin != nullptr, error);
        if (plugin == nullptr)
        {
            return -1;
        }
        return audio.addSlot(std::move(plugin), description.name, error);
    }

    // an impulse through the audible slot (the DAW's way: processExternal, or the plugin product's processBlock); the distance of its echo
    int processImpulse(engine::MeasurementEngine& audio, juce::AudioProcessor* product)
    {
        std::vector<float> left;
        juce::MidiBuffer midi;
        // the crossfade to the slot first: silence
        for (int block = 0; block < kBlocksToRender / 4; ++block)
        {
            juce::AudioBuffer<float> buffer(2, kBlockSize);
            buffer.clear();
            processBlock(audio, product, buffer, midi);
        }
        int impulseAt = -1;
        for (int block = 0; block < kBlocksToRender; ++block)
        {
            juce::AudioBuffer<float> buffer(2, kBlockSize);
            buffer.clear();
            if (block == 0)
            {
                buffer.setSample(0, 0, 1.0f);
                buffer.setSample(1, 0, 1.0f);
            }
            processBlock(audio, product, buffer, midi);
            for (int sample = 0; sample < kBlockSize; ++sample)
            {
                left.push_back(buffer.getSample(0, sample));
            }
        }
        // the dry impulse comes out after the engine's alignment delay; the echo one beat after it
        float largest = 0.0f;
        for (int sample = 0; sample < static_cast<int>(left.size()); ++sample)
        {
            if (std::abs(left[static_cast<size_t>(sample)]) > largest + 1.0e-6f)
            {
                largest = std::abs(left[static_cast<size_t>(sample)]);
                impulseAt = sample;
            }
        }
        return findEcho(left, impulseAt) - impulseAt;
    }

    static void processBlock(engine::MeasurementEngine& audio, juce::AudioProcessor* product, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        if (product != nullptr)
        {
            product->processBlock(buffer, midi);
            return;
        }
        audio.processExternal(buffer);
    }

    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static TempoTests tempoTests;
