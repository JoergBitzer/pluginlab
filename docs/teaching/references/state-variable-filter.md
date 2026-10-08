# The state-variable filter (TPT / zero-delay feedback)

V. Zavalishin, "The Art of VA Filter Design" (topology-preserving transform); the form of A. Simper (Cytomic), "Linear Trapezoidal Integrated SVF".
Code: `StateVariableFilter` in `src/reference/StateVariableFilter.cpp`; in the Reference EQ: algorithm "State variable (TPT)".

## The structure
Two trapezoidal integrators in a loop. With $g = \tan(\pi f_c/f_s)$, $k = 1/Q$ and
$a_1 = 1/(1 + g(g + k))$, $a_2 = g a_1$, $a_3 = g a_2$, per sample:

$$v_3 = v_0 - ic_2,\quad v_1 = a_1\, ic_1 + a_2 v_3,\quad v_2 = ic_2 + a_2\, ic_1 + a_3 v_3,\quad ic_1 \leftarrow 2v_1 - ic_1,\quad ic_2 \leftarrow 2v_2 - ic_2$$

$v_2$ is the low-pass, $v_1$ the band-pass, and every type is a mix $m_0 v_0 + m_1 v_1 + m_2 v_2$:

| Type | $m_0$ | $m_1$ | $m_2$ | remarks |
|---|---|---|---|---|
| low-pass | 0 | 0 | 1 | |
| high-pass | 1 | $-k$ | $-1$ | |
| band-pass | 0 | 1 | 0 | peak gain $Q$ |
| band-pass 0 dB | 0 | $k$ | 0 | |
| notch | 1 | $-k$ | 0 | |
| all-pass | 1 | $-2k$ | 0 | |
| peak | 1 | $k(A^2 - 1)$ | 0 | $k = 1/(QA)$ |
| low shelf | 1 | $k(A - 1)$ | $A^2 - 1$ | $g \to g/\sqrt{A}$ |
| high shelf | $A^2$ | $k(1 - A)A$ | $1 - A^2$ | $g \to g\sqrt{A}$ |

## The known answer
For fixed parameters the structure has **exactly** the transfer function of the RBJ design of the same type (both are the bilinear transform of the same
analog prototype). The tests run an impulse through the structure and compare with the RBJ formula: below 1e-9 dB for all nine types at three sample
rates. So the curves of the [RBJ page](rbj-cookbook.md) are also the curves of this filter.

What is different: when the parameters change while the filter runs. The state of a direct-form biquad has no physical meaning, and a fast change of
its coefficients can make it ring or even blow up; the integrator states of the SVF are the "capacitor voltages" of the analog circuit and stay
meaningful. The test modulates the cutoff of a low-pass with $Q = 10$ between 100 Hz and 10 kHz at 200 Hz, every sample: the output stays bounded.
That is why many modern plugins (filters with modulation, synthesizers) use it.
