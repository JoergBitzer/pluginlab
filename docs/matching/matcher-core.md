# The matcher: the general layer

Unit of W9.1. Code: `src/match/` (library `pluginlab_match`), `Match.h`;
tests `tests/MatchTests.cpp`. Plan: `docs/design/W9-plan.md`, section 2.

## Purpose
The author: matching a compressor or a reverb works completely differently
from matching an EQ; what all have in common is that **parameters are
optimized against an objective**. This layer is that common part. It knows
nothing about EQs; a domain (the EQ of W9.2, later dynamics or reverb)
supplies the probe and the objective.

## The parts
| Part | What |
|---|---|
| `ParameterSpace` | the whole setting (normalised values); the parameters the user releases, each with its index, a range (all of 0 ... 1 or a part the user narrows it to) and a start; the others keep their values (only the user decides what is optimized) |
| `Probe` | a domain's measurement: a stimulus through a `measure::Device`, a feature vector out |
| `Objective` | the residual between the master's and the target's features (a vector, for least squares) and the tolerance; `WeightedDifference` is the usual one (weights per feature, tolerance on the largest unweighted difference) |
| `minimizeLevenbergMarquardt` | the optimizer (the author: LM is enough, CMA-ES later if needed) |
| `MatchProblem` | master features, a device per setting of the target, probe, objective, space; every setting measured once (cache); `solve` |
| `Budget` | evaluations and seconds (the author: 10 ... 15 s per target, no parallel renders); default 200 and 15 s |

## The optimizer
Levenberg-Marquardt on the residual r(x) of the released values x:
- numerical Jacobian: one evaluation per released parameter and iteration,
  a forward step of 0.001 of the parameter's range (backwards at the upper
  bound);
- the step from (J^T J + lambda diag(J^T J)) delta = -J^T r, projected onto the
  ranges; accepted if the sum of squares falls (lambda x 0.3), else lambda x 10
  and solved again;
- stops when the objective's tolerance is reached, when the relative
  improvement falls below 1e-6 (converged), when no step helps, or at the
  budget; the result is the best point with its residual, rms and largest
  value, the parameters at a bound (within 1 % of the range: the first cause
  for the attribution of W9.5), the evaluations (renders, the cache's answers
  not counted), the iterations, the seconds and the rms after each iteration.

## Results
From `MatchTests` (2026-10-13, 0.56.0): a linear least-squares problem is
solved exactly in a few iterations; the Rosenbrock valley (residual form,
mapped into the box) is found from (0.2, 0.9) to 1e-4 in 19 iterations and
64 evaluations; a minimum outside the box stops at the bound and is reported
there; the evaluation budget (10) and the time budget (0.3 s with slow
evaluations) stop the search; the parameter space keeps the locked values and
clamps to narrowed ranges; a match problem measures every setting once.
