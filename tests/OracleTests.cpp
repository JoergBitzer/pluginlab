#include <cmath>
#include <complex>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/reference/Delays.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/signals/SweptSine.h"

namespace
{
namespace ref = pluginlab::reference;
namespace sig = pluginlab::signals;

constexpr double kPi = 3.14159265358979323846;
constexpr double kAbsentDb = -150.0;          // harmonics the oracle measured below this are not compared (numerical floor)
constexpr double kGroupDelayStepHz = 0.01;    // central difference of the phase for the group delay

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-300));
}

std::vector<double> toVector(const juce::var& array)
{
    std::vector<double> values;
    if (const juce::Array<juce::var>* items = array.getArray())
    {
        for (const juce::var& item : *items)
        {
            values.push_back(static_cast<double>(item));
        }
    }
    return values;
}

ref::FilterType getFilterType(const juce::String& filterName, bool& found)
{
    for (const ref::FilterType type : {ref::FilterType::LowPass, ref::FilterType::HighPass, ref::FilterType::BandPass, ref::FilterType::BandPassUnity,
                                       ref::FilterType::Notch, ref::FilterType::AllPass, ref::FilterType::Peak, ref::FilterType::LowShelf,
                                       ref::FilterType::HighShelf})
    {
        if (filterName == ref::getFilterTypeName(type))
        {
            found = true;
            return type;
        }
    }
    found = false;
    return ref::FilterType::Peak;
}

// The largest differences between the C++ answer and the oracle
struct Deviation
{
    double decibels = 0.0;
    double degrees = 0.0;

    void add(std::complex<double> cpp, std::complex<double> oracle)
    {
        decibels = std::max(decibels, std::abs(toDb(std::abs(cpp)) - toDb(std::abs(oracle))));
        degrees = std::max(degrees, std::abs(std::arg(cpp / oracle)) * 180.0 / kPi);
    }
};
}

// W6.5: the C++ references against the oracle files of the Python prototype (tests/oracle/*.json, written by
// measurement_tool/tools/export_oracle.py). Every file states its tolerance; a file of an unknown kind fails (it needs C++ code here).
class OracleTests : public juce::UnitTest
{
public:
    OracleTests()
        : juce::UnitTest("Oracle", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("every oracle file: the C++ answer within the tolerance of the file");
        const juce::Array<juce::File> files = juce::File(PLUGINLAB_ORACLE_DIR).findChildFiles(juce::File::findFiles, false, "*.json");
        expect(files.size() >= 11, "oracle files found: " + juce::String(files.size()));
        for (const juce::File& file : files)
        {
            const juce::var oracle = juce::JSON::parse(file);
            expect(oracle.isObject(), file.getFileName() + " is not JSON");
            if (!oracle.isObject())
            {
                continue;
            }
            check(oracle);
        }
    }

private:
    void check(const juce::var& oracle)
    {
        const juce::String caseName = oracle["case"].toString();
        const juce::String kind = oracle["kind"].toString();
        const juce::String type = oracle["processor"]["type"].toString();
        if (kind == "response" && type == "rbj")
        {
            checkRbj(oracle, caseName);
        }
        else if (kind == "response" && type == "butterworth")
        {
            checkButterworth(oracle, caseName);
        }
        else if (kind == "response" && type == "thiran")
        {
            checkThiran(oracle, caseName);
        }
        else if (kind == "measured_magnitude")
        {
            checkMeasuredMagnitude(oracle, caseName);
        }
        else if (kind == "harmonics")
        {
            checkHarmonics(oracle, caseName);
        }
        else if (kind == "snr")
        {
            checkSnr(oracle, caseName);
        }
        else if (kind == "sweep_measurement")
        {
            checkSweep(oracle, caseName);
        }
        else
        {
            expect(false, caseName + ": no C++ comparison for kind '" + kind + "', type '" + type + "'");
        }
    }

    // A response case: the C++ answer at the frequencies of the file against real/imag of the oracle
    void expectResponse(const juce::var& oracle, const juce::String& caseName, const std::function<std::complex<double>(double)>& cpp)
    {
        const juce::var& response = oracle["response"];
        const std::vector<double> frequencies = toVector(response["frequency_hz"]);
        const std::vector<double> real = toVector(response["real"]);
        const std::vector<double> imag = toVector(response["imag"]);
        Deviation deviation;
        for (size_t index = 0; index < frequencies.size(); ++index)
        {
            deviation.add(cpp(frequencies[index]), std::complex<double>(real[index], imag[index]));
        }
        const double toleranceDb = oracle["tolerance"]["magnitude_db"];
        const double toleranceDegrees = oracle["tolerance"]["phase_deg"];
        logMessage(caseName + ": " + juce::String(deviation.decibels, 2, true) + " dB, " + juce::String(deviation.degrees, 2, true) + " degrees (tolerance "
                   + juce::String(toleranceDb) + " dB, " + juce::String(toleranceDegrees) + " degrees)");
        expect(deviation.decibels < toleranceDb && deviation.degrees < toleranceDegrees, caseName);
    }

