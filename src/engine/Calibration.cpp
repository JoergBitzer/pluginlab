#include "pluginlab/engine/Calibration.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/eqmodel/BandFit.h"
#include "pluginlab/engine/ReportText.h"
#include "pluginlab/measure/FrequencyResponse.h"

namespace pluginlab::engine
{
namespace
{
constexpr double kLowestHz = 20.0;          // the band fit sees 20 Hz ... 20 kHz
constexpr double kHighestHz = 20000.0;
constexpr double kGridPerOctave = 12.0;
constexpr int kFitBands = 3;
constexpr double kOrderTolerance = 0.5;
constexpr double kUnitSlopeTolerance = 0.01; // a gain slope within 1 % of 1 is an offset
constexpr double kKnownRates[] = {44100.0, 48000.0, 88200.0, 96000.0};
constexpr double kRateAgreement = 0.01;     // design rates estimated at the host rates agree within 1 %

bool isGainType(reference::FilterType type)
{
    return type == reference::FilterType::Peak || type == reference::FilterType::LowShelf || type == reference::FilterType::HighShelf;
}

bool isBandQuantity(MappedQuantity quantity)
{
    return quantity == MappedQuantity::Frequency || quantity == MappedQuantity::Gain || quantity == MappedQuantity::Q || quantity == MappedQuantity::Order;
}

juce::String formatRate(double sampleRate)
{
    return juce::String(sampleRate / 1000.0, 1) + " kHz";
}

// a measured or shown value of a quantity, for the report
juce::String formatValue(MappedQuantity quantity, double value)
{
    if (quantity == MappedQuantity::Frequency)
    {
        return formatFrequency(value) + " Hz";
    }
    if (quantity == MappedQuantity::Gain)
    {
        return juce::String(value, 2) + " dB";
    }
    if (quantity == MappedQuantity::Order)
    {
        return juce::String(juce::roundToInt(value));
    }
    return juce::String(value, 3);
}

// the band of the fit that the quantity belongs to: the band with the largest gain (a peak or shelf), else the first other band (a pass filter)
const eqmodel::Band* findDominantBand(const eqmodel::FitResult& fit, MappedQuantity quantity)
{
    const eqmodel::Band* best = nullptr;
    for (const eqmodel::Band& band : fit.model.bands)
    {
        if (isGainType(band.type) && (best == nullptr || std::abs(band.gainDb) > std::abs(best->gainDb)))
        {
            best = &band;
        }
    }
    if (best != nullptr || quantity == MappedQuantity::Gain)
    {
        return best;
    }
    for (const eqmodel::Band& band : fit.model.bands)
    {
        if (quantity != MappedQuantity::Order || band.family == eqmodel::DesignFamily::Butterworth || band.family == eqmodel::DesignFamily::LinkwitzRiley)
        {
            return &band;
        }
    }
    return nullptr;
}

double getTolerance(MappedQuantity quantity, const CalibrationPoint& point, const CalibrationSettings& settings)
{
    const double relativeResolution = point.resolution / std::max(std::abs(point.shown), 1.0e-9);
    if (quantity == MappedQuantity::Frequency)
    {
        return std::max(settings.frequencyTolerance, relativeResolution);
    }
    if (quantity == MappedQuantity::Q)
    {
        return std::max(settings.qTolerance, relativeResolution);
    }
    if (quantity == MappedQuantity::Gain)
    {
        return std::max(settings.gainToleranceDb, point.resolution);
    }
    return kOrderTolerance;
}

bool isRelative(MappedQuantity quantity)
{
    return quantity == MappedQuantity::Frequency || quantity == MappedQuantity::Q;
}

// the deviation of a point: relative (frequency, Q) or absolute (gain, order)
double getDeviation(MappedQuantity quantity, double shown, double measured)
{
    return isRelative(quantity) ? measured / shown - 1.0 : measured - shown;
}

// deviations, their pattern and whether one straight line or one factor describes them
void analyseSeries(CalibrationSeries& series, MappedQuantity quantity, const CalibrationSettings& settings)
{
    std::vector<const CalibrationPoint*> measured;
    for (CalibrationPoint& point : series.points)
    {
        if (! point.measured)
        {
            continue;
        }
        point.deviates = std::abs(getDeviation(quantity, point.shown, *point.measured)) > getTolerance(quantity, point, settings);
        series.deviates = series.deviates || point.deviates;
        measured.push_back(&point);
    }
    series.measuredPoints = static_cast<int>(measured.size());
    if (measured.empty())
    {
        series.pattern = "not measurable";
        return;
    }
    if (isRelative(quantity))
    {
        double sum = 0.0;
        double lowest = 1.0e9;
        double highest = 0.0;
        for (const CalibrationPoint* point : measured)
        {
            const double ratio = *point->measured / point->shown;
            sum += std::log(ratio);
            lowest = std::min(lowest, ratio);
            highest = std::max(highest, ratio);
        }
        series.factor = std::exp(sum / static_cast<double>(measured.size()));
        series.constant = std::all_of(measured.begin(), measured.end(), [&](const CalibrationPoint* point) {
            return std::abs(*point->measured / point->shown / series.factor - 1.0) <= getTolerance(quantity, *point, settings);
        });
        if (! series.deviates)
        {
            series.pattern = "as shown";
            return;
        }
        if (quantity == MappedQuantity::Q)
        {
            // a "Q" knob that is a bandwidth in octaves
            const bool bandwidth = std::all_of(measured.begin(), measured.end(), [&](const CalibrationPoint* point) {
                const double expected = eqmodel::getQFromBandwidthAnalog(point->shown);
                return std::abs(*point->measured / expected - 1.0) <= getTolerance(quantity, *point, settings);
            });
            if (bandwidth)
            {
                series.constant = true;
                series.pattern = "a bandwidth in octaves, not a Q";
                return;
            }
        }
        series.pattern = series.constant ? "x " + juce::String(series.factor, 3)
                                         : "varies: x " + juce::String(lowest, 3) + " ... x " + juce::String(highest, 3);
        return;
    }
    if (quantity == MappedQuantity::Gain)
    {
        // measured = a shown + b, least squares
        double sx = 0.0;
        double sy = 0.0;
        double sxx = 0.0;
        double sxy = 0.0;
        const double n = static_cast<double>(measured.size());
        for (const CalibrationPoint* point : measured)
        {
            sx += point->shown;
            sy += *point->measured;
            sxx += point->shown * point->shown;
            sxy += point->shown * *point->measured;
        }
        const double denominator = n * sxx - sx * sx;
        series.factor = std::abs(denominator) > 1.0e-12 ? (n * sxy - sx * sy) / denominator : 1.0;
        series.offset = (sy - series.factor * sx) / n;
        series.constant = std::all_of(measured.begin(), measured.end(), [&](const CalibrationPoint* point) {
            return std::abs(*point->measured - (series.factor * point->shown + series.offset)) <= getTolerance(quantity, *point, settings);
        });
        if (! series.deviates)
        {
            series.pattern = "as shown";
            return;
        }
        double worst = 0.0;
        const CalibrationPoint* worstPoint = measured.front();
        for (const CalibrationPoint* point : measured)
        {
            if (std::abs(*point->measured - point->shown) > worst)
            {
                worst = std::abs(*point->measured - point->shown);
                worstPoint = point;
            }
        }
        if (! series.constant)
        {
            series.pattern = "varies: up to " + juce::String(*worstPoint->measured - worstPoint->shown, 2) + " dB (at " + worstPoint->text + ")";
        }
        else if (std::abs(series.factor - 1.0) < kUnitSlopeTolerance)
        {
            series.pattern = "offset " + juce::String(series.offset >= 0.0 ? "+" : "") + juce::String(series.offset, 2) + " dB";
        }
        else if (std::abs(series.offset) < settings.gainToleranceDb)
        {
            series.pattern = "x " + juce::String(series.factor, 3) + " (the dB shown are not the band's dB)";
        }
        else
        {
            series.pattern = juce::String(series.factor, 3) + " x shown " + (series.offset >= 0.0 ? "+ " : "- ") + juce::String(std::abs(series.offset), 2) + " dB";
        }
        return;
    }
    // order: a whole number of steps
    std::vector<int> differences;
    for (const CalibrationPoint* point : measured)
    {
        differences.push_back(juce::roundToInt(*point->measured - point->shown));
    }
    series.constant = std::all_of(differences.begin(), differences.end(), [&](int d) { return d == differences.front(); });
    series.offset = differences.front();
    if (! series.deviates)
    {
        series.pattern = "as shown";
        return;
    }
    series.pattern = series.constant ? juce::String(differences.front() > 0 ? "+" : "") + juce::String(differences.front()) + " orders" : juce::String("varies");
}

// what the series of all rates say together
juce::String concludeOverRates(const ParameterCalibration& parameter, const CalibrationSettings& settings)
{
    if (! parameter.deviates)
    {
        return "as shown at every rate";
    }
    std::vector<const CalibrationSeries*> measured;
    for (const CalibrationSeries& series : parameter.rates)
    {
        if (series.measuredPoints > 0)
        {
            measured.push_back(&series);
        }
    }
    const bool allConstant = ! measured.empty() && std::all_of(measured.begin(), measured.end(), [](const CalibrationSeries* s) { return s->constant; });
    juce::StringArray deviating;
    juce::StringArray agreeing;
    for (const CalibrationSeries* series : measured)
    {
        (series->deviates ? deviating : agreeing).add(formatRate(series->sampleRate));
    }
    if (parameter.quantity == MappedQuantity::Frequency && allConstant && measured.size() > 1)
    {
        // a band designed for one rate whatever the host runs at: the factor is host rate / design rate
        double lowest = 1.0e12;
        double highest = 0.0;
        for (const CalibrationSeries* series : measured)
        {
            const double designRate = series->sampleRate / series->factor;
            lowest = std::min(lowest, designRate);
            highest = std::max(highest, designRate);
        }
        const double designRate = std::sqrt(lowest * highest);
        if (highest / lowest - 1.0 < kRateAgreement && ! agreeing.isEmpty())
        {
            double named = designRate;
            for (const double rate : kKnownRates)
            {
                if (std::abs(designRate / rate - 1.0) < kRateAgreement)
                {
                    named = rate;
                }
            }
            return "follows the sample rate: the band is designed for " + formatRate(named) + " whatever rate the host runs at (as shown only at "
                   + agreeing.joinIntoString(", ") + ")";
        }
    }
    if (allConstant && agreeing.isEmpty())
    {
        // the same pattern, or factors (and offsets) within the tolerance of each other
        const CalibrationSeries& first = *measured.front();
        const double relativeTolerance = parameter.quantity == MappedQuantity::Q ? settings.qTolerance : settings.frequencyTolerance;
        const bool same = std::all_of(measured.begin(), measured.end(), [&](const CalibrationSeries* series) {
            if (series->pattern == first.pattern)
            {
                return true;
            }
            if (isRelative(parameter.quantity))
            {
                return std::abs(series->factor / first.factor - 1.0) <= relativeTolerance;
            }
            return std::abs(series->factor - first.factor) < kUnitSlopeTolerance && std::abs(series->offset - first.offset) <= settings.gainToleranceDb;
        });
        if (same)
        {
            return "the same at every rate: the knob shows something else than it does";
        }
    }
    if (! agreeing.isEmpty())
    {
        return "depends on the sample rate: deviates at " + deviating.joinIntoString(", ") + ", as shown at " + agreeing.joinIntoString(", ");
    }
    return "deviates at every rate, in different ways";
}
}

double getDisplayedResolution(const juce::String& text)
{
    const juce::String trimmed = text.trim();
    int start = 0;
    while (start < trimmed.length() && ! juce::CharacterFunctions::isDigit(trimmed[start]))
    {
        ++start;
    }
    if (start >= trimmed.length())
    {
        return 0.0;
    }
    int end = start;
    int decimals = 0;
    bool afterPoint = false;
    while (end < trimmed.length() && (juce::CharacterFunctions::isDigit(trimmed[end]) || trimmed[end] == '.'))
    {
        if (afterPoint)
        {
            ++decimals;
        }
        afterPoint = afterPoint || trimmed[end] == '.';
        ++end;
    }
    double resolution = 0.5 * std::pow(10.0, -decimals);
    const juce::String rest = trimmed.substring(end).trimStart();
    if (rest.startsWithChar('k') || rest.startsWithChar('K'))
    {
        resolution *= 1000.0;
    }
    return resolution;
}

CalibrationResult calibrate(const SurveySubject& subject, const Mapping& mapping, const CalibrationSettings& settings, const CalibrationProgress& progress)
{
    const double started = juce::Time::getMillisecondCounterHiRes();
    CalibrationResult result;
    result.pluginName = mapping.pluginName;
    result.settings = settings;
    std::vector<float> defaults;
    for (const SurveyParameter& parameter : subject.parameters)
    {
        defaults.push_back(parameter.defaultValue);
    }
    std::vector<const MappedParameter*> mapped;
    for (const MappedParameter& parameter : mapping.parameters)
    {
        if (isBandQuantity(parameter.quantity) && parameter.index >= 0 && parameter.index < static_cast<int>(subject.parameters.size()))
        {
            mapped.push_back(&parameter);
        }
    }
    const int total = static_cast<int>(mapped.size() * settings.sampleRates.size());
    int done = 0;
    for (const MappedParameter* parameter : mapped)
    {
        const SurveyParameter& surveyed = subject.parameters[static_cast<size_t>(parameter->index)];
        ParameterCalibration calibration;
        calibration.index = parameter->index;
        calibration.name = parameter->name;
        calibration.quantity = parameter->quantity;
        calibration.band = parameter->band;
        std::vector<float> base = defaults;
        juce::StringArray context;
        for (const Requirement& requirement : parameter->requirements)
        {
            if (requirement.parameter >= 0 && requirement.parameter < static_cast<int>(base.size()))
            {
                base[static_cast<size_t>(requirement.parameter)] = requirement.value;
                context.add(requirement.name + " = " + requirement.text);
            }
        }
        calibration.context = context.joinIntoString(", ");
        // the knob positions: every step of a discrete knob, else evenly 0.05 ... 0.95
        std::vector<float> values;
        const int steps = surveyed.numSteps;
        if (steps >= 2 && steps <= settings.maximumSteps)
        {
            for (int step = 0; step < steps; ++step)
            {
                values.push_back(static_cast<float>(step) / static_cast<float>(steps - 1));
            }
        }
        else
        {
            for (int index = 0; index < settings.continuousPoints; ++index)
            {
                values.push_back(static_cast<float>(0.05 + 0.9 * index / std::max(settings.continuousPoints - 1, 1)));
            }
        }
        for (const double sampleRate : settings.sampleRates)
        {
            if (progress)
            {
                progress(static_cast<double>(done) / std::max(total, 1), parameter->name + " at " + formatRate(sampleRate));
            }
            CalibrationSeries series;
            series.sampleRate = sampleRate;
            const std::vector<double> grid = eqmodel::makeLogGrid(kLowestHz, std::min(kHighestHz, 0.45 * sampleRate), kGridPerOctave);
            for (const float value : values)
            {
                CalibrationPoint point;
                point.value = value;
                point.text = surveyed.getText ? surveyed.getText(value) : juce::String(value);
                const std::optional<double> shown = parseDisplayedNumber(point.text);
                if (! shown)
                {
                    continue; // a text without a number: nothing to compare
                }
                point.shown = *shown;
                point.resolution = getDisplayedResolution(point.text);
                if (parameter->quantity == MappedQuantity::Frequency && (point.shown < kLowestHz || point.shown > grid.back()))
                {
                    point.note = "outside the measured band";
                    series.points.push_back(point);
                    continue;
                }
                std::vector<float> setting = base;
                setting[static_cast<size_t>(parameter->index)] = value;
                measure::SweepResponseSettings sweep;
                sweep.sampleRate = sampleRate;
                sweep.channels = 1;
                sweep.levelDbfs = settings.levelDbfs;
                sweep.approximateSeconds = settings.sweepSeconds;
                sweep.frequencies = grid;
                const measure::FrequencyResponse response = measure::measureSweptResponse(subject.makeDevice(setting), sweep);
                ++result.renders;
                if (response.channels.empty())
                {
                    point.note = "no output";
                    series.points.push_back(point);
                    continue;
                }
                eqmodel::FitSettings fitSettings;
                fitSettings.sampleRate = sampleRate;
                fitSettings.maximumBands = kFitBands;
                fitSettings.highestHz = grid.back();
                eqmodel::FitResult fit = eqmodel::fitEqModel(grid, response.channels[0].response, fitSettings);
                fit.model.bands.erase(std::remove_if(fit.model.bands.begin(), fit.model.bands.end(),
                                                     [&](const eqmodel::Band& band) { return isGainType(band.type) && std::abs(band.gainDb) < settings.silentGainDb; }),
                                      fit.model.bands.end());
                if (! fit.goodDescription)
                {
                    point.note = "no band description";
                    series.points.push_back(point);
                    continue;
                }
                const eqmodel::Band* band = findDominantBand(fit, parameter->quantity);
                if (band == nullptr)
                {
                    if (parameter->quantity == MappedQuantity::Gain)
                    {
                        point.measured = 0.0; // flat: no gain
                    }
                    else
                    {
                        point.note = "no band";
                    }
                    series.points.push_back(point);
                    continue;
                }
                if (parameter->quantity == MappedQuantity::Frequency)
                {
                    point.measured = band->frequencyHz;
                }
                else if (parameter->quantity == MappedQuantity::Gain)
                {
                    point.measured = band->gainDb;
                }
                else if (parameter->quantity == MappedQuantity::Q)
                {
                    point.measured = band->q;
                }
                else
                {
                    point.measured = static_cast<double>(band->order);
                }
                // a band at the edge of the measured band: its frequency (and width) cannot be trusted
                if (parameter->quantity != MappedQuantity::Gain && (band->frequencyHz <= kLowestHz * 1.05 || band->frequencyHz >= grid.back() / 1.05))
                {
                    point.measured.reset();
                    point.note = "band at the edge of the measured band";
                }
                series.points.push_back(point);
            }
            analyseSeries(series, parameter->quantity, settings);
            calibration.deviates = calibration.deviates || series.deviates;
            calibration.rates.push_back(series);
            ++done;
        }
        calibration.conclusion = concludeOverRates(calibration, settings);
        result.parameters.push_back(calibration);
    }
    if (progress)
    {
        progress(1.0, "done");
    }
    result.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
    return result;
}

juce::String createCalibrationReport(const CalibrationResult& result)
{
    juce::String text;
    juce::StringArray rates;
    for (const double rate : result.settings.sampleRates)
    {
        rates.add(formatRate(rate));
    }
    text << "## Calibration: myth and reality\n\n";
    text << "What the knobs show against what the band model measures, for the band parameters of the mapping, at " << rates.joinIntoString(", ")
         << " (W8.6). " << result.renders << " renders (" << juce::roundToInt(result.seconds) << " s).\n\n";
    juce::StringArray asShown;
    std::vector<const ParameterCalibration*> deviating;
    for (const ParameterCalibration& parameter : result.parameters)
    {
        if (parameter.deviates)
        {
            deviating.push_back(&parameter);
        }
        else
        {
            asShown.add(parameter.name);
        }
    }
    if (result.parameters.empty())
    {
        text << "The mapping has no band parameters (frequency, gain, Q, order): nothing to calibrate.\n";
        return wrapMarkdownProse(text);
    }
    if (deviating.empty())
    {
        text << "**Everything as shown**: " << asShown.joinIntoString(", ") << " agree with the measurement within the tolerances at every rate.\n";
        return wrapMarkdownProse(text);
    }
    text << "### Deviations\n\n| # | Parameter | Band |";
    for (const juce::String& rate : rates)
    {
        text << " " << rate << " |";
    }
    text << " Conclusion |\n|---|---|---|";
    for (int index = 0; index < rates.size(); ++index)
    {
        text << "---|";
    }
    text << "---|\n";
    for (const ParameterCalibration* parameter : deviating)
    {
        text << "| " << (parameter->index + 1) << " | " << parameter->name << " (" << getMappedQuantityName(parameter->quantity) << ") | "
             << (parameter->band >= 0 ? juce::String(parameter->band + 1) : juce::String("-")) << " |";
        for (const CalibrationSeries& series : parameter->rates)
        {
            text << " " << (series.deviates ? "**" + series.pattern + "**" : series.pattern) << " |";
        }
        text << " " << parameter->conclusion << " |\n";
    }
    if (! asShown.isEmpty())
    {
        text << "\nAs shown at every rate: " << asShown.joinIntoString(", ") << ".\n";
    }
    for (const ParameterCalibration* parameter : deviating)
    {
        text << "\n### " << parameter->name << ": shown and measured\n\n";
        if (parameter->context.isNotEmpty())
        {
            text << "With " << parameter->context << ". ";
        }
        text << "Bold: outside the tolerance.\n\n| Shown |";
        for (const juce::String& rate : rates)
        {
            text << " " << rate << " |";
        }
        text << "\n|---|";
        for (int index = 0; index < rates.size(); ++index)
        {
            text << "---|";
        }
        text << "\n";
        const size_t count = parameter->rates.empty() ? 0 : parameter->rates.front().points.size();
        for (size_t row = 0; row < count; ++row)
        {
            const CalibrationPoint& shownPoint = parameter->rates.front().points[row];
            text << "| " << formatValue(parameter->quantity, shownPoint.shown) << " |";
            for (const CalibrationSeries& series : parameter->rates)
            {
                if (row >= series.points.size())
                {
                    text << " - |";
                    continue;
                }
                const CalibrationPoint& point = series.points[row];
                if (! point.measured)
                {
                    text << " - (" << point.note << ") |";
                    continue;
                }
                juce::String cell = formatValue(parameter->quantity, *point.measured);
                if (isRelative(parameter->quantity))
                {
                    cell << " (x " << juce::String(*point.measured / point.shown, 3) << ")";
                }
                else
                {
                    const double difference = *point.measured - point.shown;
                    cell << " (" << (difference >= 0.0 ? "+" : "") << juce::String(difference, 2) << ")";
                }
                text << " " << (point.deviates ? "**" + cell + "**" : cell) << " |";
            }
            text << "\n";
        }
    }
    return wrapMarkdownProse(text);
}

juce::String createCalibrationReportFooter(const CalibrationSettings& settings)
{
    juce::String text;
    text << "## About the calibration\n\n";
    text << "**Why.** A knob that says 1 kHz does not always give a band at 1 kHz: a filter designed for one sample rate (the PeakEQ of W5 sits "
            "8.8 % high at 48 kHz), a \"Q\" that is a bandwidth, dB that are not the band's dB. Matching one EQ to another (W9) needs to know where "
            "the knobs lie; the matcher gets these deviations as a few examples, not a corrected law.\n\n";
    text << "**What it does.** For each band parameter of the mapping (frequency, gain, Q, order) the knob is set to several positions ("
         << settings.continuousPoints << " of a continuous knob, every step of a switch), with the other parameters it needs to act (the mapping's "
            "\"needs\"), at every sample rate. Each position is measured with a synchronized sweep and described by the band model (W8.3); the "
            "dominant band's value is compared with the number the plugin shows.\n\n";
    text << "**Tolerances.** Frequency " << juce::String(100.0 * settings.frequencyTolerance, 1) << " %, gain "
         << juce::String(settings.gainToleranceDb, 2) << " dB, Q " << juce::String(100.0 * settings.qTolerance, 1)
         << " %, order 0; never less than half a unit of the last digit the plugin shows (\"28 Hz\": 0.5 Hz). Positions where no band can be measured "
            "(a band outside 20 Hz ... 20 kHz or at its edge, no gain) are marked \"-\".\n\n";
    text << "**Patterns.** \"x 1.088\": the measured value is the shown one times 1.088 at every position. \"follows the sample rate\": the factor is "
            "host rate / design rate (a filter designed for a fixed rate). \"offset\" and \"x\" for a gain: measured = shown + offset, or a "
            "multiple of the shown dB. \"a bandwidth in octaves\": the measured Q is that of a bandwidth of the shown number of octaves. "
            "\"varies\": no single factor or line describes it (see the points).\n";
    return text;
}
}
