#include <cmath>
#include <complex>
#include <functional>
#include <iostream>
#include <vector>

#include "SvgPlot.h"
#include "pluginlab/reference/Analog.h"
#include "pluginlab/reference/Delays.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/LinearPhaseFir.h"
#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

// PluginLabTeachingFigures <folder>: the figures of the teaching pages (docs/teaching/references), computed with pluginlab_reference.
namespace
{
namespace ref = pluginlab::reference;

constexpr int kExitOk = 0;
constexpr int kExitWrongArguments = 2;
constexpr int kExitCannotWrite = 3;
constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr double kLowestHz = 20.0;
constexpr double kHighestHz = 20000.0;
constexpr int kPoints = 400;
constexpr double kGroupDelayStepHz = 1.0;

std::vector<double> logGrid(double from, double to, int points)
{
    std::vector<double> grid;
    for (int index = 0; index < points; ++index)
    {
        grid.push_back(from * std::pow(to / from, static_cast<double>(index) / (points - 1)));
    }
    return grid;
}

std::vector<double> linearGrid(double from, double to, int points)
{
    std::vector<double> grid;
    for (int index = 0; index < points; ++index)
    {
        grid.push_back(from + (to - from) * index / (points - 1));
    }
    return grid;
}

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-12));
}

SvgPlot::Series makeSeries(const juce::String& name, const std::vector<double>& x, const std::function<double(double)>& y, bool dashed = false)
{
    SvgPlot::Series series;
    series.name = name;
    series.x = x;
    series.dashed = dashed;
    for (const double value : x)
    {
        series.y.push_back(y(value));
    }
    return series;
}

SvgPlot::Series magnitude(const juce::String& name, const std::vector<double>& frequencies, const std::function<std::complex<double>(double)>& response,
                          bool dashed = false)
{
    return makeSeries(name, frequencies, [&response](double f) { return toDb(std::abs(response(f))); }, dashed);
}

SvgPlot makeResponsePlot(const juce::String& title, double yMin, double yMax)
{
    SvgPlot plot(title, "frequency in Hz", "magnitude in dB");
    plot.setLogX(true);
    plot.setXRange(kLowestHz, kHighestHz);
    plot.setYRange(yMin, yMax);
    return plot;
}

std::function<std::complex<double>(double)> rbj(ref::FilterType type, double f0, double gainDb, double q)
{
    const ref::BiquadCoefficients section = ref::designRbj(type, kSampleRate, f0, gainDb, q);
    return [section](double f) { return ref::getBiquadResponse(section, f, kSampleRate); };
}

double groupDelaySamples(const std::function<std::complex<double>(double)>& response, double f, double sampleRate)
{
    const double low = std::max(f - kGroupDelayStepHz, 0.001);
    const double high = f + kGroupDelayStepHz;
    const double phaseStep = std::arg(response(high) / response(low));
    return -phaseStep / (2.0 * kPi * (high - low) / sampleRate);
}

// THD of a sine of the amplitude through the shaper (harmonics 2 ... 15), in dB re the fundamental
double getThdDb(const ref::Waveshaper& shaper, double amplitude)
{
    const std::vector<double> harmonics = ref::getShaperHarmonics(shaper, amplitude, 15, 1 << 14);
    double power = 0.0;
    for (size_t harmonic = 2; harmonic < harmonics.size(); ++harmonic)
    {
        power += harmonics[harmonic] * harmonics[harmonic];
    }
    return 10.0 * std::log10(std::max(power, 1.0e-30)) - toDb(harmonics[1]);
}

// SNR of a 997 Hz sine of amplitude 1 - 4 q through the quantizer
double measureSnr(int bits, bool dither)
{
    constexpr int kLength = 1 << 15;
    ref::Quantizer quantizer(bits, dither);
    const double amplitude = 1.0 - 4.0 * quantizer.getStep();
    juce::AudioBuffer<float> buffer(1, kLength);
    for (int index = 0; index < kLength; ++index)
    {
        buffer.setSample(0, index, static_cast<float>(amplitude * std::sin(2.0 * kPi * 997.0 * index / kSampleRate)));
    }
    juce::AudioBuffer<float> input;
    input.makeCopyOf(buffer);
    quantizer.process(buffer);
    double error = 0.0;
    for (int index = 0; index < kLength; ++index)
    {
        const double difference = static_cast<double>(buffer.getSample(0, index)) - input.getSample(0, index);
        error += difference * difference;
    }
    return 10.0 * std::log10(amplitude * amplitude / 2.0 / (error / kLength));
}

