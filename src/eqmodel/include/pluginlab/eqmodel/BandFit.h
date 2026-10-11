#pragma once

#include <complex>
#include <vector>

#include "pluginlab/eqmodel/EqModel.h"

namespace pluginlab::eqmodel
{
// Band identification (W8.3, docs/eqmodel/band-identification.md): an EqModel fitted to a measured response. The magnitude in dB on a log grid
// by Levenberg-Marquardt (numerical Jacobian; parameters: log frequency, gain, log Q per band and the output gain); the structure greedily: the
// largest remaining feature (a slope at an end, a plateau difference, a peak or dip) becomes a band, then all bands are refitted. The phase decides
// between minimum phase and linear phase. Peaks and second-order shelves of RBJ and Zoelzer are the same digital shapes up to a mapping of Q: the
// fit reports them in RBJ's terms.
struct FitSettings
{
    double sampleRate = 48000.0;
    int maximumBands = 6;
    double goodResidualDb = 0.05;      // rms residual below this: a good description; the search stops at a tenth of it
    double acceptableResidualDb = 0.5; // rms residual above this: "no good description"
    double lowestHz = 20.0;
    double highestHz = 20000.0;
    int latencySamples = -1;           // the delay to remove before the phase test (-1: from the group delay at mid band)
};

struct FitResult
{
    EqModel model;
    int latencySamples = 0;
    bool linearPhase = false;
    double rmsResidualDb = 0.0;
    double largestResidualDb = 0.0;
    bool goodDescription = false;      // rms residual below acceptableResidualDb
    juce::String summary;              // one line per band
};

// frequencies: the grid of the measurement; response: H(f) there (complex, with the plugin's delay)
FitResult fitEqModel(const std::vector<double>& frequencies, const std::vector<std::complex<double>>& response, const FitSettings& settings);

// A log grid of `perOctave` points per octave
std::vector<double> makeLogGrid(double lowestHz, double highestHz, double perOctave);
}
