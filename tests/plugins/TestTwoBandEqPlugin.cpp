#include <array>
#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>

// Two RBJ peaking bands for the interactions of W8.7 (docs/eqmodel/interactions.md):
//   PLUGINLAB_TEST_EQ_PARALLEL=0  the bands in series: H = H1 H2, they add exactly in dB (no interaction)
//   PLUGINLAB_TEST_EQ_PARALLEL=1  the bands in parallel, as in many analogue designs: H = 1 + (H1 - 1) + (H2 - 1); where they overlap they
//                                 do not add in dB (two +6 dB peaks at one frequency give 9.54 dB, not 12 dB)
namespace
{
constexpr int kParameterVersion = 1;
constexpr float kMinGainDb = -24.0f;
constexpr float kMaxGainDb = 24.0f;
constexpr float kMinFrequencyHz = 20.0f;
constexpr float kMaxFrequencyHz = 20000.0f;
constexpr float kFrequencySkew = 0.3f;
constexpr float kMinQ = 0.1f;
constexpr float kMaxQ = 10.0f;
constexpr float kDefaultQ = 1.0f;
constexpr std::array<float, 2> kDefaultFrequencyHz = {300.0f, 3000.0f}; // more than a third of an octave apart: two bands in the mapping
constexpr int kBands = 2;
constexpr int kMaximumChannels = 2;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDbPerGainStep = 40.0;

struct Coefficients
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

struct State
{
    double z1 = 0.0;
    double z2 = 0.0;
};

double processBiquad(const Coefficients& c, State& state, double input)
{
    const double output = c.b0 * input + state.z1;
    state.z1 = c.b1 * input - c.a1 * output + state.z2;
    state.z2 = c.b2 * input - c.a2 * output;
    return output;
}

// RBJ audio EQ cookbook, peaking filter
Coefficients designPeak(double gainDb, double frequency, double q, double sampleRate)
{
    const double a = std::pow(10.0, gainDb / kDbPerGainStep);
    const double w0 = 2.0 * kPi * frequency / sampleRate;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha / a;
    Coefficients c;
    c.b0 = (1.0 + alpha * a) / a0;
    c.b1 = -2.0 * std::cos(w0) / a0;
    c.b2 = (1.0 - alpha * a) / a0;
    c.a1 = -2.0 * std::cos(w0) / a0;
    c.a2 = (1.0 - alpha / a) / a0;
    return c;
}
}

class TestTwoBandEqProcessor : public juce::AudioProcessor
{
public:
    TestTwoBandEqProcessor()
        : juce::AudioProcessor(BusesProperties()
                                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                   .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        for (int band = 0; band < kBands; ++band)
        {
            const juce::String number(band + 1);
            m_gain[static_cast<size_t>(band)] = new juce::AudioParameterFloat(juce::ParameterID{"gain" + number, kParameterVersion}, "Gain " + number,
                                                                              juce::NormalisableRange<float>(kMinGainDb, kMaxGainDb, 0.0f), 0.0f);
            m_frequency[static_cast<size_t>(band)] =
                new juce::AudioParameterFloat(juce::ParameterID{"frequency" + number, kParameterVersion}, "Frequency " + number,
                                              juce::NormalisableRange<float>(kMinFrequencyHz, kMaxFrequencyHz, 0.0f, kFrequencySkew),
                                              kDefaultFrequencyHz[static_cast<size_t>(band)]);
            m_q[static_cast<size_t>(band)] = new juce::AudioParameterFloat(juce::ParameterID{"q" + number, kParameterVersion}, "Q " + number,
                                                                           juce::NormalisableRange<float>(kMinQ, kMaxQ, 0.0f), kDefaultQ);
            addParameter(m_gain[static_cast<size_t>(band)]);
            addParameter(m_frequency[static_cast<size_t>(band)]);
            addParameter(m_q[static_cast<size_t>(band)]);
        }
    }

    const juce::String getName() const override
    {
        return JucePlugin_Name;
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        juce::ignoreUnused(samplesPerBlock);
        m_sampleRate = sampleRate;
        for (auto& channel : m_state)
        {
            channel.fill(State());
        }
    }

    void releaseResources() override
    {
    }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        const juce::AudioChannelSet& output = layouts.getMainOutputChannelSet();
        const bool monoOrStereo = output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
        return monoOrStereo && layouts.getMainInputChannelSet() == output;
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ignoreUnused(midi);
        juce::ScopedNoDenormals noDenormals;
        std::array<Coefficients, kBands> coefficients;
        for (size_t band = 0; band < static_cast<size_t>(kBands); ++band)
        {
            coefficients[band] = designPeak(m_gain[band]->get(), m_frequency[band]->get(), m_q[band]->get(), m_sampleRate);
        }
        const int channels = juce::jmin(buffer.getNumChannels(), kMaximumChannels);
        for (int channel = 0; channel < channels; ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            auto& state = m_state[static_cast<size_t>(channel)];
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const double input = data[sample];
                double output = 0.0;
                if (PLUGINLAB_TEST_EQ_PARALLEL)
                {
                    // 1 + (H1 - 1) + (H2 - 1): each band contributes only what it adds to the dry signal
                    output = input;
                    for (size_t band = 0; band < static_cast<size_t>(kBands); ++band)
                    {
                        output += processBiquad(coefficients[band], state[band], input) - input;
                    }
                }
                else
                {
                    output = input;
                    for (size_t band = 0; band < static_cast<size_t>(kBands); ++band)
                    {
                        output = processBiquad(coefficients[band], state[band], output);
                    }
                }
                data[sample] = static_cast<float>(output);
            }
        }
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        return new juce::GenericAudioProcessorEditor(*this);
    }
    bool hasEditor() const override
    {
        return true;
    }
    bool acceptsMidi() const override
    {
        return false;
    }
    bool producesMidi() const override
    {
        return false;
    }
    double getTailLengthSeconds() const override
    {
        return 0.0;
    }
    int getNumPrograms() override
    {
        return 1;
    }
    int getCurrentProgram() override
    {
        return 0;
    }
    void setCurrentProgram(int index) override
    {
        juce::ignoreUnused(index);
    }
    const juce::String getProgramName(int index) override
    {
        juce::ignoreUnused(index);
        return {};
    }
    void changeProgramName(int index, const juce::String& name) override
    {
        juce::ignoreUnused(index, name);
    }
    void getStateInformation(juce::MemoryBlock& destData) override
    {
        juce::ignoreUnused(destData);
    }
    void setStateInformation(const void* data, int size) override
    {
        juce::ignoreUnused(data, size);
    }

private:
    std::array<juce::AudioParameterFloat*, kBands> m_gain{};
    std::array<juce::AudioParameterFloat*, kBands> m_frequency{};
    std::array<juce::AudioParameterFloat*, kBands> m_q{};
    double m_sampleRate = 48000.0;
    std::array<std::array<State, kBands>, kMaximumChannels> m_state{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestTwoBandEqProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestTwoBandEqProcessor();
}