std::vector<std::pair<juce::String, SvgPlot>> makeFigures()
{
    std::vector<std::pair<juce::String, SvgPlot>> figures;
    const std::vector<double> grid = logGrid(kLowestHz, kHighestHz, kPoints);

    {
        SvgPlot plot = makeResponsePlot("RBJ cookbook: pass and band types, 1 kHz, Q 1, 48 kHz", -40.0, 5.0);
        plot.addSeries(magnitude("low-pass", grid, rbj(ref::FilterType::LowPass, 1000.0, 0.0, 1.0)));
        plot.addSeries(magnitude("high-pass", grid, rbj(ref::FilterType::HighPass, 1000.0, 0.0, 1.0)));
        plot.addSeries(magnitude("band-pass (0 dB peak)", grid, rbj(ref::FilterType::BandPassUnity, 1000.0, 0.0, 1.0)));
        plot.addSeries(magnitude("notch", grid, rbj(ref::FilterType::Notch, 1000.0, 0.0, 1.0)));
        figures.emplace_back("rbj_pass_types.svg", plot);
    }
    {
        SvgPlot plot = makeResponsePlot("RBJ cookbook: peak and shelves (+9 dB), 48 kHz", -12.0, 12.0);
        plot.addSeries(magnitude("low shelf 200 Hz, Q 0.71", grid, rbj(ref::FilterType::LowShelf, 200.0, 9.0, 0.71)));
        plot.addSeries(magnitude("peak 1 kHz, Q 1", grid, rbj(ref::FilterType::Peak, 1000.0, 9.0, 1.0)));
        plot.addSeries(magnitude("high shelf 5 kHz, Q 0.71", grid, rbj(ref::FilterType::HighShelf, 5000.0, 9.0, 0.71)));
        plot.addSeries(magnitude("peak 1 kHz, -9 dB (mirror)", grid, rbj(ref::FilterType::Peak, 1000.0, -9.0, 1.0)));
        figures.emplace_back("rbj_eq_types.svg", plot);
    }
    {
        SvgPlot plot = makeResponsePlot("RBJ peak +12 dB at 1 kHz: the bandwidth follows Q", -2.0, 14.0);
        for (const double q : {0.5, 1.0, 2.0, 8.0})
        {
            plot.addSeries(magnitude("Q " + formatTick(q), grid, rbj(ref::FilterType::Peak, 1000.0, 12.0, q)));
        }
        figures.emplace_back("rbj_peak_q.svg", plot);
    }
    {
        SvgPlot plot = makeResponsePlot("Cramping: peak +12 dB, Q 1 at 10 kHz, 48 kHz", -2.0, 14.0);
        plot.setXRange(1000.0, 24000.0);
        const std::vector<double> high = logGrid(1000.0, 23990.0, kPoints);
        plot.addSeries(magnitude("analog prototype", high, [](double f) { return ref::getAnalogResponse(ref::FilterType::Peak, f, 10000.0, 12.0, 1.0); }, true));
        plot.addSeries(magnitude("RBJ (bilinear)", high, rbj(ref::FilterType::Peak, 10000.0, 12.0, 1.0)));
        const ref::BiquadCoefficients orfanidis = ref::designOrfanidisPeak(kSampleRate, 10000.0, 12.0, 1.0);
        plot.addSeries(magnitude("Orfanidis", high, [orfanidis](double f) { return ref::getBiquadResponse(orfanidis, f, kSampleRate); }));
        figures.emplace_back("cramping.svg", plot);
    }
    {
        SvgPlot plot("RBJ all-pass at 1 kHz: phase", "frequency in Hz", "phase in degrees");
        plot.setLogX(true);
        plot.setXRange(kLowestHz, kHighestHz);
        plot.setYRange(-360.0, 0.0);
        for (const double q : {0.71, 4.0})
        {
            const auto response = rbj(ref::FilterType::AllPass, 1000.0, 0.0, q);
            plot.addSeries(makeSeries("Q " + formatTick(q), grid, [&response](double f)
                                      {
                                          double degrees = std::arg(response(f)) * 180.0 / kPi;
                                          if (degrees > 0.0)
                                          {
                                              degrees -= 360.0; // unwrapped: the all-pass goes from 0 to -360 degrees
                                          }
                                          return degrees;
                                      }));
        }
        figures.emplace_back("rbj_allpass_phase.svg", plot);
    }
    {
        SvgPlot plot = makeResponsePlot("Zoelzer shelves at 300 Hz: boost and cut are mirror images", -12.0, 12.0);
        for (const auto& [type, name] : {std::pair<ref::ZoelzerType, juce::String>{ref::ZoelzerType::LowShelfFirstOrder, "1st order"},
                                          std::pair<ref::ZoelzerType, juce::String>{ref::ZoelzerType::LowShelf, "2nd order"}})
        {
            for (const double gain : {9.0, -9.0})
            {
                const ref::BiquadCoefficients section = ref::designZoelzer(type, kSampleRate, 300.0, gain, 0.71);
                juce::String label = name + ", +9 dB";
                if (gain < 0.0)
                {
                    label = name + ", -9 dB";
                }
                plot.addSeries(magnitude(label, grid, [section](double f) { return ref::getBiquadResponse(section, f, kSampleRate); }));
            }
        }
        figures.emplace_back("zoelzer_shelves.svg", plot);
    }
    {
        SvgPlot plot = makeResponsePlot("Butterworth low-pass at 1 kHz: 6 dB per octave and order", -80.0, 5.0);
        for (const int order : {1, 2, 4, 8})
        {
            const std::vector<ref::BiquadCoefficients> sections = ref::designButterworth(ref::Pass::Low, order, kSampleRate, 1000.0);
            plot.addSeries(magnitude("order " + juce::String(order), grid, [sections](double f) { return ref::getCascadeResponse(sections, f, kSampleRate); }));
        }
        figures.emplace_back("butterworth_orders.svg", plot);
    }
    {
        SvgPlot plot = makeResponsePlot("Linkwitz-Riley 4th order at 1 kHz: low + high = flat", -40.0, 5.0);
        const std::vector<ref::BiquadCoefficients> low = ref::designLinkwitzRiley(ref::Pass::Low, 4, kSampleRate, 1000.0);
        const std::vector<ref::BiquadCoefficients> high = ref::designLinkwitzRiley(ref::Pass::High, 4, kSampleRate, 1000.0);
        plot.addSeries(magnitude("low-pass", grid, [low](double f) { return ref::getCascadeResponse(low, f, kSampleRate); }));
        plot.addSeries(magnitude("high-pass", grid, [high](double f) { return ref::getCascadeResponse(high, f, kSampleRate); }));
        plot.addSeries(magnitude("sum", grid, [low, high](double f)
                                 { return ref::getCascadeResponse(low, f, kSampleRate) + ref::getCascadeResponse(high, f, kSampleRate); }));
        figures.emplace_back("linkwitz_riley.svg", plot);
    }
    {
        SvgPlot plot = makeResponsePlot("Linear-phase FIR for the RBJ peak (+6 dB, 1 kHz, Q 1): the length decides", -2.0, 8.0);
        const auto target = rbj(ref::FilterType::Peak, 1000.0, 6.0, 1.0);
        const auto magnitudeOfTarget = [target](double f) { return std::abs(target(f)); };
        plot.addSeries(magnitude("target (RBJ magnitude)", grid, target, true));
        for (const int taps : {255, 2047})
        {
            const ref::DirectFormFilter fir = ref::makeLinearPhaseFir(magnitudeOfTarget, kSampleRate, taps);
            plot.addSeries(magnitude(juce::String(taps) + " taps", grid, [fir](double f) { return fir.getResponse(f); }));
        }
        figures.emplace_back("fir_peak.svg", plot);
    }
    {
        SvgPlot plot("Impulse responses: linear phase rings before the peak", "samples relative to the peak", "amplitude");
        plot.setXRange(-64.0, 64.0);
        plot.setYRange(-0.1, 0.25);
        const auto target = rbj(ref::FilterType::Peak, 1000.0, 6.0, 1.0);
        const std::vector<double> taps = ref::designLinearPhaseFir([target](double f) { return std::abs(target(f)); }, kSampleRate, 255);
        SvgPlot::Series fir;
        fir.name = "linear-phase FIR (255 taps, centre 127)";
        for (int index = 0; index < static_cast<int>(taps.size()); ++index)
        {
            fir.x.push_back(index - 127);
            fir.y.push_back(taps[static_cast<size_t>(index)]);
        }
        // the minimum-phase RBJ filter itself: its impulse response minus the direct 1 (the peak) to show the shape
        ref::BiquadCascade cascade({ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 1.0)}, kSampleRate);
        SvgPlot::Series iir;
        iir.name = "RBJ peak (minimum phase)";
        for (int index = 0; index < 64; ++index)
        {
            double input = 0.0;
            if (index == 0)
            {
                input = 1.0;
            }
            iir.x.push_back(index);
            iir.y.push_back(cascade.processSample(0, input));
        }
        plot.addSeries(fir);
        plot.addSeries(iir);
        figures.emplace_back("fir_impulse.svg", plot);
    }
    {
        SvgPlot plot("Fractional delay of 10.5 samples: group delay", "frequency in Hz", "group delay in samples");
        plot.setXRange(0.0, 24000.0);
        plot.setYRange(8.0, 13.0);
        const std::vector<double> linear = linearGrid(50.0, 23500.0, kPoints);
        const std::vector<std::pair<juce::String, ref::DirectFormFilter>> delays = {
            {"Thiran order 1", ref::makeThiranDelay(10.5, 1, kSampleRate)},
            {"Thiran order 3", ref::makeThiranDelay(10.5, 3, kSampleRate)},
            {"Lagrange order 1", ref::makeLagrangeDelay(10.5, 1, kSampleRate)},
            {"Lagrange order 3", ref::makeLagrangeDelay(10.5, 3, kSampleRate)}};
        for (const auto& [name, delay] : delays)
        {
            const ref::DirectFormFilter& filter = delay;
            plot.addSeries(makeSeries(name, linear, [&filter](double f)
                                      { return groupDelaySamples([&filter](double g) { return filter.getResponse(g); }, f, kSampleRate); }));
        }
        figures.emplace_back("delays_group_delay.svg", plot);
    }
    {
        SvgPlot plot("Fractional delay of 10.5 samples: magnitude", "frequency in Hz", "magnitude in dB");
        plot.setXRange(0.0, 24000.0);
        plot.setYRange(-30.0, 2.0);
        const std::vector<double> linear = linearGrid(50.0, 23500.0, kPoints);
        const ref::DirectFormFilter thiran = ref::makeThiranDelay(10.5, 3, kSampleRate);
        const ref::DirectFormFilter lagrange1 = ref::makeLagrangeDelay(10.5, 1, kSampleRate);
        const ref::DirectFormFilter lagrange3 = ref::makeLagrangeDelay(10.5, 3, kSampleRate);
        plot.addSeries(magnitude("Thiran order 3 (all-pass)", linear, [&thiran](double f) { return thiran.getResponse(f); }));
        plot.addSeries(magnitude("Lagrange order 1 (linear interpolation)", linear, [&lagrange1](double f) { return lagrange1.getResponse(f); }));
        plot.addSeries(magnitude("Lagrange order 3", linear, [&lagrange3](double f) { return lagrange3.getResponse(f); }));
        figures.emplace_back("delays_magnitude.svg", plot);
    }
    {
        SvgPlot plot("Memoryless curves y = f(x)", "input x", "output y");
        plot.setXRange(-1.0, 1.0);
        plot.setYRange(-1.1, 1.1);
        const std::vector<double> x = linearGrid(-1.0, 1.0, kPoints);
        const ref::Waveshaper polynomial = ref::Waveshaper::makePolynomial({0.0, 1.0, 0.2, -0.3});
        const ref::Waveshaper hard = ref::Waveshaper::makeHardClip(0.5);
        const ref::Waveshaper soft = ref::Waveshaper::makeSoftClip(2.0);
        plot.addSeries(makeSeries("y = x", x, [](double v) { return v; }, true));
        plot.addSeries(makeSeries("x + 0.2 x^2 - 0.3 x^3", x, [&polynomial](double v) { return polynomial.processSample(v); }));
        plot.addSeries(makeSeries("hard clip at 0.5", x, [&hard](double v) { return hard.processSample(v); }));
        plot.addSeries(makeSeries("tanh(2 x)", x, [&soft](double v) { return soft.processSample(v); }));
        figures.emplace_back("waveshaper_curves.svg", plot);
    }
    {
        SvgPlot plot("THD of a sine against its level", "input level in dBFS (peak)", "THD in dB re fundamental");
        plot.setXRange(-40.0, 0.0);
        plot.setYRange(-100.0, 0.0);
        const std::vector<double> levels = linearGrid(-40.0, 0.0, 81);
        const ref::Waveshaper polynomial = ref::Waveshaper::makePolynomial({0.0, 1.0, 0.1, 0.05});
        const ref::Waveshaper hard = ref::Waveshaper::makeHardClip(0.5);
        const ref::Waveshaper soft = ref::Waveshaper::makeSoftClip(2.0);
        plot.addSeries(makeSeries("x + 0.1 x^2 + 0.05 x^3", levels, [&polynomial](double l) { return getThdDb(polynomial, std::pow(10.0, l / 20.0)); }));
        plot.addSeries(makeSeries("hard clip at 0.5 (-6 dBFS)", levels, [&hard](double l) { return getThdDb(hard, std::pow(10.0, l / 20.0)); }));
        plot.addSeries(makeSeries("tanh(2 x)", levels, [&soft](double l) { return getThdDb(soft, std::pow(10.0, l / 20.0)); }));
        figures.emplace_back("waveshaper_thd.svg", plot);
    }
    {
        SvgPlot plot("Quantizer: SNR of a full-scale sine against the word length", "bits", "SNR in dB");
        plot.setXRange(4.0, 24.0);
        plot.setYRange(0.0, 160.0);
        std::vector<double> bits;
        for (int value = 4; value <= 24; ++value)
        {
            bits.push_back(value);
        }
        plot.addSeries(makeSeries("6.02 N + 1.76 dB", bits, [](double n) { return 6.0206 * n + 1.7609; }, true));
        plot.addSeries(makeSeries("measured, no dither", bits, [](double n) { return measureSnr(static_cast<int>(n), false); }));
        plot.addSeries(makeSeries("measured, TPDF dither", bits, [](double n) { return measureSnr(static_cast<int>(n), true); }));
        figures.emplace_back("quantizer_snr.svg", plot);
    }
    {
        SvgPlot plot("A sine of 2 steps at 8 bits: without and with dither", "time in ms", "value in quantization steps");
        plot.setXRange(0.0, 3.0);
        plot.setYRange(-4.0, 4.0);
        constexpr int kLength = 144; // 3 ms
        ref::Quantizer plain(8, false);
        ref::Quantizer dithered(8, true);
        const double step = plain.getStep();
        juce::AudioBuffer<float> input(1, kLength);
        for (int index = 0; index < kLength; ++index)
        {
            input.setSample(0, index, static_cast<float>(2.0 * step * std::sin(2.0 * kPi * 1000.0 * index / kSampleRate)));
        }
        juce::AudioBuffer<float> withoutDither;
        withoutDither.makeCopyOf(input);
        plain.process(withoutDither);
        juce::AudioBuffer<float> withDither;
        withDither.makeCopyOf(input);
        dithered.process(withDither);
        SvgPlot::Series original;
        SvgPlot::Series quantized;
        SvgPlot::Series ditheredSeries;
        original.name = "input";
        original.dashed = true;
        quantized.name = "quantized";
        ditheredSeries.name = "quantized with TPDF dither";
        for (int index = 0; index < kLength; ++index)
        {
            const double time = 1000.0 * index / kSampleRate;
            original.x.push_back(time);
            original.y.push_back(input.getSample(0, index) / step);
            quantized.x.push_back(time);
            quantized.y.push_back(withoutDither.getSample(0, index) / step);
            ditheredSeries.x.push_back(time);
            ditheredSeries.y.push_back(withDither.getSample(0, index) / step);
        }
        plot.addSeries(original);
        plot.addSeries(quantized);
        plot.addSeries(ditheredSeries);
        figures.emplace_back("quantizer_low_level.svg", plot);
    }
    {
        SvgPlot plot("Tremolo: gain curve, 5 Hz, depth 0.6", "time in s", "gain");
        plot.setXRange(0.0, 0.5);
        plot.setYRange(0.0, 1.1);
        const ref::Tremolo tremolo(kSampleRate, 5.0, 0.6);
        plot.addSeries(makeSeries("gain", linearGrid(0.0, 0.5, kPoints),
                                  [&tremolo](double t) { return tremolo.getGain(static_cast<juce::int64>(std::round(t * kSampleRate))); }));
        figures.emplace_back("tremolo_gain.svg", plot);
    }
    return figures;
}
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: PluginLabTeachingFigures <folder>\n";
        return kExitWrongArguments;
    }
    const juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    if (!folder.createDirectory())
    {
        std::cerr << "Cannot create " << folder.getFullPathName() << "\n";
        return kExitCannotWrite;
    }
    int written = 0;
    for (const auto& [name, plot] : makeFigures())
    {
        if (!plot.write(folder.getChildFile(name)))
        {
            std::cerr << "Cannot write " << name << "\n";
            return kExitCannotWrite;
        }
        ++written;
    }
    std::cout << written << " figures written to " << folder.getFullPathName() << "\n";
    return kExitOk;
}