    ref::BiquadCoefficients designFromProcessor(const juce::var& processor, const juce::String& caseName)
    {
        bool found = false;
        const ref::FilterType type = getFilterType(processor["filter"].toString(), found);
        expect(found, caseName + ": unknown filter " + processor["filter"].toString());
        return ref::designRbj(type, processor["sample_rate"], processor["frequency_hz"], processor["gain_db"], processor["q"]);
    }

    void checkRbj(const juce::var& oracle, const juce::String& caseName)
    {
        const juce::var& processor = oracle["processor"];
        const double sampleRate = processor["sample_rate"];
        const ref::BiquadCoefficients section = designFromProcessor(processor, caseName);
        expectResponse(oracle, caseName, [&](double f) { return ref::getBiquadResponse(section, f, sampleRate); });
    }

    void checkButterworth(const juce::var& oracle, const juce::String& caseName)
    {
        const juce::var& processor = oracle["processor"];
        const double sampleRate = processor["sample_rate"];
        ref::Pass pass = ref::Pass::Low;
        if (processor["pass"].toString() == "high")
        {
            pass = ref::Pass::High;
        }
        const std::vector<ref::BiquadCoefficients> sections = ref::designButterworth(pass, processor["order"], sampleRate, processor["frequency_hz"]);
        expectResponse(oracle, caseName, [&](double f) { return ref::getCascadeResponse(sections, f, sampleRate); });
    }

    void checkThiran(const juce::var& oracle, const juce::String& caseName)
    {
        const juce::var& processor = oracle["processor"];
        const double sampleRate = processor["sample_rate"];
        const ref::DirectFormFilter thiran = ref::makeThiranDelay(processor["samples"], processor["order"], sampleRate);
        expectResponse(oracle, caseName, [&](double f) { return thiran.getResponse(f); });

        // the coefficients and the group delay
        const juce::var& coefficients = oracle["coefficients"];
        expectEquals(thiran.getDelaySamples(), static_cast<int>(coefficients["integer_delay"]), caseName + ": integer delay");
        const std::vector<double> numerator = toVector(coefficients["numerator"]);
        const std::vector<double> denominator = toVector(coefficients["denominator"]);
        double largest = 0.0;
        for (size_t index = 0; index < numerator.size() && index < thiran.getNumerator().size(); ++index)
        {
            largest = std::max(largest, std::abs(numerator[index] - thiran.getNumerator()[index]));
            largest = std::max(largest, std::abs(denominator[index] - thiran.getDenominator()[index]));
        }
        expect(largest < 1.0e-12, caseName + ": coefficients " + juce::String(largest));
        const std::vector<double> frequencies = toVector(oracle["group_delay_samples"]["frequency_hz"]);
        const std::vector<double> delays = toVector(oracle["group_delay_samples"]["value"]);
        const double tolerance = oracle["tolerance"]["group_delay_samples"];
        for (size_t index = 0; index < frequencies.size(); ++index)
        {
            const double f = frequencies[index];
            const double phaseStep = std::arg(thiran.getResponse(f + kGroupDelayStepHz) / thiran.getResponse(f - kGroupDelayStepHz));
            const double groupDelay = -phaseStep / (2.0 * kPi * 2.0 * kGroupDelayStepHz / sampleRate);
            expectWithinAbsoluteError(groupDelay, delays[index], tolerance, caseName + ": group delay at " + juce::String(f) + " Hz");
        }
    }

    void checkMeasuredMagnitude(const juce::var& oracle, const juce::String& caseName)
    {
        const juce::var& processor = oracle["processor"];
        const double sampleRate = processor["sample_rate"];
        const ref::BiquadCoefficients section = designFromProcessor(processor, caseName);
        const std::vector<double> frequencies = toVector(oracle["measured"]["frequency_hz"]);
        const std::vector<double> measured = toVector(oracle["measured"]["magnitude_db"]);
        double largest = 0.0;
        for (size_t index = 0; index < frequencies.size(); ++index)
        {
            largest = std::max(largest, std::abs(toDb(std::abs(ref::getBiquadResponse(section, frequencies[index], sampleRate))) - measured[index]));
        }
        const double tolerance = oracle["tolerance"]["magnitude_db"];
        logMessage(caseName + ": the prototype's measurement against the exact C++ response: " + juce::String(largest, 4) + " dB (tolerance "
                   + juce::String(tolerance) + " dB)");
        expect(largest < tolerance, caseName);
    }

