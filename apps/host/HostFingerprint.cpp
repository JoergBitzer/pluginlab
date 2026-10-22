#include "HostFingerprint.h"

#include <cstdio>

#include "HostSettings.h"
#include "pluginlab/PluginLabVersion.h"
#include "pluginlab/engine/Calibration.h"
#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/engine/Interactions.h"
#include "pluginlab/engine/PluginMatch.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/ParameterSurvey.h"
#include "pluginlab/engine/PluginMeasurement.h"
#include "pluginlab/engine/ReportText.h"
#include "pluginlab/measure/NullTest.h"
#include "pluginlab/measure/Spectrogram.h"
#include "pluginlab/ui/Logo.h"
#include "pluginlab/ui/ResponsePlotImage.h"
#include "pluginlab/ui/SpectrogramImage.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::host
{
namespace
{
constexpr double kLevelFloorDb = -60.0; // the level of a matched target: only where the master is above this
constexpr double kFingerprintShare = 0.75; // of a report: the fingerprint; the measurement units the rest
constexpr double kSurveyShare = 0.97;      // of a mapping: the survey; the proposal the rest
constexpr double kCalibrationSurveyShare = 0.5; // of a calibration without a mapping: the survey first
constexpr double kCalibrationShare = 0.8;       // of the rest: the calibration; the interactions of the bands the remainder
// the spectrogram of the fingerprint report: a synchronized sweep through the plugin at its defaults (docs/measurements/spectrogram.md)
constexpr double kSpectrogramStartHz = 20.0;
constexpr double kSpectrogramStopHz = 20000.0;
constexpr double kSpectrogramSweepSeconds = 3.0;
constexpr double kSpectrogramLevelDbfs = -6.0;
constexpr double kSpectrogramTailSeconds = 0.5; // silence after the sweep: latency, ringing, noise
constexpr int kSpectrogramFftSize = 2048;          // 43 ms: fine time steps where the sweep is fast
constexpr int kSpectrogramLowFftSize = 8192;       // 5.9 Hz per bin: the resolution the logarithmic axis needs at the low end
constexpr double kSpectrogramOverlap = 0.75;       // of the short frame: hop 512 samples (10.7 ms) for both
constexpr double kSpectrogramFloorDb = -120.0;
constexpr double kSpectrogramCeilingDb = 0.0;

double getMeasurementRate()
{
    return pluginlab::engine::MeasurementOptions().sampleRate;
}

// the sweep through the plugin at a setting as a spectrogram picture (a PNG file); false if nothing came out or the file cannot be written
bool writeSweepPicture(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, int channels, const std::vector<float>& setting,
                       const juce::String& title, const juce::File& file, pluginlab::measure::Spectrogram& spectrogram,
                       pluginlab::measure::SpectrogramSettings& settings)
{
    const double sampleRate = getMeasurementRate();
    const std::vector<float> sweep =
        pluginlab::measure::makeSynchronizedSweep(sampleRate, kSpectrogramStartHz, kSpectrogramStopHz, kSpectrogramSweepSeconds, kSpectrogramLevelDbfs);
    const int length = static_cast<int>(sweep.size()) + static_cast<int>(kSpectrogramTailSeconds * sampleRate);
    juce::AudioBuffer<float> input(std::max(channels, 1), length);
    input.clear();
    for (int channel = 0; channel < input.getNumChannels(); ++channel)
    {
        input.copyFrom(channel, 0, sweep.data(), static_cast<int>(sweep.size()));
    }
    const juce::AudioBuffer<float> output = pluginlab::engine::makePluginDevice(formatManager, description, setting)(input, sampleRate);
    if (output.getNumChannels() == 0 || output.getNumSamples() == 0)
    {
        return false;
    }
    const std::vector<float> left(output.getReadPointer(0), output.getReadPointer(0) + output.getNumSamples());
    settings.fftSize = kSpectrogramFftSize;
    settings.lowFrequencyFftSize = kSpectrogramLowFftSize;
    settings.overlap = kSpectrogramOverlap;
    spectrogram = pluginlab::measure::computeSpectrogram(left, sampleRate, settings);
    pluginlab::ui::SpectrogramImageOptions options;
    options.floorDb = kSpectrogramFloorDb;
    options.ceilingDb = kSpectrogramCeilingDb;
    options.title = title;
    return pluginlab::ui::writeSpectrogramPng(spectrogram, options, file);
}

// the sweep through the plugin at the setting A (defaults) and at B (the parameters that change the audio moved, as in the fingerprint): two
// pictures next to the report; the Markdown section that shows them (empty if no picture could be written)
juce::String writeSweepSpectrograms(juce::AudioPluginFormatManager& formatManager, const pluginlab::engine::PluginFingerprint& fingerprint,
                                    const juce::File& reportFile, int index)
{
    const juce::PluginDescription& description = fingerprint.description;
    const double sampleRate = getMeasurementRate();
    const std::vector<float> settingA = pluginlab::engine::getDefaultSetting(formatManager, description, sampleRate);
    std::vector<float> settingB = settingA;
    juce::StringArray changes;
    for (const pluginlab::engine::SettingEntry& entry : fingerprint.settingB)
    {
        if (entry.index >= 0 && entry.index < static_cast<int>(settingB.size()))
        {
            settingB[static_cast<size_t>(entry.index)] = entry.valueB;
            changes.add(entry.name + " = " + pluginlab::engine::tidyNumberText(entry.textB));
        }
    }
    juce::String stemSuffix; // the first spectrogram without a number, the others with theirs
    if (index > 0)
    {
        stemSuffix = "_" + juce::String(index);
    }
    else
    {
        stemSuffix = juce::String();
    }
    const juce::String stem = reportFile.getFileNameWithoutExtension() + "_sweep_spectrogram" + stemSuffix;
    pluginlab::measure::Spectrogram spectrogram;
    pluginlab::measure::SpectrogramSettings settings;
    const juce::String nameA = stem + ".png";
    if (! writeSweepPicture(formatManager, description, fingerprint.measuredChannels, settingA, description.name + ": synchronized sweep at A (the defaults), output channel 1",
                            reportFile.getSiblingFile(nameA), spectrogram, settings))
    {
        return {};
    }
    juce::String nameB;
    if (! changes.isEmpty())
    {
        pluginlab::measure::Spectrogram spectrogramB;
        pluginlab::measure::SpectrogramSettings settingsB;
        nameB = stem + "_b.png";
        if (! writeSweepPicture(formatManager, description, fingerprint.measuredChannels, settingB, description.name + ": synchronized sweep at B, output channel 1",
                                reportFile.getSiblingFile(nameB), spectrogramB, settingsB))
        {
            nameB.clear();
        }
    }
    const auto describeFrame = [sampleRate](int size) {
        return juce::String(size) + " samples (" + juce::String(1000.0 * size / sampleRate, 1) + " ms, " + juce::String(sampleRate / size, 1) + " Hz per bin)";
    };
    juce::String text;
    text << "## Spectrogram of the sweep\n\n";
    text << "At A (the defaults):\n\n![Spectrogram of the sweep at A](" << nameA << ")\n\n";
    if (nameB.isNotEmpty())
    {
        text << "At B (" << changes.joinIntoString(", ") << "):\n\n![Spectrogram of the sweep at B](" << nameB << ")\n\n";
    }
    else
    {
        juce::String reason;
        if (changes.isEmpty())
        {
            reason = juce::String("no parameter changes the audio, so B is A");
        }
        else
        {
            reason = juce::String("nothing came out");
        }
        text << "No picture at B: " << reason << ".\n\n";
    }
    text << "| Parameter | Value |\n|---|---|\n";
    text << "| signal | synchronized exponential sweep (Novak et al. 2015), " << juce::roundToInt(kSpectrogramStartHz) << " Hz ... "
         << juce::roundToInt(kSpectrogramStopHz / 1000.0) << " kHz in " << juce::String(kSpectrogramSweepSeconds, 1) << " s, peak "
         << juce::String(kSpectrogramLevelDbfs, 0) << " dBFS, the same on every input channel, then " << juce::String(kSpectrogramTailSeconds, 1)
         << " s of silence |\n";
    text << "| plugin | A: the default setting; B: the parameters that change the audio moved as in the fingerprint (section \"The settings A and B\"); "
         << juce::String(sampleRate / 1000.0, 1) << " kHz; shown: output channel 1 |\n";
    text << "| analysis | two short-time Fourier transforms, " << pluginlab::measure::getSpectrogramWindowName(settings.window) << " window, "
         << describeFrame(spectrogram.fftSize) << " and " << describeFrame(spectrogram.shortFftSize) << ", centred on the same instants, hop "
         << spectrogram.hopSize << " samples (" << juce::String(1000.0 * spectrogram.hopSize / sampleRate, 1)
         << " ms); each bin shows the smaller of the two levels (the long transform smears in time, the short one in frequency) |\n";
    text << "| level | dB re a full-scale sine (a sine of amplitude 1 reads 0 dB); colour map plasma from " << juce::roundToInt(kSpectrogramFloorDb)
         << " dB (dark) to " << juce::roundToInt(kSpectrogramCeilingDb) << " dB (yellow) |\n";
    text << "| axes | frequency logarithmic, " << juce::roundToInt(kSpectrogramStartHz) << " Hz ... " << juce::roundToInt(kSpectrogramStopHz / 1000.0)
         << " kHz (a pixel row shows the largest bin in it); time in s |\n\n";
    text << "How to read it: the sweep is the bright rising line. Harmonics are fainter lines above it at 2, 3, ... times its frequency (a non-linear "
            "plugin); lines that fold back down at the top are aliasing; a uniform glow is noise; anything after the end of the sweep is the "
            "plugin's latency, ringing or its own signal. A filter shows as a line that gets darker where it attenuates. B often shows what A "
            "hides: a drive or a curve that does nothing at the defaults.\n";
    return text;
}

// writes the progress file and prints each new step; removes the file when it goes out of scope
class ProgressWriter
{
public:
    explicit ProgressWriter(const juce::File& reportFile)
        : m_file(getProgressFile(reportFile))
    {
    }

    ~ProgressWriter()
    {
        m_file.deleteFile();
    }

    void operator()(double fraction, const juce::String& step)
    {
        m_file.replaceWithText(juce::String(juce::jlimit(0.0, 1.0, fraction), 3) + "\n" + step + "\n");
        if (step != m_lastStep)
        {
            std::printf("[%3d %%] %s\n", juce::roundToInt(100.0 * fraction), step.toRawUTF8());
            std::fflush(stdout);
            m_lastStep = step;
        }
    }

private:
    juce::File m_file;
    juce::String m_lastStep;
};
}


bool writeEchoComparison(const juce::File& pluginFileA, const juce::File& pluginFileB, const juce::File& reportFile)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scanA = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFileA);
    const hosting::PluginScanResult scanB = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFileB);
    if (scanA.descriptions.isEmpty() || scanB.descriptions.isEmpty())
    {
        return false;
    }
    const juce::PluginDescription& a = scanA.descriptions[0];
    const juce::PluginDescription& b = scanB.descriptions[0];
    const double rate = getMeasurementRate();
    const juce::String report = pluginlab::engine::compareEchoResponses(formatManager, a, pluginlab::engine::getDefaultSetting(formatManager, a, rate), b,
                                                                       pluginlab::engine::getDefaultSetting(formatManager, b, rate), rate);
    return reportFile.replaceWithText(pluginlab::engine::signReport(report, "PluginLabHost, echo comparison"));
}

