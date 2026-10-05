#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>

// An RBJ peaking EQ for the tests of the fingerprint, with faults that can be built in:
//   PLUGINLAB_TEST_EQ_FS_BUG=1       designs the filter for 44.1 kHz whatever sample rate the host runs at (the fault of the own PeakEQ)
//   PLUGINLAB_TEST_EQ_PREPARE_BUG=1  prepareToPlay resets the filter to hard-coded values and a parameter is read again only when its
//                                    value changes (the fault of the PeakEqualizer template)
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
constexpr double kBuggySampleRate = 44100.0;
constexpr double kHardCodedGainDb = 20.0;
constexpr double kHardCodedFrequencyHz = 4000.0;
constexpr double kHardCodedQ = 9.0;
constexpr int kMaximumChannels = 2;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDbPerGainStep = 40.0;
}

class TestEqProcessor : public juce::AudioProcessor
{
public:
    TestEqProcessor()
        : juce::AudioProcessor(BusesProperties()
                                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                   .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        m_gain = new juce::AudioParameterFloat(juce::ParameterID{"gain", kParameterVersion}, "Gain",
                                               juce::NormalisableRange<float>(kMinGainDb, kMaxGainDb, 0.0f), 0.0f);
        m_frequency = new juce::AudioParameterFloat(juce::ParameterID{"frequency", kParameterVersion}, "Frequency",
                                                    juce::NormalisableRange<float>(kMinFrequencyHz, kMaxFrequencyHz, 0.0f, kFrequencySkew), 1000.0f);
        m_q = new juce::AudioParameterFloat(juce::ParameterID{"q", kParameterVersion}, "Q", juce::NormalisableRange<float>(kMinQ, kMaxQ, 0.0f), 1.0f);
        addParameter(m_gain);
        addParameter(m_frequency);
        addParameter(m_q);
    }

    const juce::String getName() const override
    {
        return JucePlugin_Name;
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        juce::ignoreUnused(samplesPerBlock);
        m_sampleRate = sampleRate;
        if (PLUGINLAB_TEST_EQ_FS_BUG)
        {
            m_sampleRate = kBuggySampleRate;
        }
        for (Filter& filter : m_filters)
        {
            filter = Filter();
        }
        if (PLUGINLAB_TEST_EQ_PREPARE_BUG)
        {
            design(kHardCodedGainDb, kHardCodedFrequencyHz, kHardCodedQ);
            m_lastGain = m_gain->get(); // what the plugin has seen: only a change is read again
            m_lastFrequency = m_frequency->get();
            m_lastQ = m_q->get();
            return;
        }
        design(m_gain->get(), m_frequency->get(), m_q->get());
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
        updateDesign();
        const int channels = juce::jmin(buffer.getNumChannels(), kMaximumChannels);
        for (int channel = 0; channel < channels; ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            Filter& filter = m_filters[static_cast<size_t>(channel)];
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const double input = data[sample];
                const double output = m_b0 * input + filter.z1;
                filter.z1 = m_b1 * input - m_a1 * output + filter.z2;
                filter.z2 = m_b2 * input - m_a2 * output;
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
    struct Filter
    {
        double z1 = 0.0;
        double z2 = 0.0;
    };

    // reads the parameters (or, with the prepare fault, only the ones that changed)
    void updateDesign()
    {
        const double gain = m_gain->get();
        const double frequency = m_frequency->get();
        const double q = m_q->get();
        if (PLUGINLAB_TEST_EQ_PREPARE_BUG)
        {
            const bool changed = gain != m_lastGain || frequency != m_lastFrequency || q != m_lastQ;
            if (! changed)
            {
                return;
            }
            m_lastGain = gain;
            m_lastFrequency = frequency;
            m_lastQ = q;
        }
        design(gain, frequency, q);
    }

    // RBJ audio EQ cookbook, peaking filter
    void design(double gainDb, double frequency, double q)
    {
        const double a = std::pow(10.0, gainDb / kDbPerGainStep);
        const double w0 = 2.0 * kPi * frequency / m_sampleRate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha / a;
        m_b0 = (1.0 + alpha * a) / a0;
        m_b1 = -2.0 * std::cos(w0) / a0;
        m_b2 = (1.0 - alpha * a) / a0;
        m_a1 = -2.0 * std::cos(w0) / a0;
        m_a2 = (1.0 - alpha / a) / a0;
    }

    juce::AudioParameterFloat* m_gain = nullptr;
    juce::AudioParameterFloat* m_frequency = nullptr;
    juce::AudioParameterFloat* m_q = nullptr;
    double m_sampleRate = kBuggySampleRate;
    double m_b0 = 1.0;
    double m_b1 = 0.0;
    double m_b2 = 0.0;
    double m_a1 = 0.0;
    double m_a2 = 0.0;
    double m_lastGain = 0.0;
    double m_lastFrequency = 0.0;
    double m_lastQ = 0.0;
    Filter m_filters[kMaximumChannels];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestEqProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestEqProcessor();
}