    void checkHarmonics(const juce::var& oracle, const juce::String& caseName)
    {
        const std::vector<double> coefficients = toVector(oracle["processor"]["coefficients"]);
        const double amplitude = std::pow(10.0, static_cast<double>(oracle["signal"]["level_dbfs_peak"]) / 20.0);
        const std::vector<double> closed = ref::getPolynomialHarmonics(coefficients, amplitude, 5);
        const std::vector<double> harmonics = toVector(oracle["harmonics_db_re_fundamental"]["harmonic"]);
        const std::vector<double> values = toVector(oracle["harmonics_db_re_fundamental"]["value"]);
        const double tolerance = oracle["tolerance"]["harmonic_db"];
        double distortion = 0.0;
        double largest = 0.0;
        for (size_t index = 0; index < harmonics.size(); ++index)
        {
            const size_t harmonic = static_cast<size_t>(harmonics[index]);
            const double ratio = closed[harmonic] / closed[1];
            distortion += ratio * ratio;
            if (values[index] > kAbsentDb)
            {
                largest = std::max(largest, std::abs(toDb(ratio) - values[index]));
            }
            else
            {
                expect(toDb(ratio) < kAbsentDb, caseName + ": harmonic " + juce::String(static_cast<int>(harmonic)) + " should be absent");
            }
        }
        const double thd = 10.0 * std::log10(distortion);
        logMessage(caseName + ": harmonics " + juce::String(largest, 5) + " dB, THD " + juce::String(thd, 4) + " dB against " + juce::String(static_cast<double>(oracle["thd_db"]), 4)
                   + " dB (closed form against the prototype's measurement)");
        expect(largest < tolerance, caseName + ": harmonics");
        expectWithinAbsoluteError(thd, static_cast<double>(oracle["thd_db"]), static_cast<double>(oracle["tolerance"]["thd_db"]), caseName + ": THD");
    }

    void checkSnr(const juce::var& oracle, const juce::String& caseName)
    {
        const juce::var& signal = oracle["signal"];
        const double sampleRate = oracle["processor"]["sample_rate"];
        const double frequency = signal["frequency_hz"];
        const double amplitude = signal["amplitude"];
        const int length = signal["length"];
        ref::Quantizer quantizer(oracle["processor"]["bits"], false);
        juce::AudioBuffer<float> buffer(1, length);
        for (int index = 0; index < length; ++index)
        {
            buffer.setSample(0, index, static_cast<float>(amplitude * std::sin(2.0 * kPi * frequency * index / sampleRate)));
        }
        juce::AudioBuffer<float> input;
        input.makeCopyOf(buffer);
        quantizer.process(buffer);
        double errorPower = 0.0;
        for (int index = 0; index < length; ++index)
        {
            const double error = static_cast<double>(buffer.getSample(0, index)) - input.getSample(0, index);
            errorPower += error * error;
        }
        const double snr = 10.0 * std::log10(amplitude * amplitude / 2.0 / (errorPower / length));
        logMessage(caseName + ": SNR " + juce::String(snr, 4) + " dB against " + juce::String(static_cast<double>(oracle["snr_db"]), 4) + " dB (formula "
                   + juce::String(static_cast<double>(oracle["formula_snr_db"]), 4) + " dB, expected " + juce::String(ref::Quantizer::getExpectedSnrDb(oracle["processor"]["bits"], false, amplitude), 4) + " dB)");
        expectWithinAbsoluteError(snr, static_cast<double>(oracle["snr_db"]), static_cast<double>(oracle["tolerance"]["snr_db"]), caseName);
    }

    void checkSweep(const juce::var& oracle, const juce::String& caseName)
    {
        const juce::var& signal = oracle["signal"];
        const juce::var& processor = oracle["processor"];
        sig::SweptSineSettings settings;
        settings.sampleRate = processor["sample_rate"];
        settings.startHz = signal["start_hz"];
        settings.stopHz = signal["stop_hz"];
        settings.approximateSeconds = signal["approximate_seconds"];
        settings.levelDbfsPeak = signal["level_dbfs_peak"];
        settings.preSilenceSeconds = signal["pre_seconds"];
        settings.postSilenceSeconds = signal["post_seconds"];
        const sig::SweptSine sweep = sig::makeSweptSine(settings, 1);
        expectWithinAbsoluteError(sweep.rate, static_cast<double>(signal["rate_seconds"]), 1.0e-12, caseName + ": L");

        ref::BiquadCascade filter({designFromProcessor(processor, caseName)}, settings.sampleRate);
        juce::AudioBuffer<float> buffer;
        buffer.makeCopyOf(sweep.signal);
        filter.process(buffer);
        const std::vector<float> response(buffer.getReadPointer(0), buffer.getReadPointer(0) + buffer.getNumSamples());
        const std::vector<float> impulse = sig::deconvolveSweptSine(sweep, response);
        const int length = signal["impulse_response_length"];
        expectResponse(oracle, caseName, [&](double f)
                       {
                           std::complex<double> sum = 0.0;
                           for (int index = 0; index < length; ++index)
                           {
                               sum += static_cast<double>(impulse[static_cast<size_t>(index)]) * std::polar(1.0, -2.0 * kPi * f * index / settings.sampleRate);
                           }
                           return sum;
                       });
    }
};

static OracleTests oracleTests;