bool writeMeasurementReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);
    juce::String report;
    juce::Array<juce::var> results; // <report>.json: the rows for programs (the Expert page's test compares them)
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        const pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measurePlugin(
            formatManager, description, pluginlab::engine::getDefaultSetting(formatManager, description, getMeasurementRate()), {});
        results.add(pluginlab::engine::createMeasurementJson(summary));
        report += pluginlab::engine::signReport("# Measurements: " + description.name + " " + description.version + "\n\n"
                                                    + pluginlab::engine::createMeasurementReport(summary),
                                                "PluginLabHost, measurement")
                  + "\n";
    }
    if (report.isEmpty())
    {
        return false;
    }
    pluginlab::ui::writeLogo(reportFile.getParentDirectory(), pluginlab::engine::kLogoFileName);
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("plugins", results);
    juce::File(reportFile.getFullPathName() + ".json").replaceWithText(juce::JSON::toString(juce::var(root.release())));
    return reportFile.replaceWithText(report);
}

bool writeFingerprintReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);
    if (scan.descriptions.isEmpty())
    {
        return false;
    }
    juce::String warning;
    const pluginlab::engine::FingerprintSettings settings =
        pluginlab::engine::FingerprintSettings::loadOrCreate(pluginlab::engine::FingerprintSettings::getDefaultFile(), warning);
    ProgressWriter progress(reportFile);
    juce::String report;
    int measured = 0;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        double share = 1.0; // the measurement units take the rest
        if (settings.measurements)
        {
            share = kFingerprintShare;
        }
        else
        {
            share = 1.0;
        }
        pluginlab::engine::PluginFingerprint fingerprint = pluginlab::engine::measureFingerprint(
            formatManager, description, settings, [&progress, share](double fraction, const juce::String& step) { progress(share * fraction, "fingerprint: " + step); });
        fingerprint.settingsWarning = warning;
        report += pluginlab::engine::createReport(fingerprint) + "\n";
        if (fingerprint.loaded)
        {
            progress(share, "spectrogram of the sweep");
            report += writeSweepSpectrograms(formatManager, fingerprint, reportFile, measured) + "\n";
        }
        if (settings.measurements)
        {
            // the measurement units (W7.11) on the default setting of the plugin
            progress(kFingerprintShare, "measurements (level, frequency response, distortion, noise)");
            const pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measurePlugin(
                formatManager, description, pluginlab::engine::getDefaultSetting(formatManager, description, getMeasurementRate()), {});
            report += pluginlab::engine::createMeasurementReport(summary) + "\n";
            fingerprint.echoes = pluginlab::engine::summarizeEchoes(summary);
            juce::StringArray rows;
            for (const pluginlab::engine::MeasurementRow& row : summary.rows)
            {
                if (row.unit == "echoes")
                {
                    rows.add(row.quantity + ": " + row.text);
                }
            }
            fingerprint.echoesDetail = rows.joinIntoString("; ");
            fingerprint.reverb = pluginlab::engine::summarizeReverb(summary);
            juce::StringArray reverbRows;
            for (const pluginlab::engine::MeasurementRow& row : summary.rows)
            {
                if (row.unit == "reverb")
                {
                    reverbRows.add(row.quantity + ": " + row.text);
                }
            }
            fingerprint.reverbDetail = reverbRows.joinIntoString("; ");
        }
        // the single results for programs (the Developer page): <report>.json, or <report>.<n>.json for further plugins of the file
        juce::String jsonPath = reportFile.getFullPathName() + ".json";
        if (measured > 0)
        {
            jsonPath = reportFile.getFullPathName() + "." + juce::String(measured) + ".json";
        }
        juce::File(jsonPath).replaceWithText(pluginlab::engine::createSummaryJson(fingerprint));
        ++measured;
    }
    if (report.isEmpty())
    {
        return false; // the identifier is not in the file
    }
    pluginlab::ui::writeLogo(reportFile.getParentDirectory(), pluginlab::engine::kLogoFileName);
    return reportFile.replaceWithText(pluginlab::engine::signReport(report, "PluginLabHost, fingerprint and measurement"));
}

