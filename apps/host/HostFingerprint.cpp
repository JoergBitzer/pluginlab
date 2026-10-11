#include "HostFingerprint.h"

#include <cstdio>

#include "HostSettings.h"
#include "pluginlab/PluginLabVersion.h"
#include "pluginlab/engine/Calibration.h"
#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/ParameterSurvey.h"
#include "pluginlab/engine/PluginMeasurement.h"
#include "pluginlab/engine/ReportText.h"
#include "pluginlab/ui/Logo.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::host
{
namespace
{
constexpr double kFingerprintShare = 0.75; // of a report: the fingerprint; the measurement units the rest
constexpr double kSurveyShare = 0.97;      // of a mapping: the survey; the proposal the rest
constexpr double kCalibrationSurveyShare = 0.5; // of a calibration without a mapping: the survey first

double getMeasurementRate()
{
    return pluginlab::engine::MeasurementOptions().sampleRate;
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
