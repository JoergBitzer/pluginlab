#include "pluginlab/eqmodel/BandFit.h"

#include <algorithm>
#include <cmath>

namespace pluginlab::eqmodel
{
namespace
{
namespace ref = pluginlab::reference;

constexpr double kLowestQ = 0.1;
constexpr double kLowestPeakQ = 0.2;
constexpr double kLowestShelfQ = 0.3;
constexpr double kHighestQ = 30.0;
constexpr double kLargestGainDb = 30.0;
constexpr int kIterations = 150;
constexpr int kExactIterations = 8;
constexpr double kStep = 1.0e-6;
constexpr double kLinearPhaseDegrees = 2.0;   // the phase without the delay stays within this: linear phase
constexpr double kHalf = 0.5;
constexpr int kMaximumPassOrder = 8;
constexpr double kStopBandFloorDb = 90.0;     // below the largest measured level: noise of the measurement, not a shape to fit         // Butterworth orders of the model

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-12));
}

// The free parameters of a band: log10 frequency, gain, log10 Q (as far as the type has them)
bool hasGain(ref::FilterType type)
{
    return type == ref::FilterType::Peak || type == ref::FilterType::LowShelf || type == ref::FilterType::HighShelf;
}

bool hasQ(const Band& band)
{
    if (band.family == DesignFamily::Butterworth || band.family == DesignFamily::LinkwitzRiley)
    {
        return false;
    }
    if (band.family == DesignFamily::Zoelzer && band.type != ref::FilterType::Peak)
    {
        return false;
    }
    return true;
}

// a very broad peak imitates a shelf and an offset: peaks stay above Q 0.2, shelves and pass filters above 0.3
double getLowestQ(ref::FilterType type)
{
    if (type == ref::FilterType::Peak)
    {
        return kLowestPeakQ;
    }
    return kLowestShelfQ;
}

std::vector<double> getParameters(const EqModel& model)
{
    std::vector<double> parameters{model.outputGainDb};
    for (const Band& band : model.bands)
    {
        parameters.push_back(std::log10(band.frequencyHz));
        if (hasGain(band.type))
        {
            parameters.push_back(band.gainDb);
        }
        if (hasQ(band))
        {
            parameters.push_back(std::log10(band.q));
        }
    }
    return parameters;
}

void setParameters(EqModel& model, const std::vector<double>& parameters, double sampleRate)
{
    size_t index = 0;
    model.outputGainDb = parameters[index++];
    for (Band& band : model.bands)
    {
        band.frequencyHz = std::clamp(std::pow(10.0, parameters[index++]), 10.0, 0.45 * sampleRate);
        if (hasGain(band.type))
        {
            band.gainDb = std::clamp(parameters[index++], -kLargestGainDb, kLargestGainDb);
        }
        if (hasQ(band))
        {
            band.q = std::clamp(std::pow(10.0, parameters[index++]), getLowestQ(band.type), kHighestQ);
        }
    }
}

// The model's magnitude during the fit: linear-phase bands with the RBJ magnitude (their FIR is designed only for the final residual)
std::vector<double> getModelDb(const EqModel& model, const std::vector<double>& frequencies, double sampleRate, bool exact)
{
    EqModel fast = model;
    for (Band& band : fast.bands)
    {
        if (band.family == DesignFamily::LinearPhase && ! exact)
        {
            band.family = DesignFamily::Rbj;
        }
    }
    std::vector<BandDesign> designs;
    for (const Band& band : fast.bands)
    {
        designs.push_back(designBand(band, sampleRate));
    }
    std::vector<double> levels;
    for (const double f : frequencies)
    {
        std::complex<double> response = std::pow(10.0, fast.outputGainDb / 20.0);
        for (size_t index = 0; index < fast.bands.size(); ++index)
        {
            response *= getBandResponse(designs[index], fast.bands[index], f, sampleRate);
        }
        levels.push_back(toDb(std::abs(response)));
    }
    return levels;
}

double getRms(const std::vector<double>& values)
{
    double sum = 0.0;
    for (const double value : values)
    {
        sum += value * value;
    }
    return std::sqrt(sum / std::max<size_t>(values.size(), 1));
}

// The floor of a deep stop band: more than kStopBandFloorDb below the largest measured level the measurement is noise (a steep low-pass reaches
// the floor of the sweep within the audio band); the model and the measurement are both clamped there
double getFloorDb(const std::vector<double>& measuredDb)
{
    return *std::max_element(measuredDb.begin(), measuredDb.end()) - kStopBandFloorDb;
}