bool writeMappingProposal(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);
    const juce::File folder = HostSettings().getMappingFolder();
    ProgressWriter progress(reportFile);
    juce::String report;
    bool written = true;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        const pluginlab::engine::SurveySubject subject = pluginlab::engine::makePluginSubject(formatManager, description, getMeasurementRate());
        pluginlab::engine::SurveySettings settings;
        settings.sampleRate = getMeasurementRate();
        progress(0.0, "survey: the default setting");
        const pluginlab::engine::SurveyResult survey = pluginlab::engine::surveyParameters(subject, settings, [&progress, &subject](int index, int total) {
            if (index < total)
            {
                progress(kSurveyShare * index / std::max(total, 1),
                         "survey: parameter " + juce::String(index + 1) + " of " + juce::String(total) + ", " + subject.parameters[static_cast<size_t>(index)].name);
            }
            else
            {
                progress(kSurveyShare, "survey: switches once more with a band that acts");
            }
        });
        progress(kSurveyShare, "proposing the mapping");
        const pluginlab::engine::Mapping proposal = pluginlab::engine::proposeMapping(
            subject, survey, pluginlab::engine::makePluginIdentity(description), "PluginLabHost " + juce::String(pluginlab::getVersionString()));
        juce::File target = folder.getChildFile(pluginlab::engine::getMappingFileName(description.name));
        juce::String note = "Proposal written to `" + target.getFullPathName() + "`.";
        juce::String error;
        if (const std::optional<pluginlab::engine::Mapping> existing = pluginlab::engine::Mapping::read(target, error); existing && existing->confirmedBy.isNotEmpty())
        {
            target = target.withFileExtension(".proposal.json");
            const juce::StringArray differences = pluginlab::engine::compareMappings(proposal, *existing);
            juce::String comparison;
            if (differences.isEmpty())
            {
                comparison = juce::String("It agrees with the confirmed one.");
            }
            else
            {
                comparison = "Differences:\n\n- " + differences.joinIntoString("\n- ");
            }
            note = "A confirmed mapping exists (" + existing->confirmedBy + "); the proposal was written to `" + target.getFullPathName() + "`. "
                   + comparison;
        }
        written = proposal.write(target) && written;
        report += pluginlab::engine::signReport("# Parameter survey and mapping: " + description.name + " " + description.version + "\n\n"
                                                    + pluginlab::engine::createSurveyReport(survey, "Parameter survey") + "\n"
                                                    + pluginlab::engine::createMappingReport(proposal) + "\n" + note + "\n\n"
                                                    + pluginlab::engine::createMappingReportFooter(),
                                                "PluginLabHost, mapping")
                  + "\n";
    }
    if (report.isEmpty())
    {
        return false;
    }
    pluginlab::ui::writeLogo(reportFile.getParentDirectory(), pluginlab::engine::kLogoFileName);
    return reportFile.replaceWithText(report) && written;
}

