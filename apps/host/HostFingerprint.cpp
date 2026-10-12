#include "HostFingerprint.h"

#include <cstdio>

#include "HostSettings.h"
#include "pluginlab/PluginLabVersion.h"
#include "pluginlab/engine/Calibration.h"
#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/ParameterSurvey.h"
#include "pluginlab/engine/PluginMeasurement.h"
#include "pluginlab/engine/ReportText.h"
#include "pluginlab/measure/Spectrogram.h"
#include "pluginlab/ui/Logo.h"
#include "pluginlab/ui/SpectrogramImage.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::host
{
namespace
{
constexpr double kFingerprintShare = 0.75; // of a report: the fingerprint; the measurement units the rest
constexpr double kSurveyShare = 0.97;      // of a mapping: the survey; the proposal the rest
constexpr double kCalibrationSurveyShare = 0.5; // of a calibration without a mapping: the survey first
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

// a parameter's text without false precision: a number with more than two decimals rounded ("7678.3115234" -> "7678", "7.5249996" -> "7.52")
juce::String tidyParameterText(const juce::String& text)
{
    const juce::String trimmed = text.trim();
    int start = 0;
    while (start < trimmed.length() && ! juce::CharacterFunctions::isDigit(trimmed[start]))
    {
        ++start;
    }
    int end = start;
    int point = -1;
    while (end < trimmed.length() && (juce::CharacterFunctions::isDigit(trimmed[end]) || (trimmed[end] == '.' && point < 0)))
    {
        point = trimmed[end] == '.' ? end : point;
        ++end;
    }
    if (point < 0 || end - point - 1 <= 2)
    {
        return trimmed;
    }
    const double value = trimmed.substring(start, end).getDoubleValue();
    juce::String number = std::abs(value) >= 100.0 ? juce::String(juce::roundToInt(value)) : juce::String(value, 2);
    if (number.containsChar('.'))
    {
        number = number.trimCharactersAtEnd("0").trimCharactersAtEnd(".");
    }
    return trimmed.substring(0, start) + number + trimmed.substring(end);
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
            changes.add(entry.name + " = " + tidyParameterText(entry.textB));
        }
    }
    const juce::String stem = reportFile.getFileNameWithoutExtension() + "_sweep_spectrogram" + (index > 0 ? "_" + juce::String(index) : juce::String());
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
        text << "No picture at B: " << (changes.isEmpty() ? juce::String("no parameter changes the audio, so B is A") : juce::String("nothing came out")) << ".\n\n";
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

juce::File getProgressFile(const juce::File& reportFile)
{
    return juce::File(reportFile.getFullPathName() + ".progress");
}

JobProgress readProgressFile(const juce::File& progressFile)
{
    JobProgress progress;
    const juce::StringArray lines = juce::StringArray::fromLines(progressFile.loadFileAsString());
    if (lines.size() >= 2 && lines[0].containsOnly("0123456789."))
    {
        progress.fraction = lines[0].getDoubleValue();
        progress.step = lines[1];
    }
    return progress;
}

bool writeMeasurementReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);
    juce::String report;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        const pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measurePlugin(
            formatManager, description, pluginlab::engine::getDefaultSetting(formatManager, description, getMeasurementRate()), {});
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
        const double share = settings.measurements ? kFingerprintShare : 1.0; // the measurement units take the rest
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
            note = "A confirmed mapping exists (" + existing->confirmedBy + "); the proposal was written to `" + target.getFullPathName() + "`. "
                   + (differences.isEmpty() ? juce::String("It agrees with the confirmed one.") : "Differences:\n\n- " + differences.joinIntoString("\n- "));
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
            note = "Mapping: `" + file.getFullPathName() + "` (" + (mapping->confirmedBy.isEmpty() ? juce::String("a proposal, not confirmed") : "confirmed by " + mapping->confirmedBy) + ").";
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
        const pluginlab::engine::CalibrationSettings settings;
        const pluginlab::engine::CalibrationResult result = pluginlab::engine::calibrate(subject, *mapping, settings, [&progress, share](double fraction, const juce::String& step) {
            progress(share + (1.0 - share) * fraction, "calibration: " + step);
        });
        report += pluginlab::engine::signReport("# Calibration: " + description.name + " " + description.version + "\n\n" + note + "\n\n"
                                                    + pluginlab::engine::createCalibrationReport(result) + "\n"
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
}
