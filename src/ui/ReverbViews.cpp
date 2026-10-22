#include "pluginlab/ui/ReverbViews.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/ui/ChartImage.h"

namespace pluginlab::ui
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDirectWindowSeconds = 0.0025;
constexpr size_t kChartPoints = 1500;        // a decay curve is drawn with at most this many points
constexpr double kEchogramSeconds = 0.1;
constexpr double kFloorDb = -70.0;

// the reverberant part as the decay analysis uses it: the direct path cut out
std::vector<double> getReverberant(const measure::ReverbMeasurement& reverb, const std::vector<double>& response, double sampleRate)
{
    std::vector<double> result = response;
    if (reverb.decay.hasDirect)
    {
        const int peak = static_cast<int>(reverb.decay.directSample);
        const int window = static_cast<int>(kDirectWindowSeconds * sampleRate);
        for (int sample = std::max(0, peak - window); sample < std::min(static_cast<int>(result.size()), peak + window + 1); ++sample)
        {
            result[static_cast<size_t>(sample)] = 0.0;
        }
    }
    return result;
}

// every n-th point of a curve, as seconds and values
void decimate(const std::vector<double>& curve, double sampleRate, std::vector<double>& x, std::vector<double>& y)
{
    const size_t step = std::max<size_t>(1, curve.size() / kChartPoints);
    for (size_t index = 0; index < curve.size(); index += step)
    {
        x.push_back(static_cast<double>(index) / sampleRate);
        y.push_back(std::max(curve[index], kFloorDb - 10.0));
    }
}

// the last value of a time axis, or a fallback for an empty one
double getLast(const std::vector<double>& values, double fallback)
{
    if (values.empty())
    {
        return fallback;
    }
    return values.back();
}

double toDb(double value)
{
    return 20.0 * std::log10(std::max(std::abs(value), 1.0e-12));
}

juce::String writeView(const juce::Image& image, const juce::File& folder, const juce::String& stem, const juce::String& view, const juce::String& caption)
{
    const juce::String fileName = stem + "_" + view + ".png";
    if (! writePng(image, folder.getChildFile(fileName)))
    {
        return {};
    }
    return "![" + caption + "](" + fileName + ")\n\n";
}
}