bool writeCalibrationReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);
    const juce::File folder = HostSettings().getMappingFolder();
    ProgressWriter progress(reportFile);
    juce::String report;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        const pluginlab::engine::SurveySubject subject = pluginlab::engine::makePluginSubject(formatManager, description, getMeasurementRate());
        const juce::File file = folder.getChildFile(pluginlab::engine::getMappingFileName(description.name));
        juce::String error;
        std::optional<pluginlab::engine::Mapping> mapping = pluginlab::engine::Mapping::read(file, error);
        juce::String note;
        double share = 0.0; // of the progress that the survey took
        if (mapping)
        {
            juce::String status;
            if (mapping->confirmedBy.isEmpty())
            {
                status = juce::String("a proposal, not confirmed");
            }
            else
            {
                status = "confirmed by " + mapping->confirmedBy;
            }
            note = "Mapping: `" + file.getFullPathName() + "` (" + status + ").";
        }
        else
        {
            // no mapping yet: the survey proposes one (written as --map would)
            share = kCalibrationSurveyShare;
            pluginlab::engine::SurveySettings settings;
            settings.sampleRate = getMeasurementRate();
            const pluginlab::engine::SurveyResult survey = pluginlab::engine::surveyParameters(subject, settings, [&progress, &subject](int index, int total) {
                if (index < total)
                {
                    progress(kCalibrationSurveyShare * index / std::max(total, 1),
                             "survey (no mapping yet): parameter " + juce::String(index + 1) + " of " + juce::String(total) + ", " + subject.parameters[static_cast<size_t>(index)].name);
                }
            });
            mapping = pluginlab::engine::proposeMapping(subject, survey, pluginlab::engine::makePluginIdentity(description),
                                                         "PluginLabHost " + juce::String(pluginlab::getVersionString()));
            mapping->write(file);
            note = "No mapping existed: the survey proposed one, written to `" + file.getFullPathName() + "` (not confirmed; check it with \"View mapping\").";
        }
        // the calibration band by band, then whether the bands add (W8.7)
        const double calibrationEnd = share + (1.0 - share) * kCalibrationShare;
        const pluginlab::engine::CalibrationSettings settings;
        const pluginlab::engine::CalibrationResult result = pluginlab::engine::calibrate(subject, *mapping, settings, [&progress, share, calibrationEnd](double fraction, const juce::String& step) {
            progress(share + (calibrationEnd - share) * fraction, "calibration: " + step);
        });
        const pluginlab::engine::InteractionResult interactions =
            pluginlab::engine::measureInteractions(subject, *mapping, {}, [&progress, calibrationEnd](double fraction, const juce::String& step) {
                progress(calibrationEnd + (1.0 - calibrationEnd) * fraction, "interactions: " + step);
            });
        report += pluginlab::engine::signReport("# Calibration: " + description.name + " " + description.version + "\n\n" + note + "\n\n"
                                                    + pluginlab::engine::createCalibrationReport(result) + "\n"
                                                    + pluginlab::engine::createInteractionReport(interactions) + "\n"
                                                    + pluginlab::engine::createCalibrationReportFooter(settings),
                                                "PluginLabHost, calibration")
                  + "\n";
    }
    if (report.isEmpty())
    {
        return false;
    }
    pluginlab::ui::writeLogo(reportFile.getParentDirectory(), pluginlab::engine::kLogoFileName);
    return reportFile.replaceWithText(report);
}