std::vector<double> getResidual(const EqModel& model, const std::vector<double>& frequencies, const std::vector<double>& measuredDb, double sampleRate,
                                bool exact = false)
{
    std::vector<double> residual = getModelDb(model, frequencies, sampleRate, exact);
    const double floorDb = getFloorDb(measuredDb);
    for (size_t index = 0; index < residual.size(); ++index)
    {
        residual[index] = std::max(residual[index], floorDb) - std::max(measuredDb[index], floorDb);
    }
    return residual;
}

// Solves A x = b (small, dense; Gaussian elimination with partial pivoting)
std::vector<double> solve(std::vector<std::vector<double>> a, std::vector<double> b)
{
    const size_t n = b.size();
    for (size_t column = 0; column < n; ++column)
    {
        size_t pivot = column;
        for (size_t row = column + 1; row < n; ++row)
        {
            if (std::abs(a[row][column]) > std::abs(a[pivot][column]))
            {
                pivot = row;
            }
        }
        std::swap(a[column], a[pivot]);
        std::swap(b[column], b[pivot]);
        if (std::abs(a[column][column]) < 1.0e-300)
        {
            continue;
        }
        for (size_t row = column + 1; row < n; ++row)
        {
            const double factor = a[row][column] / a[column][column];
            for (size_t k = column; k < n; ++k)
            {
                a[row][k] -= factor * a[column][k];
            }
            b[row] -= factor * b[column];
        }
    }
    std::vector<double> x(n, 0.0);
    for (size_t row = n; row-- > 0;)
    {
        double sum = b[row];
        for (size_t k = row + 1; k < n; ++k)
        {
            sum -= a[row][k] * x[k];
        }
        if (std::abs(a[row][row]) > 1.0e-300)
        {
            x[row] = sum / a[row][row];
        }
    }
    return x;
}

// Levenberg-Marquardt on all parameters of the model; returns the rms residual
double refine(EqModel& model, const std::vector<double>& frequencies, const std::vector<double>& measuredDb, double sampleRate, bool exact = false,
              int iterations = kIterations)
{
    std::vector<double> parameters = getParameters(model);
    std::vector<double> residual = getResidual(model, frequencies, measuredDb, sampleRate, exact);
    double cost = getRms(residual);
    double lambda = 1.0e-3;
    const size_t count = parameters.size();
    for (int iteration = 0; iteration < iterations; ++iteration)
    {
        // the Jacobian by forward differences
        std::vector<std::vector<double>> jacobian(residual.size(), std::vector<double>(count, 0.0));
        for (size_t p = 0; p < count; ++p)
        {
            std::vector<double> shifted = parameters;
            const double step = kStep * std::max(1.0, std::abs(parameters[p]));
            shifted[p] += step;
            EqModel trial = model;
            setParameters(trial, shifted, sampleRate);
            const std::vector<double> moved = getResidual(trial, frequencies, measuredDb, sampleRate, exact);
            for (size_t k = 0; k < residual.size(); ++k)
            {
                jacobian[k][p] = (moved[k] - residual[k]) / step;
            }
        }
        std::vector<std::vector<double>> normal(count, std::vector<double>(count, 0.0));
        std::vector<double> gradient(count, 0.0);
        for (size_t k = 0; k < residual.size(); ++k)
        {
            for (size_t i = 0; i < count; ++i)
            {
                gradient[i] -= jacobian[k][i] * residual[k];
                for (size_t j = 0; j < count; ++j)
                {
                    normal[i][j] += jacobian[k][i] * jacobian[k][j];
                }
            }
        }
        bool improved = false;
        for (int attempt = 0; attempt < 10 && ! improved; ++attempt)
        {
            std::vector<std::vector<double>> damped = normal;
            for (size_t i = 0; i < count; ++i)
            {
                damped[i][i] += lambda * std::max(normal[i][i], 1.0e-12);
            }
            const std::vector<double> delta = solve(damped, gradient);
            std::vector<double> next = parameters;
            for (size_t i = 0; i < count; ++i)
            {
                next[i] += delta[i];
            }
            EqModel trial = model;
            setParameters(trial, next, sampleRate);
            const std::vector<double> trialResidual = getResidual(trial, frequencies, measuredDb, sampleRate, exact);
            const double trialCost = getRms(trialResidual);
            if (trialCost < cost)
            {
                parameters = getParameters(trial);
                model = trial;
                residual = trialResidual;
                const double gain = cost - trialCost;
                cost = trialCost;
                lambda = std::max(lambda * 0.3, 1.0e-12);
                improved = true;
                if (gain < 1.0e-10)
                {
                    return cost;
                }
            }
            else
            {
                lambda *= 10.0;
            }
        }
        if (! improved)
        {
            break;
        }
    }
    return cost;
}

