#pragma once

namespace pluginlab::measure
{
// Noise weighting (docs/measurements/noise.md). The curves are the magnitudes of the analogue networks, applied to the bins of a spectrum (no
// bilinear warping near Nyquist).
enum class Weighting
{
    Unweighted,   // the standard low-pass only (AES17 5.2.5; at 44.1 and 48 kHz everything up to Nyquist, DC included)
    Band,         // 20 Hz ... 20 kHz (the AES17 passband, 3.4), DC excluded
    CcirRms,      // AES17 5.2.7: ITU-R BS.468-4 with -5.63 dB ("dB CCIR-RMS", unity gain at 2 kHz)
    A             // IEC 61672-1 A-weighting ("dB(A)", unity gain at 1 kHz)
};

// The gain (linear) of a weighting at a frequency
double getWeightingGain(Weighting weighting, double frequencyHz);

// ITU-R BS.468-4 weighting, 0 dB at 1 kHz, +12.2 dB at 6.3 kHz
double getBs468Gain(double frequencyHz);

// IEC 61672-1 annex E: A-weighting, 0 dB at 1 kHz
double getAWeightingGain(double frequencyHz);
}
