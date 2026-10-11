#include "HostFingerprint.h"

#include "HostSettings.h"
#include "pluginlab/PluginLabVersion.h"
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
double getMeasurementRate()
{
    return pluginlab::engine::MeasurementOptions().sampleRate;
}
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
    juce::String report;
    int measured = 0;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        pluginlab::engine::PluginFingerprint fingerprint = pluginlab::engine::measureFingerprint(formatManager, description, settings);
        fingerprint.settingsWarning = warning;
        report += pluginlab::engine::createReport(fingerprint) + "\n";
        if (settings.measurements)
        {
            // the measurement units (W7.11) on the default setting of the plugin
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
        const pluginlab::engine::SurveyResult survey = pluginlab::engine::surveyParameters(subject, settings);
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
                                                    + pluginlab::engine::createMappingReport(proposal) + "\n" + note + "\n",
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
}