// The frequency where the residual crosses a level, searching from an index outwards (left: towards lower frequencies)
double findCrossing(const std::vector<double>& frequencies, const std::vector<double>& values, size_t from, double level, bool left)
{
    if (left)
    {
        for (size_t index = from; index-- > 0;)
        {
            if ((values[index] - level) * (values[from] - level) <= 0.0)
            {
                return frequencies[index];
            }
        }
        return frequencies.front();
    }
    for (size_t index = from + 1; index < values.size(); ++index)
    {
        if ((values[index] - level) * (values[from] - level) <= 0.0)
        {
            return frequencies[index];
        }
    }
    return frequencies.back();
}

// The value at a frequency (the nearest grid point on the log axis)
double getValueAt(const std::vector<double>& frequencies, const std::vector<double>& values, double frequencyHz)
{
    size_t best = 0;
    for (size_t index = 1; index < frequencies.size(); ++index)
    {
        if (std::abs(std::log(frequencies[index] / frequencyHz)) < std::abs(std::log(frequencies[best] / frequencyHz)))
        {
            best = index;
        }
    }
    return values[best];
}

// Candidate bands for the largest remaining feature of the residual (measured - model)
std::vector<Band> findCandidates(const std::vector<double>& frequencies, const std::vector<double>& difference)
{
    std::vector<Band> candidates;
    std::vector<Band> passes;      // first in the list when an end falls steeply
    const size_t last = difference.size() - 1;
    // a peak or dip: the largest deviation
    size_t extreme = 0;
    for (size_t index = 1; index < difference.size(); ++index)
    {
        if (std::abs(difference[index]) > std::abs(difference[extreme]))
        {
            extreme = index;
        }
    }
    {
        Band peak;
        peak.type = ref::FilterType::Peak;
        peak.frequencyHz = frequencies[extreme];
        peak.gainDb = difference[extreme];
        const double low = findCrossing(frequencies, difference, extreme, kHalf * difference[extreme], true);
        const double high = findCrossing(frequencies, difference, extreme, kHalf * difference[extreme], false);
        peak.q = std::clamp(getQFromBandwidthAnalog(std::max(std::log2(high / low), 0.05)), kLowestPeakQ, kHighestQ);
        candidates.push_back(peak);
        Band orfanidis = peak;
        orfanidis.family = DesignFamily::Orfanidis;
        candidates.push_back(orfanidis);
    }
    // shelves: the level at an end relative to the other end
    for (const bool lowEnd : {true, false})
    {
        Band shelf;
        shelf.type = ref::FilterType::HighShelf;
        size_t end = last;
        ref::FilterType passType = ref::FilterType::LowPass;
        if (lowEnd)
        {
            shelf.type = ref::FilterType::LowShelf;
            end = 0;
            passType = ref::FilterType::HighPass;
        }
        shelf.gainDb = difference[end];
        shelf.frequencyHz = findCrossing(frequencies, difference, end, kHalf * difference[end], ! lowEnd);
        shelf.q = 0.71;
        candidates.push_back(shelf);
        // a pass filter where the end falls steeply
        Band pass;
        pass.type = passType;
        pass.frequencyHz = findCrossing(frequencies, difference, end, -3.0, ! lowEnd);
        pass.q = 0.71;
        if (difference[end] < -6.0)
        {
            passes.push_back(pass);
            // the order from the slope: a Butterworth filter is 10 lg(1 + 2^(2n)) ~ 6.02 n dB down one octave beyond its -3 dB point; that
            // order and its neighbours (1 ... 8, the orders the model has)
            const double octaveBeyond = lowEnd ? pass.frequencyHz / 2.0 : pass.frequencyHz * 2.0;
            const bool insideGrid = octaveBeyond >= frequencies.front() && octaveBeyond <= frequencies.back();
            const int estimate = insideGrid ? juce::jlimit(1, kMaximumPassOrder, static_cast<int>(std::round(-getValueAt(frequencies, difference, octaveBeyond) / 6.02))) : 2;
            for (int order = std::max(1, estimate - 1); order <= std::min(kMaximumPassOrder, estimate + 1); ++order)
            {
                Band butterworth = pass;
                butterworth.family = DesignFamily::Butterworth;
                butterworth.order = order;
                passes.push_back(butterworth);
            }
        }
    }
    passes.insert(passes.end(), candidates.begin(), candidates.end());
    return passes;
}

