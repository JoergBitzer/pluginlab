#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>

// An RBJ peaking EQ for the tests of the fingerprint, with faults that can be built in:
//   PLUGINLAB_TEST_EQ_FS_BUG=1       designs the filter for 44.1 kHz whatever sample rate the host runs at (the fault of the own PeakEQ)
//   PLUGINLAB_TEST_EQ_PREPARE_BUG=1  prepareToPlay resets the filter to hard-coded values and a parameter is read again only when its
//                                    value changes (the fault of the PeakEqualizer template)
//   PLUGINLAB_TEST_EQ_ENABLE_SWITCH=1  the band has a switch "Enabled" that is off by default (like the bands of Venn Audio Free EQ)
//   PLUGINLAB_TEST_EQ_DRIVE=1        an "analogue" EQ for the parameter survey: "Drive" (tanh(g x) / g before the band, off at 0 dB), "Output" (a
//                                    level after it) and "Display" (a switch of the editor that changes nothing in the audio)
//   PLUGINLAB_TEST_EQ_LIMITED=1      the frequency knob ends at 5 kHz (the attribution of the matcher, W9.5: a knob at the end of its range)
//   PLUGINLAB_TEST_EQ_KNOB_FAULTS=1  knobs that show something else than they do (the calibration of W8.6): the band sits 5 % above the frequency
//                                    shown, the "Q" knob is a bandwidth in octaves (range 0.1 ... 3), the gain is half the dB shown
namespace
{
constexpr int kParameterVersion = 1;
constexpr float kMinGainDb = -24.0f;
constexpr float kMaxGainDb = 24.0f;
constexpr float kMinFrequencyHz = 20.0f;
constexpr float kMaxFrequencyHz = PLUGINLAB_TEST_EQ_LIMITED ? 5000.0f : 20000.0f;
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
constexpr float kMaxDriveDb = 24.0f;
constexpr float kMaxOutputDb = 12.0f;
constexpr double kFaultFrequencyFactor = 1.05;
constexpr double kFaultGainFactor = 0.5;
constexpr float kFaultMaxBandwidth = 3.0f;
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
        m_q = new juce::AudioParameterFloat(juce::ParameterID{"q", kParameterVersion}, "Q", juce::NormalisableRange<float>(kMinQ, PLUGINLAB_TEST_EQ_KNOB_FAULTS ? kFaultMaxBandwidth : kMaxQ, 0.0f), 1.0f);
        addParameter(m_gain);
        addParameter(m_frequency);
        addParameter(m_q);
        if (PLUGINLAB_TEST_EQ_ENABLE_SWITCH)
        {
            m_enabled = new juce::AudioParameterBool(juce::ParameterID{"enabled", kParameterVersion}, "Enabled", false);
            addParameter(m_enabled);
        }
        if (PLUGINLAB_TEST_EQ_DRIVE)
        {
            m_drive = new juce::AudioParameterFloat(juce::ParameterID{"drive", kParameterVersion}, "Drive",
                                                    juce::NormalisableRange<float>(0.0f, kMaxDriveDb, 0.0f), 0.0f);
            m_output = new juce::AudioParameterFloat(juce::ParameterID{"output", kParameterVersion}, "Output",
                                                     juce::NormalisableRange<float>(-kMaxOutputDb, kMaxOutputDb, 0.0f), 0.0f);
            m_display = new juce::AudioParameterChoice(juce::ParameterID{"display", kParameterVersion}, "Display", juce::StringArray{"Dark", "Light", "Vintage"}, 0);
            addParameter(m_drive);
            addParameter(m_output);
            addParameter(m_display);
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
        if (m_enabled != nullptr && ! m_enabled->get())
        {
            return; // the band is off: the audio passes unchanged
        }
        const int channels = juce::jmin(buffer.getNumChannels(), kMaximumChannels);
        const double drive = m_drive != nullptr ? juce::Decibels::decibelsToGain(static_cast<double>(m_drive->get())) : 1.0;
        const double output = m_output != nullptr ? juce::Decibels::decibelsToGain(static_cast<double>(m_output->get())) : 1.0;
        for (int channel = 0; channel < channels; ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            Filter& filter = m_filters[static_cast<size_t>(channel)];
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                double input = data[sample];
                if (drive > 1.0)
                {
                    input = std::tanh(drive * input) / drive; // unity gain for small signals: the drive adds harmonics, not level
                }
                const double filtered = m_b0 * input + filter.z1;
                filter.z1 = m_b1 * input - m_a1 * filtered + filter.z2;
                filter.z2 = m_b2 * input - m_a2 * filtered;
                data[sample] = static_cast<float>(output * filtered);
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
        return "Default"; // a name: the VST3 validator of pluginval 2.x rejects an empty one
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
        if (PLUGINLAB_TEST_EQ_KNOB_FAULTS)
        {
            frequency *= kFaultFrequencyFactor;
            gainDb *= kFaultGainFactor;
            const double ratio = std::pow(2.0, q); // the knob is a bandwidth in octaves: Q = sqrt(2^N) / (2^N - 1)
            q = std::sqrt(ratio) / (ratio - 1.0);
        }
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
    juce::AudioParameterBool* m_enabled = nullptr;
    juce::AudioParameterFloat* m_drive = nullptr;
    juce::AudioParameterFloat* m_output = nullptr;
    juce::AudioParameterChoice* m_display = nullptr;
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