namespace
{
// normalised values by parameter name ({"Gain": 0.75, ...}) into a setting; false (with the name) if a name is not a parameter of the plugin
bool applyNamedValues(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const juce::var& values,
                      std::vector<float>& setting, juce::String& error)
{
    const juce::DynamicObject* object = values.getDynamicObject();
    if (object == nullptr)
    {
        return true;
    }
    juce::String instanceError;
    const std::unique_ptr<juce::AudioPluginInstance> instance = formatManager.createPluginInstance(description, getMeasurementRate(), 512, instanceError);
    if (instance == nullptr)
    {
        error = instanceError;
        return false;
    }
    for (const juce::NamedValueSet::NamedValue& entry : object->getProperties())
    {
        bool found = false;
        for (int index = 0; index < instance->getParameters().size() && index < static_cast<int>(setting.size()); ++index)
        {
            if (instance->getParameters()[index]->getName(64) == entry.name.toString())
            {
                setting[static_cast<size_t>(index)] = static_cast<float>(static_cast<double>(entry.value));
                found = true;
            }
        }
        if (! found)
        {
            error = description.name + " has no parameter \"" + entry.name.toString() + "\"";
            return false;
        }
    }
    return true;
}

// normalised values by position ([0.5, 0.25, ...]: the whole setting of a hosted instance, as the Matcher page sends it)
void applyValues(const juce::var& values, std::vector<float>& setting)
{
    if (const juce::Array<juce::var>* list = values.getArray())
    {
        for (int index = 0; index < list->size() && index < static_cast<int>(setting.size()); ++index)
        {
            setting[static_cast<size_t>(index)] = static_cast<float>(static_cast<double>((*list)[index]));
        }
    }
}

std::optional<juce::PluginDescription> scanFirst(juce::AudioPluginFormatManager& formatManager, const juce::String& path)
{
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, juce::File(path));
    if (scan.descriptions.isEmpty())
    {
        return std::nullopt;
    }
    return scan.descriptions[0];
}
}