juce::String describeBand(const Band& band)
{
    juce::String text = juce::String(getFamilyName(band.family)) + " " + ref::getFilterTypeName(band.type) + " " + juce::String(band.frequencyHz, 1) + " Hz";
    if (hasGain(band.type))
    {
        text << ", " << juce::String(band.gainDb, 2) << " dB";
    }
    if (hasQ(band))
    {
        text << ", Q " << juce::String(band.q, 3);
    }
    if (band.family == DesignFamily::Butterworth || band.family == DesignFamily::LinkwitzRiley)
    {
        text << ", order " << band.order;
    }
    return text;
}
double growGreedily(EqModel& model, double cost, const std::vector<double>& frequencies, const std::vector<double>& measuredDb, const FitSettings& settings)
{
    const double fs = settings.sampleRate;
    while (static_cast<int>(model.bands.size()) < settings.maximumBands && cost > 0.1 * settings.goodResidualDb)
    {
        std::vector<double> difference = getResidual(model, frequencies, measuredDb, fs);
        for (double& value : difference)
        {
            value = -value; // measured - model
        }
        EqModel best = model;
        double bestCost = cost;
        for (const Band& candidate : findCandidates(frequencies, difference))
        {
            EqModel trial = model;
            trial.bands.push_back(candidate);
            const double trialCost = refine(trial, frequencies, measuredDb, fs);
            if (trialCost < bestCost)
            {
                best = trial;
                bestCost = trialCost;
            }
        }
        if (bestCost > 0.9 * cost)
        {
            break; // no band helps any more
        }
        model = best;
        cost = bestCost;
    }
    return cost;
}

// removes bands without which the model stays good (the greedy search may have used two bands for one feature)
double prune(EqModel& model, double cost, const std::vector<double>& frequencies, const std::vector<double>& measuredDb, const FitSettings& settings)
{
    bool removed = true;
    while (removed && model.bands.size() > 1)
    {
        removed = false;
        for (size_t index = 0; index < model.bands.size(); ++index)
        {
            EqModel trial = model;
            trial.bands.erase(trial.bands.begin() + static_cast<std::ptrdiff_t>(index));
            const double trialCost = refine(trial, frequencies, measuredDb, settings.sampleRate);
            if (trialCost < std::max(0.1 * settings.goodResidualDb, 1.1 * cost))
            {
                model = trial;
                cost = trialCost;
                removed = true;
                break;
            }
        }
    }
    return cost;
}
}

std::vector<double> makeLogGrid(double lowestHz, double highestHz, double perOctave)
{
    std::vector<double> grid;
    for (double f = lowestHz; f <= highestHz * 1.0001; f *= std::pow(2.0, 1.0 / perOctave))
    {
        grid.push_back(f);
    }
    return grid;
}

