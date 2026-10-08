# Oracle files (W6.5)

One JSON file per case, written by the Python prototype (`measurement_tool/tools/export_oracle.py`, run with its virtual environment:
`cd ~/AudioDev/measurement_tool && .venv/bin/python tools/export_oracle.py`). `tests/OracleTests.cpp` computes the same quantity with
`pluginlab_reference` / `pluginlab_signals` and must agree within the tolerance stored in the file. Do not edit the files by hand; to change a case,
change the script and run it again (the files record the script version, the date and the numpy/scipy versions).

| Case | The answer in Python | Tolerance |
|---|---|---|
| `rbj_peak_48k`, `rbj_low_shelf_44k`, `rbj_high_shelf_48k`, `rbj_low_pass_96k`, `rbj_notch_48k` | `scipy.signal.bilinear` of the analog RBJ prototype, pre-warped at f0 (not the cookbook formulas); the peak also with the prototype's own `rbj_peaking` | 1e-6 dB, 1e-5 degrees |
| `butterworth_lp5_96k` | `scipy.signal.butter` | 1e-6 dB, 1e-5 degrees |
| `thiran_37_25_48k` | Thiran coefficients (new numpy code), response by `scipy.signal.freqz`, group delay by `scipy.signal.group_delay` | 1e-6 dB, 1e-5 degrees, 1e-6 samples |
| `farina_rbj_peak_48k` | the prototype's `measure_frequency_response` (Farina sweep) of its RBJ peak: a measurement | 0.05 dB against the exact response |
| `polynomial_thd_48k` | the prototype's `measure_thdn` of y = x + 0.1 x^2 + 0.05 x^3 at -6 dBFS | 0.01 dB (harmonics, THD) |
| `quantizer16_snr_48k` | numpy rounding of a float32 sine, 16 bits | 0.01 dB |
| `gain_simple_997_48k` | the prototype's `measure_gain` (lock-in at 997 Hz, -20 dBFS) of a plain gain of -6 dB: the simplest case | 1e-4 dB, 1e-4 degrees |
| `gain_rbj_peak_997_48k` | the prototype's `measure_gain` of its RBJ peak (1 kHz, +6 dB, Q 2) at 997 Hz | 0.001 dB, 0.01 degrees |
| `sync_sweep_rbj_peak_48k` | the synchronized swept sine and its deconvolution (new numpy code), the first 8192 samples of the impulse response | 0.001 dB, 0.01 degrees |

A file of a kind the C++ test does not know fails the test: a new kind of case needs its comparison in `OracleTests.cpp`.