bool writeMatchReport(const juce::File& jobFile, const juce::File& reportFile)
{
    const juce::var job = juce::JSON::parse(jobFile.loadFileAsString());
    juce::String error;
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    pluginlab::engine::MatchMaster master;
    const juce::var masterJob = job.getProperty("master", {});
    if (const juce::Array<juce::var>* curve = masterJob.getProperty("curve", {}).getArray())
    {
        pluginlab::eqmodel::EqModel model;
        for (const juce::var& entry : *curve)
        {
            pluginlab::eqmodel::Band band;
            const juce::String family = entry.getProperty("family", "RBJ cookbook").toString();
            const juce::String type = entry.getProperty("type", "peak").toString();
            for (const pluginlab::eqmodel::DesignFamily candidate : pluginlab::eqmodel::getAllFamilies())
            {
                if (family.equalsIgnoreCase(pluginlab::eqmodel::getFamilyName(candidate)))
                {
                    band.family = candidate;
                }
            }
            for (int candidate = 0; candidate <= static_cast<int>(pluginlab::reference::FilterType::HighShelf); ++candidate)
            {
                if (type.equalsIgnoreCase(pluginlab::reference::getFilterTypeName(static_cast<pluginlab::reference::FilterType>(candidate))))
                {
                    band.type = static_cast<pluginlab::reference::FilterType>(candidate);
                }
            }
            band.frequencyHz = entry.getProperty("frequency", 1000.0);
            band.gainDb = entry.getProperty("gain", 0.0);
            band.q = entry.getProperty("q", 0.71);
            band.order = entry.getProperty("order", 2);
            model.bands.push_back(band);
        }
        master.curve = model;
    }
    else if (const std::optional<juce::PluginDescription> plugin = scanFirst(formatManager, masterJob.getProperty("plugin", "").toString()))
    {
        master.plugin = plugin;
        master.setting = pluginlab::engine::getDefaultSetting(formatManager, *plugin, getMeasurementRate());
        applyValues(masterJob.getProperty("values", {}), master.setting);
        if (! applyNamedValues(formatManager, *plugin, masterJob.getProperty("setting", {}), master.setting, error))
        {
            std::fprintf(stderr, "%s\n", error.toRawUTF8());
            return false;
        }
    }
    else
    {
        std::fprintf(stderr, "the job has no master (\"plugin\" or \"curve\")\n");
        return false;
    }

    std::vector<pluginlab::engine::MatchTarget> targets;
    const juce::File mappingFolder = HostSettings().getMappingFolder();
    if (const juce::Array<juce::var>* list = job.getProperty("targets", {}).getArray())
    {
        for (const juce::var& entry : *list)
        {
            const std::optional<juce::PluginDescription> plugin = scanFirst(formatManager, entry.getProperty("plugin", "").toString());
            if (! plugin)
            {
                std::fprintf(stderr, "cannot scan %s\n", entry.getProperty("plugin", "").toString().toRawUTF8());
                return false;
            }
            pluginlab::engine::MatchTarget target;
            target.plugin = *plugin;
            target.start = pluginlab::engine::getDefaultSetting(formatManager, *plugin, getMeasurementRate());
            applyValues(entry.getProperty("values", {}), target.start);
            if (! applyNamedValues(formatManager, *plugin, entry.getProperty("start", {}), target.start, error))
            {
                std::fprintf(stderr, "%s\n", error.toRawUTF8());
                return false;
            }
            juce::String instanceError;
            const std::unique_ptr<juce::AudioPluginInstance> instance = formatManager.createPluginInstance(*plugin, getMeasurementRate(), 512, instanceError);
            if (const juce::Array<juce::var>* indices = entry.getProperty("releaseIndices", {}).getArray())
            {
                for (const juce::var& index : *indices)
                {
                    target.released.push_back(static_cast<int>(index));
                }
            }
            if (const juce::Array<juce::var>* release = entry.getProperty("release", {}).getArray())
            {
                for (const juce::var& name : *release)
                {
                    for (int index = 0; instance != nullptr && index < instance->getParameters().size(); ++index)
                    {
                        if (instance->getParameters()[index]->getName(64) == name.toString())
                        {
                            target.released.push_back(index);
                        }
                    }
                }
            }
            target.modelStart = entry.getProperty("modelStart", false);
            if (target.modelStart)
            {
                juce::String mappingError;
                target.mapping = pluginlab::engine::Mapping::read(mappingFolder.getChildFile(pluginlab::engine::getMappingFileName(plugin->name)), mappingError);
            }
            targets.push_back(target);
        }
    }
    pluginlab::engine::PluginMatchSettings settings;
    settings.probe.sampleRate = getMeasurementRate();
    settings.probe.lowestHz = job.getProperty("lowestHz", 20.0);
    settings.probe.highestHz = job.getProperty("highestHz", 20000.0);
    settings.budget.seconds = job.getProperty("seconds", 15.0);
    if (const std::optional<pluginlab::match::MatchTolerance> tolerance = pluginlab::match::parseTolerance(job.getProperty("tolerance", "strict").toString()))
    {
        settings.tolerance = *tolerance;
    }
    ProgressWriter progress(reportFile);
    const pluginlab::engine::PluginMatchResult result =
        pluginlab::engine::matchPlugins(formatManager, master, targets, settings, [&progress](double fraction, const juce::String& step) { progress(fraction, step); });
    // the found settings for programs (the Matcher page applies them): <report>.json
    juce::Array<juce::var> found;
    for (size_t index = 0; index < result.targets.size(); ++index)
    {
        const pluginlab::engine::TargetMatchResult& item = result.targets[index];
        auto object = std::make_unique<juce::DynamicObject>();
        juce::String pluginIdentifier;
        if (index < targets.size())
        {
            pluginIdentifier = targets[index].plugin.createIdentifierString();
        }
        else
        {
            pluginIdentifier = juce::String();
        }
        object->setProperty("plugin", pluginIdentifier);
        object->setProperty("name", item.name);
        object->setProperty("refused", item.refused);
        object->setProperty("reason", item.reason);
        object->setProperty("matched", ! item.refused && item.optimizer.withinTolerance);
        object->setProperty("largestDb", item.optimizer.largest);
        object->setProperty("worstHz", item.worstHz);
        if (! item.causes.empty())
        {
            object->setProperty("cause", pluginlab::engine::getMatchCauseName(item.causes.front().cause));
            object->setProperty("evidence", item.causes.front().evidence);
        }
        // the level of the matched target against the master (mean dB difference where the master is above -60 dB): the trim for listening
        double levelSum = 0.0;
        int levelCount = 0;
        for (size_t point = 0; point < item.targetFeatures.size() && point < result.masterFeatures.size(); ++point)
        {
            if (result.masterFeatures[point] > kLevelFloorDb)
            {
                levelSum += item.targetFeatures[point] - result.masterFeatures[point];
                ++levelCount;
            }
        }
        double levelDb = 0.0;
        if (levelCount > 0)
        {
            levelDb = levelSum / levelCount;
        }
        else
        {
            levelDb = 0.0;
        }
        object->setProperty("levelDb", levelDb);
        juce::Array<juce::var> setting;
        for (const float value : item.setting)
        {
            setting.add(value);
        }
        object->setProperty("setting", setting);
        juce::Array<juce::var> released;
        for (const pluginlab::engine::ReleasedValue& value : item.values)
        {
            released.add(value.index);
        }
        object->setProperty("released", released);
        found.add(juce::var(object.release()));
    }
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("targets", found);
    juce::File(reportFile.getFullPathName() + ".json").replaceWithText(juce::JSON::toString(juce::var(root.release())));
    // the curves of the master and the matched targets, and their deviations
    juce::String plotName;
    if (result.error.isEmpty() && ! result.targets.empty())
    {
        std::vector<pluginlab::ui::PlotCurve> curves{{"master", juce::Colour(0xffe0e0e0), result.masterFeatures, 3.0f}};
        std::vector<pluginlab::ui::PlotCurve> differences;
        int colour = 0;
        for (const pluginlab::engine::TargetMatchResult& item : result.targets)
        {
            if (item.refused || item.targetFeatures.size() != result.masterFeatures.size())
            {
                continue;
            }
            std::vector<double> difference(item.targetFeatures.size());
            for (size_t index = 0; index < difference.size(); ++index)
            {
                difference[index] = item.targetFeatures[index] - result.masterFeatures[index];
            }
            curves.push_back({item.name, pluginlab::ui::getPlotColour(colour), item.targetFeatures, 2.0f});
            differences.push_back({item.name, pluginlab::ui::getPlotColour(colour), difference, 2.0f});
            ++colour;
        }
        pluginlab::ui::ResponsePlotOptions options;
        options.title = "Master and matched targets (magnitude, dB)";
        plotName = reportFile.getFileNameWithoutExtension() + "_curves.png";
        if (! pluginlab::ui::writeResponsePlotPng(result.frequencies, curves, differences, options, reportFile.getSiblingFile(plotName)))
        {
            plotName.clear();
        }
    }
    pluginlab::ui::writeLogo(reportFile.getParentDirectory(), pluginlab::engine::kLogoFileName);
    return reportFile.replaceWithText(pluginlab::engine::signReport("# Match\n\n" + pluginlab::engine::createPluginMatchReport(result, plotName) + "\n"
                                                                        + pluginlab::engine::createPluginMatchReportFooter(),
                                                                    "PluginLabHost, match"));
}