FitResult fitEqModel(const std::vector<double>& frequencies, const std::vector<std::complex<double>>& response, const FitSettings& settings)
{
    FitResult result;
    const double fs = settings.sampleRate;
    std::vector<double> measuredDb;
    for (const std::complex<double>& value : response)
    {
        measuredDb.push_back(toDb(std::abs(value)));
    }

    // the delay: given, or the group delay at mid band from the phase slope; then the phase test
    int latency = settings.latencySamples;
    if (latency < 0)
    {
        // the group delay at the lowest frequencies, where neighbouring points are close enough not to wrap the phase
        const double dPhase = std::arg(response[1] * std::conj(response[0]));
        const double dOmega = 2.0 * juce::MathConstants<double>::pi * (frequencies[1] - frequencies[0]) / fs;
        latency = std::max(0, juce::roundToInt(-dPhase / dOmega));
    }
    result.latencySamples = latency;
    double largestPhase = 0.0;
    for (size_t index = 0; index < frequencies.size(); ++index)
    {
        const std::complex<double> zeroPhase = response[index] * std::polar(1.0, 2.0 * juce::MathConstants<double>::pi * frequencies[index] * latency / fs);
        // a real response (positive or negative) has phase 0 or 180 degrees
        const double phase = std::abs(std::atan(zeroPhase.imag() / std::max(std::abs(zeroPhase.real()), 1.0e-300))) * 180.0 / juce::MathConstants<double>::pi;
        largestPhase = std::max(largestPhase, phase);
    }
    double range = 0.0;
    for (const double level : measuredDb)
    {
        range = std::max(range, std::abs(level - measuredDb.front()));
    }
    result.linearPhase = latency > 0 && largestPhase < kLinearPhaseDegrees && range > 0.5;

    // greedy, started once with every candidate for the first band: the best candidate for the largest remaining feature, then all bands refitted;
    // afterwards bands that are not needed are removed. Of the starts, the good one with the fewest bands, else the smallest residual.
    EqModel flat;
    double sum = 0.0;
    for (const double level : measuredDb)
    {
        sum += level;
    }
    flat.outputGainDb = sum / static_cast<double>(measuredDb.size());
    const double flatCost = refine(flat, frequencies, measuredDb, fs);
    const double target = 0.1 * settings.goodResidualDb;
    EqModel model = flat;
    double cost = flatCost;
    if (flatCost > target)
    {
        std::vector<double> difference = getResidual(flat, frequencies, measuredDb, fs);
        for (double& value : difference)
        {
            value = -value;
        }
        bool found = false;
        for (const Band& first : findCandidates(frequencies, difference))
        {
            EqModel start = flat;
            start.bands.push_back(first);
            double startCost = refine(start, frequencies, measuredDb, fs);
            startCost = growGreedily(start, startCost, frequencies, measuredDb, settings);
            startCost = prune(start, startCost, frequencies, measuredDb, settings);
            const bool good = startCost < settings.goodResidualDb;
            const bool better = ! found || (good && cost >= settings.goodResidualDb) || (good && start.bands.size() < model.bands.size())
                                || (good == (cost < settings.goodResidualDb) && start.bands.size() == model.bands.size() && startCost < cost)
                                || (! good && cost >= settings.goodResidualDb && startCost < cost);
            if (better)
            {
                model = start;
                cost = startCost;
                found = true;
            }
            if (startCost < target && start.bands.size() == 1)
            {
                break; // one band describes it exactly: no other start can need fewer (two shelves can imitate a high-pass within 0.004 dB)
            }
        }
    }
    // distinguishable families: Orfanidis for a peak (near Nyquist it differs from RBJ)
    for (size_t index = 0; index < model.bands.size(); ++index)
    {
        if (model.bands[index].type != ref::FilterType::Peak)
        {
            continue;
        }
        EqModel trial = model;
        trial.bands[index].family = DesignFamily::Orfanidis;
        const double trialCost = refine(trial, frequencies, measuredDb, fs);
        if (trialCost < 0.8 * cost)
        {
            model = trial;
            cost = trialCost;
        }
    }
    if (result.linearPhase)
    {
        for (Band& band : model.bands)
        {
            band.family = DesignFamily::LinearPhase;
        }
        // a few iterations with the real FIR: its magnitude deviates from RBJ by up to 0.05 dB
        cost = refine(model, frequencies, measuredDb, fs, true, kExactIterations);
    }
    // the final residual with the true designs (linear phase: the FIR)
    std::vector<double> modelDb;
    for (const double f : frequencies)
    {
        modelDb.push_back(toDb(std::abs(getModelResponse(model, f, fs))));
    }
    double squares = 0.0;
    for (size_t index = 0; index < modelDb.size(); ++index)
    {
        const double residual = std::max(modelDb[index], getFloorDb(measuredDb)) - std::max(measuredDb[index], getFloorDb(measuredDb));
        squares += residual * residual;
        result.largestResidualDb = std::max(result.largestResidualDb, std::abs(residual));
    }
    result.rmsResidualDb = std::sqrt(squares / static_cast<double>(modelDb.size()));
    result.goodDescription = result.rmsResidualDb < settings.acceptableResidualDb;
    result.model = model;
    for (const Band& band : model.bands)
    {
        result.summary << describeBand(band) << "\n";
    }
    result.summary << "output gain " << juce::String(model.outputGainDb, 2) << " dB; residual " << juce::String(result.rmsResidualDb, 4) << " dB rms";
    return result;
}
}