juce::String writeReverbViews(const measure::ReverbMeasurement& reverb, double sampleRate, const juce::File& folder, const juce::String& stem,
                              const juce::String& name)
{
    if (! reverb.hasReverb || reverb.left.empty())
    {
        return {};
    }
    const double fs = sampleRate;
    const measure::ReverbAnalysis& decay = reverb.decay;
    const int direct = static_cast<int>(decay.directSample);
    int decayStart = direct;
    if (decay.hasDirect)
    {
        decayStart = std::max(direct, static_cast<int>(decay.onsetSample));
    }
    const std::vector<double> reverberant = getReverberant(reverb, reverb.left, fs);
    const int end = decay.truncation.sample;
    // the time axes of the heat maps: down to -60 dB of the longest octave (and the pre-delay), not the whole render
    double longestDecay = decay.broadband.t30;
    for (const measure::BandDecay& band : decay.octaves)
    {
        longestDecay = std::max(longestDecay, band.times.t30);
    }
    const double shownSeconds = std::max(0.2, decay.preDelaySeconds + 1.2 * longestDecay);
    juce::String text = "## Reverb views\n\n";

    // 1. the EDC per octave with the T30 lines
    {
        std::vector<ChartSeries> series;
        double longest = 0.1;
        int colour = 0;
        for (const measure::BandDecay& band : decay.octaves)
        {
            const std::vector<double> filtered = measure::filterBand(reverberant, band.band, fs);
            const measure::Truncation truncation = measure::findTruncation(filtered, decayStart, fs);
            const std::vector<double> curve = measure::getDecayCurve(filtered, decayStart, truncation);
            ChartSeries line;
            line.name = juce::String(juce::roundToInt(band.band.centreHz)) + " Hz: T30 " + juce::String(band.times.t30, 2) + " s";
            line.colour = getChartColour(colour++);
            line.thickness = 1.5f;
            decimate(curve, fs, line.x, line.y);
            series.push_back(line);
            // the T30 line from its -5 dB point over 30 dB
            if (band.times.t30 > 0.0)
            {
                size_t five = 0;
                while (five < curve.size() && curve[five] > -5.0)
                {
                    ++five;
                }
                const double t5 = static_cast<double>(five) / fs;
                ChartSeries fit;
                fit.colour = line.colour.withAlpha(0.6f);
                fit.thickness = 1.0f;
                fit.x = {t5, t5 + band.times.t30 / 2.0};
                fit.y = {-5.0, -35.0};
                series.push_back(fit);
                longest = std::max(longest, t5 + band.times.t30 * 70.0 / 60.0);
            }
        }
        ChartOptions options;
        options.title = name + ": energy decay per octave (the reverberant part, from the onset) with the T30 lines";
        options.xLabel = "time in s";
        options.yLabel = "level in dB";
        options.xMaximum = std::min(longest, (end - decayStart) / fs);
        options.yMinimum = kFloorDb;
        options.yMaximum = 0.0;
        options.height = 460;
        text << writeView(renderChart(series, options), folder, stem, "edc_octaves", "energy decay per octave");
    }

    // 2. the decay time over frequency
    {
        ChartSeries t30;
        t30.name = "T30";
        t30.colour = getChartColour(0);
        ChartSeries edt;
        edt.name = "EDT";
        edt.colour = getChartColour(1);
        double largest = 0.1;
        for (const measure::BandDecay& band : decay.thirdOctaves)
        {
            t30.x.push_back(band.band.centreHz);
            t30.y.push_back(band.times.t30);
            edt.x.push_back(band.band.centreHz);
            edt.y.push_back(band.times.edt);
            largest = std::max({largest, band.times.t30, band.times.edt});
        }
        ChartOptions options;
        options.title = name + ": decay time per third octave";
        options.xLabel = "frequency in Hz";
        options.yLabel = "seconds";
        options.logX = true;
        options.xMinimum = 100.0;
        options.xMaximum = 10000.0;
        options.yMaximum = largest * 1.15;
        text << writeView(renderChart({t30, edt}, options), folder, stem, "decay_frequency", "decay time per third octave");
    }

    // 3. the energy decay relief
    {
        const measure::DecayRelief relief = measure::getDecayRelief(reverberant, decayStart, end, fs);
        std::vector<double> rows;
        std::vector<std::vector<double>> values;
        for (size_t bin = 1; bin < relief.frequencies.size(); ++bin)
        {
            if (relief.frequencies[bin] < 50.0)
            {
                continue;
            }
            rows.push_back(relief.frequencies[bin]);
            std::vector<double> line;
            for (const std::vector<float>& frame : relief.levelsDb)
            {
                line.push_back(frame[bin]);
            }
            values.push_back(line);
        }
        HeatmapOptions options;
        options.title = name + ": energy decay relief (Jot): the remaining energy per frequency over time";
        options.xLabel = "time in s";
        options.yLabel = "frequency in Hz";
        options.xMaximum = getLast(relief.times, 1.0);
        // only the frames up to the shown time
        const size_t frames = static_cast<size_t>(std::count_if(relief.times.begin(), relief.times.end(), [shownSeconds](double t) { return t <= shownSeconds; }));
        for (std::vector<double>& line : values)
        {
            line.resize(std::min(line.size(), std::max<size_t>(1, frames)));
        }
        options.xMaximum = std::min(options.xMaximum, shownSeconds);
        options.rowValues = rows;
        options.logY = true;
        options.floor = -60.0;
        text << writeView(renderHeatmap(values, options), folder, stem, "edr", "energy decay relief");
    }

    // 4. the echogram of the first 100 ms
    {
        ChartSeries stems;
        stems.colour = getChartColour(0);
        stems.stems = true;
        stems.thickness = 1.0f;
        double largest = 0.0;
        for (const double value : reverb.left)
        {
            largest = std::max(largest, std::abs(value));
        }
        const int count = std::min(static_cast<int>(reverb.left.size()) - direct, static_cast<int>(kEchogramSeconds * fs));
        for (int index = 0; index < count; ++index)
        {
            stems.x.push_back(1000.0 * index / fs);
            stems.y.push_back(std::max(kFloorDb + 10.0, toDb(reverb.left[static_cast<size_t>(direct + index)] / largest)));
        }
        ChartOptions options;
        options.title = name + ": echogram of the first 100 ms after the direct path (or the input)";
        options.xLabel = "time in ms";
        options.yLabel = "level in dB re the largest";
        options.xMaximum = 1000.0 * kEchogramSeconds;
        options.yMinimum = kFloorDb + 10.0;
        options.yMaximum = 0.0;
        options.markers = {{1000.0 * decay.preDelaySeconds, "pre-delay " + juce::String(1000.0 * decay.preDelaySeconds, 1) + " ms"}};
        text << writeView(renderChart({stems}, options), folder, stem, "echogram", "echogram");
    }

    // 5. the echo density profile and 6. Griesinger's echo count
    const measure::Texture& texture = reverb.texture;
    {
        ChartSeries density;
        density.name = "normalized echo density";
        density.colour = getChartColour(0);
        for (size_t index = 0; index < texture.density.size(); ++index)
        {
            density.x.push_back(1000.0 * texture.densityTimes[index]);
            density.y.push_back(texture.density[index]);
        }
        ChartOptions options;
        options.title = name + ": echo density profile (Abel and Huang; 1 = Gaussian noise) after the onset";
        options.xLabel = "time in ms";
        options.yLabel = "echo density";
        options.xMaximum = std::min(1000.0, getLast(density.x, 1000.0));
        options.yMaximum = 1.2;
        if (texture.mixingTimeSeconds >= 0.0)
        {
            options.markers = {{1000.0 * texture.mixingTimeSeconds, "mixing time " + juce::String(1000.0 * texture.mixingTimeSeconds, 0) + " ms"}};
        }
        text << writeView(renderChart({density}, options), folder, stem, "echo_density", "echo density profile");

        ChartSeries count;
        count.colour = getChartColour(1);
        count.stems = true;
        count.thickness = 3.0f;
        double largest = 1.0;
        for (size_t index = 0; index < texture.echoesPerSecond.size() && texture.echoTimes[index] <= 1.0; ++index)
        {
            count.x.push_back(1000.0 * texture.echoTimes[index]);
            count.y.push_back(texture.echoesPerSecond[index]);
            largest = std::max(largest, texture.echoesPerSecond[index]);
        }
        ChartOptions countOptions;
        countOptions.title = name + ": Griesinger's echo count (reflections within 20 dB of the largest per 20 ms; about 10000/s for percussive sounds)";
        countOptions.xLabel = "time in ms after the onset";
        countOptions.yLabel = "reflections per second";
        countOptions.xMaximum = 1000.0;
        countOptions.yMaximum = largest * 1.1;
        text << writeView(renderChart({count}, countOptions), folder, stem, "echo_count", "echo count");
    }

    // 7. the short-time histogram with the kurtosis
    if (! texture.histograms.empty())
    {
        const size_t bins = texture.histograms.front().size();
        std::vector<double> rows;
        std::vector<std::vector<double>> values(bins);
        for (size_t bin = 0; bin < bins; ++bin)
        {
            rows.push_back(-4.0 + 8.0 * (bin + 0.5) / static_cast<double>(bins));
            for (const std::vector<double>& histogram : texture.histograms)
            {
                values[bin].push_back(10.0 * std::log10(std::max(histogram[bin], 1.0e-6)));
            }
        }
        HeatmapOptions options;
        options.title = name + ": short-time histogram of h / sqrt(EDC) (the decay removed; Gaussian: a bell over time)";
        options.xLabel = "time in s after the onset";
        options.yLabel = "amplitude in standard deviations";
        options.xMaximum = texture.kurtosisTimes.back();
        options.rowValues = rows;
        options.floor = -40.0;
        options.ceiling = -10.0;
        options.height = 360;
        text << writeView(renderHeatmap(values, options), folder, stem, "histogram", "short-time histogram");
        ChartSeries kurtosis;
        kurtosis.name = "excess kurtosis (0: Gaussian)";
        kurtosis.colour = getChartColour(2);
        kurtosis.x = texture.kurtosisTimes;
        kurtosis.y = texture.kurtosis;
        double largest = 1.0;
        for (const double value : texture.kurtosis)
        {
            largest = std::max(largest, value);
        }
        ChartOptions kurtosisOptions;
        kurtosisOptions.title = name + ": kurtosis per block of 1024 samples";
        kurtosisOptions.xLabel = "time in s after the onset";
        kurtosisOptions.yLabel = "excess kurtosis";
        kurtosisOptions.xMaximum = texture.kurtosisTimes.back();
        kurtosisOptions.yMinimum = -1.0;
        kurtosisOptions.yMaximum = std::min(largest * 1.1, 60.0);
        kurtosisOptions.height = 300;
        text << writeView(renderChart({kurtosis}, kurtosisOptions), folder, stem, "kurtosis", "kurtosis");
    }

    // 8. the normalized autocorrelation
    if (texture.autocorrelation.size() > 1)
    {
        ChartSeries correlation;
        correlation.colour = getChartColour(3);
        correlation.thickness = 1.0f;
        for (size_t lag = 0; lag < texture.autocorrelation.size(); ++lag)
        {
            correlation.x.push_back(1000.0 * lag / fs);
            correlation.y.push_back(texture.autocorrelation[lag]);
        }
        ChartOptions options;
        options.title = name + ": normalized autocorrelation of the tail (repetitions show as peaks away from 0)";
        options.xLabel = "lag in ms";
        options.yLabel = "correlation";
        options.xMaximum = correlation.x.back();
        options.yMinimum = -0.4;
        options.yMaximum = 1.0;
        options.markers = {{1000.0 * texture.repetitionLagSeconds, "largest " + juce::String(texture.repetitionStrength, 2)}};
        text << writeView(renderChart({correlation}, options), folder, stem, "autocorrelation", "normalized autocorrelation");
    }

    // 9. ... 11. stereo
    if (reverb.stereo && ! reverb.right.empty())
    {
        const measure::Coherence coherence = measure::getCoherence(reverb.left, reverb.right, direct + static_cast<int>(0.08 * fs), end, fs);
        ChartSeries line;
        line.colour = getChartColour(4);
        line.x = coherence.frequencies;
        line.y = coherence.coherence;
        ChartOptions options;
        options.title = name + ": coherence of L and R in the tail (after 80 ms)";
        options.xLabel = "frequency in Hz";
        options.yLabel = "magnitude-squared coherence";
        options.logX = true;
        options.xMinimum = 50.0;
        options.xMaximum = std::min(20000.0, 0.5 * fs);
        options.yMaximum = 1.0;
        text << writeView(renderChart({line}, options), folder, stem, "coherence", "coherence");

        const size_t length = static_cast<size_t>(std::min(end, static_cast<int>(reverb.left.size())));
        const std::vector<double> left(reverb.left.begin(), reverb.left.begin() + static_cast<std::ptrdiff_t>(length));
        const std::vector<double> right(reverb.right.begin(), reverb.right.begin() + static_cast<std::ptrdiff_t>(std::min(length, reverb.right.size())));
        const measure::InterauralDifference iad = measure::getInterauralDifference(left, right, fs);
        std::vector<double> rows;
        for (const measure::Band& band : iad.bands)
        {
            rows.push_back(band.centreHz);
        }
        HeatmapOptions iadOptions;
        iadOptions.title = name + ": interaural difference 10 lg(<(l - r)^2> / <l^2 + r^2>) per octave (40 ms)";
        iadOptions.xLabel = "time in s";
        iadOptions.yLabel = "octave in Hz";
        iadOptions.xMaximum = std::min(getLast(iad.times, 1.0), shownSeconds);
        std::vector<std::vector<double>> iadShown = iad.normalizedDb;
        const size_t iadFrames = static_cast<size_t>(std::count_if(iad.times.begin(), iad.times.end(), [shownSeconds](double t) { return t <= shownSeconds; }));
        for (std::vector<double>& line : iadShown)
        {
            line.resize(std::min(line.size(), std::max<size_t>(1, iadFrames)));
        }
        iadOptions.rowValues = rows;
        iadOptions.logY = true;
        iadOptions.floor = -20.0;
        iadOptions.ceiling = 5.0;
        iadOptions.height = 340;
        text << writeView(renderHeatmap(iadShown, iadOptions), folder, stem, "iad", "interaural difference");

        const measure::SpatialDistribution sdm = measure::getSpatialDistribution(left, right, fs);
        // every block relative to its own loudest sector (TimePhaseScope.m's bNormalize: the decay removed, the direction over the whole tail)
        std::vector<std::vector<double>> levels = sdm.levelsDb;
        const size_t blocks = static_cast<size_t>(std::count_if(sdm.times.begin(), sdm.times.end(), [shownSeconds](double t) { return t <= shownSeconds; }));
        for (size_t block = 0; block < sdm.times.size(); ++block)
        {
            double loudest = -300.0;
            for (const std::vector<double>& sector : sdm.levelsDb)
            {
                loudest = std::max(loudest, sector[block]);
            }
            constexpr double kEmpty = -89.0; // every sector at the floor (-90 dB): a silent block stays dark
            for (std::vector<double>& sector : levels)
            {
                if (loudest <= kEmpty)
                {
                    sector[block] = -100.0;
                }
                else
                {
                    sector[block] -= loudest;
                }
            }
        }
        for (std::vector<double>& sector : levels)
        {
            sector.resize(std::min(sector.size(), std::max<size_t>(1, blocks)));
        }
        HeatmapOptions sdmOptions;
        sdmOptions.title = name + ": Spatial Distribution Meter (direction over time; every block re its loudest direction)";
        sdmOptions.xLabel = "time in s";
        sdmOptions.yLabel = "direction";
        sdmOptions.xMaximum = std::min(getLast(sdm.times, 1.0), shownSeconds);
        sdmOptions.floor = -30.0;
        sdmOptions.rowValues = sdm.sectorCentres;
        sdmOptions.rowLabels = {{-kPi / 2.0, "L = -R"}, {-kPi / 4.0, "L"}, {0.0, "C"}, {kPi / 4.0, "R"}, {kPi / 2.0, "R = -L"}};
        text << writeView(renderHeatmap(levels, sdmOptions), folder, stem, "sdm", "Spatial Distribution Meter");
    }
    return text;
}
}