bool writeExpertReport(const juce::File& jobFile, const juce::File& reportFile)
{
    const juce::var job = juce::JSON::parse(jobFile.loadFileAsString());
    const juce::Array<juce::var>* slots = job.getProperty("slots", {}).getArray();
    if (slots == nullptr || slots->isEmpty())
    {
        std::fprintf(stderr, "the job has no slots\n");
        return false;
    }
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    pluginlab::engine::MeasurementOptions options;
    options.sampleRate = job.getProperty("sampleRate", getMeasurementRate());
    options.linearity = job.getProperty("linearity", false);
    if (const juce::Array<juce::var>* units = job.getProperty("units", {}).getArray())
    {
        for (const juce::var& unit : *units)
        {
            options.units.add(unit.toString());
        }
    }
    struct Measured
    {
        juce::String name;
        std::optional<juce::PluginDescription> plugin;   // none: the dry slot
        std::vector<float> setting;
    };
    std::vector<Measured> measured;
    for (const juce::var& entry : *slots)
    {
        Measured item;
        item.name = entry.getProperty("name", "").toString();
        const juce::String path = entry.getProperty("plugin", "").toString();
        if (path.isNotEmpty())
        {
            item.plugin = scanFirst(formatManager, path);
            if (! item.plugin)
            {
                std::fprintf(stderr, "cannot scan %s\n", path.toRawUTF8());
                return false;
            }
            item.setting = pluginlab::engine::getDefaultSetting(formatManager, *item.plugin, options.sampleRate);
            applyValues(entry.getProperty("values", {}), item.setting);
        }
        measured.push_back(item);
    }
    // a device per slot: the plugin with its setting (the careful delivery), or the dry path
    const auto makeDevice = [&formatManager](const Measured& item) -> pluginlab::measure::Device {
        if (item.plugin)
        {
            return pluginlab::engine::makePluginDevice(formatManager, *item.plugin, item.setting);
        }
        return [](const juce::AudioBuffer<float>& input, double) { return input; };
    };
    const auto channelsOf = [&formatManager, &options](const Measured& item) {
        if (item.plugin)
        {
            return pluginlab::engine::getPluginChannels(formatManager, *item.plugin, options.sampleRate);
        }
        return 2;
    };

    ProgressWriter progress(reportFile);
    const int nullMaster = job.getProperty("nullMaster", -1);
    int nulls = 0;
    if (nullMaster >= 0 && nullMaster < static_cast<int>(measured.size()))
    {
        nulls = static_cast<int>(measured.size()) - 1;
    }
    else
    {
        nulls = 0;
    }
    const double total = static_cast<double>(measured.size() + static_cast<size_t>(nulls));
    juce::String report = "# Expert measurements\n\n";
    report << "The measurement units of W7 (AES17 defaults, docs/measurements/) on the slots of the page Expert with their settings as they were when "
              "the measurement started, each in a fresh instance per render (the careful delivery of W7.11); the system class first (W8.1).\n\n";
    juce::Array<juce::var> results;
    for (size_t index = 0; index < measured.size(); ++index)
    {
        const Measured& item = measured[index];
        pluginlab::engine::MeasurementOptions slotOptions = options;
        slotOptions.progress = [&progress, index, total, &item](double fraction, const juce::String& unit) {
            progress((static_cast<double>(index) + fraction) / total, item.name + ": " + unit);
        };
        const pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measureDevice(makeDevice(item), channelsOf(item), item.name, slotOptions);
        report << "## " << item.name << "\n\n" << pluginlab::engine::createMeasurementReport(summary).fromFirstOccurrenceOf("\n\n", false, false) << "\n";
        juce::var result = pluginlab::engine::createMeasurementJson(summary);
        result.getDynamicObject()->setProperty("name", item.name);
        results.add(result);
    }
    juce::Array<juce::var> nullResults;
    if (nulls > 0)
    {
        const Measured& master = measured[static_cast<size_t>(nullMaster)];
        report << "## Null test against " << master.name << "\n\n";
        report << "Noise at -20 dBFS through the master and each slot; each output aligned to the master's (delay, also fractional, and gain, also "
                  "inverted) and subtracted (docs/measurements/null-test.md). The depth is the residual re the master in 20 Hz ... 20 kHz: the lower, "
                  "the closer. Not meaningful for time-variant slots.\n\n";
        report << "| slot | null depth | without alignment | delay | gain | inverted |\n|---|---|---|---|---|---|\n";
        int done = 0;
        for (size_t index = 0; index < measured.size(); ++index)
        {
            if (static_cast<int>(index) == nullMaster)
            {
                continue;
            }
            const Measured& item = measured[index];
            progress((static_cast<double>(measured.size()) + done++) / total, "null test: " + item.name);
            pluginlab::measure::NullTestSettings settings;
            settings.sampleRate = options.sampleRate;
            settings.channels = std::min(channelsOf(master), channelsOf(item));
            const pluginlab::measure::NullTestResult result = pluginlab::measure::measureNull(makeDevice(master), makeDevice(item), settings);
            if (result.channels.empty())
            {
                continue;
            }
            const pluginlab::measure::ChannelNull& first = result.channels.front();
            juce::String invertedText;
            if (first.inverted)
            {
                invertedText = "yes";
            }
            else
            {
                invertedText = "no";
            }
            report << "| " << item.name << " | " << juce::String(first.nullDepthDb, 1) << " dB | " << juce::String(first.unalignedNullDb, 1) << " dB | "
                   << juce::String(first.delaySamples, 2) << " samples | " << juce::String(first.gainDb, 2) << " dB | " << invertedText
                   << " |\n";
            auto entry = std::make_unique<juce::DynamicObject>();
            entry->setProperty("name", item.name);
            entry->setProperty("nullDepthDb", first.nullDepthDb);
            entry->setProperty("unalignedNullDb", first.unalignedNullDb);
            entry->setProperty("delaySamples", first.delaySamples);
            entry->setProperty("gainDb", first.gainDb);
            entry->setProperty("inverted", first.inverted);
            nullResults.add(juce::var(entry.release()));
        }
    }
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("slots", results);
    juce::var nullMasterName;
    if (nullMaster >= 0)
    {
        nullMasterName = juce::var(measured[static_cast<size_t>(std::min(nullMaster, static_cast<int>(measured.size()) - 1))].name);
    }
    else
    {
        nullMasterName = juce::var();
    }
    root->setProperty("nullMaster", nullMasterName);
    root->setProperty("nulls", nullResults);
    juce::File(reportFile.getFullPathName() + ".json").replaceWithText(juce::JSON::toString(juce::var(root.release())));
    pluginlab::ui::writeLogo(reportFile.getParentDirectory(), pluginlab::engine::kLogoFileName);
    return reportFile.replaceWithText(pluginlab::engine::signReport(report, "PluginLabHost, expert measurements"));
}
}
