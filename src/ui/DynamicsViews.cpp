#include "pluginlab/ui/DynamicsViews.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/ui/ChartImage.h"

namespace pluginlab::ui
{
namespace
{
constexpr double kLowestDbfs = -60.0;
constexpr double kTimeConstants = 6.0;      // a phase is shown for this many of its time constants (and the hold)
constexpr double kShortestShownSeconds = 0.005;
constexpr double kMusicFloorDbfs = -60.0;
constexpr double kLegendRowDb = 3.5;        // the room a legend row needs above the music gain curves (66 dB on 440 pixels)
constexpr double kShownHoldSeconds = 0.0005; // a fitted hold below this is the fit's noise, not marked

juce::String writeView(const juce::Image& image, const juce::File& folder, const juce::String& stem, const juce::String& view, const juce::String& caption)
{
    const juce::String fileName = stem + "_" + view + ".png";
    if (! writePng(image, folder.getChildFile(fileName)))
    {
        return {};
    }
    return "![" + caption + "](" + fileName + ")\n\n";
}

// the smallest and largest of the finite values, widened to whole multiples of `grid`
void getRange(const std::vector<double>& values, double grid, double& minimum, double& maximum)
{
    for (const double value : values)
    {
        if (std::isfinite(value))
        {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
    }
    minimum = grid * std::floor(minimum / grid);
    maximum = grid * std::ceil(maximum / grid);
}

// one phase of the step (from `start` for `seconds`): the measured gain and the model's, the time in ms from the step
void addPhase(std::vector<ChartSeries>& series, const measure::StepResponse& step, const std::vector<double>& model, int start, double seconds, double sampleRate)
{
    ChartSeries measured;
    measured.name = "measured";
    measured.colour = getChartColour(0);
    measured.thickness = 2.5f;
    ChartSeries fitted;
    fitted.name = "fitted model";
    fitted.colour = getChartColour(1);
    fitted.thickness = 1.2f;
    const int count = static_cast<int>(seconds * sampleRate);
    const int stride = std::max(1, count / 1500);
    // from a little before the step, so that the level before it shows
    for (int offset = -count / 20; offset < count; offset += stride)
    {
        const int sample = start + offset;
        if (sample < 0 || sample >= static_cast<int>(step.gainDb.size()) || sample >= static_cast<int>(model.size()))
        {
            continue;
        }
        const double ms = 1000.0 * offset / sampleRate;
        measured.x.push_back(ms);
        measured.y.push_back(step.gainDb[static_cast<size_t>(sample)]);
        fitted.x.push_back(ms);
        fitted.y.push_back(model[static_cast<size_t>(sample)]);
    }
    series.push_back(measured);
    series.push_back(fitted);
}
}

juce::String writeDynamicsViews(const measure::DynamicsMeasurement& dynamics, double sampleRate, const juce::File& folder, const juce::String& stem,
                                const juce::String& name)
{
    if (! dynamics.levelDependent || dynamics.curve.rising.empty())
    {
        return {};
    }
    const double fs = sampleRate;
    const measure::StaticFit& fit = dynamics.fit;
    juce::String text = "## Dynamics views\n\n";
    std::vector<std::pair<double, juce::String>> thresholdMarker;
    if (dynamics.compressor)
    {
        thresholdMarker.push_back({fit.thresholdDb, "threshold " + juce::String(fit.thresholdDb, 1) + " dBFS"});
    }

    // 1. the static curve: output over input, the 1 : 1 line, the fitted model
    {
        ChartSeries measured;
        measured.name = "rising staircase";
        measured.colour = getChartColour(0);
        measured.thickness = 3.0f;
        ChartSeries falling;
        falling.name = "falling staircase";
        falling.colour = getChartColour(2);
        falling.thickness = 1.0f;
        ChartSeries model;
        model.name = "fitted model";
        model.colour = getChartColour(1);
        model.thickness = 1.2f;
        ChartSeries unity;
        unity.name = "1 : 1";
        unity.colour = juce::Colours::grey;
        unity.thickness = 1.0f;
        unity.x = {kLowestDbfs, 0.0};
        unity.y = {kLowestDbfs, 0.0};
        std::vector<double> outputs;
        for (const measure::StaticPoint& point : dynamics.curve.rising)
        {
            measured.x.push_back(point.inputDbfs);
            measured.y.push_back(point.outputDbfs);
            outputs.push_back(point.outputDbfs);
        }
        for (const measure::StaticPoint& point : dynamics.curve.falling)
        {
            falling.x.push_back(point.inputDbfs);
            falling.y.push_back(point.outputDbfs);
        }
        for (double level = kLowestDbfs; level <= 0.0 + 1.0e-9; level += 0.25)
        {
            model.x.push_back(level);
            model.y.push_back(level + measure::getModelGainDb(fit, level));
        }
        double low = kLowestDbfs;
        double high = 0.0;
        getRange(outputs, 6.0, low, high);
        ChartOptions options;
        options.title = name + ": static curve (a 500 Hz square per level, 0.5 s each); the model " + juce::String(fit.ratio, 2) + " : 1, knee "
                        + juce::String(fit.kneeDb, 1) + " dB, makeup " + juce::String(std::round(fit.makeupDb * 10.0) / 10.0 + 0.0, 1) + " dB";
        options.xLabel = "input level in dBFS";
        options.yLabel = "output level in dBFS";
        options.xMinimum = kLowestDbfs;
        options.xMaximum = 0.0;
        options.yMinimum = low;
        options.yMaximum = high;
        options.height = 520;
        options.markers = thresholdMarker;
        options.legendAtBottom = true;
        text << writeView(renderChart({unity, falling, measured, model}, options), folder, stem, "static_curve", "static curve");
    }

    // 2. the gain reduction over the input level
    {
        ChartSeries measured;
        measured.name = "gain (makeup removed)";
        measured.colour = getChartColour(0);
        measured.thickness = 2.5f;
        std::vector<double> gains;
        for (const measure::StaticPoint& point : dynamics.curve.rising)
        {
            measured.x.push_back(point.inputDbfs);
            measured.y.push_back(point.gainDb - fit.makeupDb);
            gains.push_back(point.gainDb - fit.makeupDb);
        }
        double low = 0.0;
        double high = 0.0;
        getRange(gains, 3.0, low, high);
        ChartOptions options;
        options.title = name + ": gain over the input level";
        options.xLabel = "input level in dBFS";
        options.yLabel = "gain in dB";
        options.xMinimum = kLowestDbfs;
        options.xMaximum = 0.0;
        options.yMinimum = low;
        options.yMaximum = std::max(high, low + 3.0);
        options.markers = thresholdMarker;
        text << writeView(renderChart({measured}, options), folder, stem, "gain_reduction", "gain reduction curve");
    }

    if (! dynamics.compressor || dynamics.step.gainDb.empty())
    {
        return text;
    }

    // 3. and 4. the gain curves of the attack and the release phase with the fitted model
    {
        const measure::StepResponse& step = dynamics.step;
        const measure::TimeParameters& parameters = step.best.parameters;
        const std::vector<double> model = measure::simulateGainDb(step.levelsDb, fit, step.best.model, parameters, fs);
        std::vector<double> phaseGains;
        const int phaseLength = step.releaseStart - step.attackStart;
        for (int sample = std::max(0, step.attackStart - phaseLength / 10); sample < std::min(static_cast<int>(step.gainDb.size()), step.releaseStart + phaseLength); ++sample)
        {
            phaseGains.push_back(step.gainDb[static_cast<size_t>(sample)]);
        }
        double low = 0.0;
        double high = 0.0;
        getRange(phaseGains, 3.0, low, high);
        const double phaseSeconds = phaseLength / fs;
        const double attackShown = std::min(phaseSeconds, std::max(kShortestShownSeconds, kTimeConstants * std::max(parameters.attackSeconds, parameters.detectorAttackSeconds)));
        const double releaseShown = std::min(phaseSeconds, std::max(kShortestShownSeconds, parameters.holdSeconds + kTimeConstants * parameters.releaseSeconds));
        {
            std::vector<ChartSeries> series;
            addPhase(series, step, model, step.attackStart, attackShown, fs);
            ChartOptions options;
            options.title = name + ", " + measure::getTimeModelName(step.best.model) + ": attack (the level steps from " + juce::String(juce::roundToInt(step.levelsDb[static_cast<size_t>(step.attackStart - 1)])) + " to "
                            + juce::String(juce::roundToInt(step.levelsDb[static_cast<size_t>(step.attackStart)])) + " dBFS at 0 ms): attack "
                            + juce::String(parameters.attackSeconds * 1000.0, 2) + " ms";
            options.xLabel = "time after the step in ms";
            options.yLabel = "gain in dB";
            options.xMinimum = -attackShown * 1000.0 / 20.0;
            options.xMaximum = attackShown * 1000.0;
            options.yMinimum = low;
            options.yMaximum = high;
            options.markers.push_back({step.attackSettleSeconds * 1000.0, "within 3 dB"});
            if (step.leadSamples > 0)
            {
                options.markers.push_back({-step.leadSamples * 1000.0 / fs, "look-ahead"});
            }
            text << writeView(renderChart(series, options), folder, stem, "attack", "gain curve of the attack phase");
        }
        {
            std::vector<ChartSeries> series;
            addPhase(series, step, model, step.releaseStart, releaseShown, fs);
            ChartOptions options;
            options.title = name + ": release (the level falls back at 0 ms): release " + juce::String(parameters.releaseSeconds * 1000.0, 1) + " ms, hold "
                            + juce::String(parameters.holdSeconds * 1000.0, 1) + " ms";
            options.xLabel = "time after the fall in ms";
            options.yLabel = "gain in dB";
            options.xMinimum = -releaseShown * 1000.0 / 20.0;
            options.xMaximum = releaseShown * 1000.0;
            options.yMinimum = low;
            options.yMaximum = high;
            options.markers.push_back({step.releaseSettleSeconds * 1000.0, "within 4 dB"});
            options.legendAtBottom = true; // the gain has come back up at the right
            if (parameters.holdSeconds >= kShownHoldSeconds)
            {
                options.markers.push_back({parameters.holdSeconds * 1000.0, "hold"});
            }
            text << writeView(renderChart(series, options), folder, stem, "release", "gain curve of the release phase");
        }
    }

    // 5. the dynamic transfer function of the two-tone test
    if (! dynamics.transfer.frequencies.empty())
    {
        ChartSeries reduction;
        reduction.name = "the probe's reduction";
        reduction.colour = getChartColour(0);
        reduction.x = dynamics.transfer.frequencies;
        reduction.y = dynamics.transfer.reductionDb;
        double low = 0.0;
        double high = 3.0;
        getRange(dynamics.transfer.reductionDb, 3.0, low, high);
        ChartOptions options;
        options.title = name + ": two-tone test (a loud " + juce::String(juce::roundToInt(dynamics.toneHz)) + " Hz tone, quiet probes): "
                        + measure::getBandKindName(dynamics.transfer.kind);
        options.xLabel = "probe frequency in Hz";
        options.yLabel = "reduction in dB";
        options.logX = true;
        options.xMinimum = 20.0;
        options.xMaximum = 20000.0;
        options.yMinimum = low;
        options.yMaximum = high;
        options.markers.push_back({dynamics.toneHz, "loud tone"});
        text << writeView(renderChart({reduction}, options), folder, stem, "two_tone", "dynamic transfer function");
    }
    return text;
}

juce::Image renderMusicGainCurves(const std::vector<MusicGainTrace>& traces, const juce::String& title)
{
    std::vector<ChartSeries> series;
    double low = 0.0;
    double high = 0.0;
    double longest = 1.0;
    if (! traces.empty())
    {
        ChartSeries level;
        level.name = "input level (dBFS)";
        level.colour = juce::Colours::grey;
        level.thickness = 1.0f;
        for (size_t index = 0; index < traces.front().curve.times.size(); ++index)
        {
            level.x.push_back(traces.front().curve.times[index]);
            level.y.push_back(std::max(kMusicFloorDbfs, traces.front().curve.inputLevelDbfs[index]));
        }
        series.push_back(level);
    }
    int colour = 0;
    for (const MusicGainTrace& trace : traces)
    {
        ChartSeries gain;
        gain.name = trace.name;
        gain.colour = getChartColour(colour++);
        gain.thickness = 1.5f;
        gain.x = trace.curve.times;
        gain.y = trace.curve.gainDb;
        getRange(trace.curve.gainDb, 6.0, low, high);
        if (! trace.curve.times.empty())
        {
            longest = std::max(longest, trace.curve.times.back());
        }
        series.push_back(gain);
    }
    ChartOptions options;
    options.title = title;
    options.xLabel = "time in s";
    options.yLabel = "gain in dB; the input level in dBFS";
    options.xMinimum = 0.0;
    options.xMaximum = longest;
    options.yMinimum = kMusicFloorDbfs;
    options.yMaximum = std::max(high, 0.0) + kLegendRowDb * static_cast<double>(series.size()); // the legend above the curves
    options.height = 520;
    return renderChart(series, options);
}
}
