"""The rational kernel of asin_f64 and acos_f64 (rc_arc_asin_ratio_f64 in src/math/arc.h).

asin(a) = a + a z N(z) / D(z) for z = a^2 in (0, 1/4], with N a quintic and D a quartic whose
constant term is 1, minimising the relative error of asin. fpminimax can't fit a rational
function, so we do it here: Lawson's iteration, which reweights a linearised least-squares fit
(N(z) - R(z) D(z), divided by the last D(z)) toward the minimax one, over 600 Chebyshev points,
with mpmath at 160 bits. The coefficients are then rounded to the nearest doubles. Prints them in
hex and the relative error, before rounding and after.

    python3 fit_asin_ratio.py    (or: ./run.sh fits)
"""

from mpmath import mp, mpf, asin, sqrt, matrix, lu_solve, log, cos, pi

mp.prec = 160

N_DEGREE = 5
D_DEGREE = 4
POINTS = 600
ITERATIONS = 60

TOP = mpf(1) / 4
ZS = [TOP * (1 - cos(pi * (k + mpf(1) / 2) / POINTS)) / 2 for k in range(POINTS)]
# R(z) = asin(a) / (a z) - 1 / z, the function z N / D stands for, divided by z
RS = [(asin(sqrt(z)) / sqrt(z) - 1) / z for z in ZS]
# The relative error of asin per unit error in R
WEIGHTS = [z / (1 + z * r) for z, r in zip(ZS, RS)]


def evaluate(coefficients, z):
    total = mpf(0)
    for c in reversed(coefficients):
        total = total * z + c
    return total


def fit(lawson, last_d):
    """The weighted least-squares N and D, linearised about the last D."""
    count = N_DEGREE + 1 + D_DEGREE
    a = matrix(count, count)
    b = matrix(count, 1)
    for z, r, w, l, d in zip(ZS, RS, WEIGHTS, lawson, last_d):
        s = w / d
        row = [z ** i * s for i in range(N_DEGREE + 1)] + [-r * z ** j * s for j in range(1, D_DEGREE + 1)]
        for i in range(count):
            b[i] += l * row[i] * r * s
            for j in range(count):
                a[i, j] += l * row[i] * row[j]
    x = lu_solve(a, b)
    return [x[i] for i in range(N_DEGREE + 1)], [mpf(1)] + [x[N_DEGREE + 1 + j] for j in range(D_DEGREE)]


def worst(n, d):
    return max(abs(evaluate(n, z) / evaluate(d, z) - r) * w for z, r, w in zip(ZS, RS, WEIGHTS))


def main():
    lawson = [mpf(1)] * POINTS
    last_d = [mpf(1)] * POINTS
    best = None
    for _ in range(ITERATIONS):
        n, d = fit(lawson, last_d)
        errors = [abs(evaluate(n, z) / evaluate(d, z) - r) * w for z, r, w in zip(ZS, RS, WEIGHTS)]
        error = max(errors)
        if best is None or error < best[0]:
            best = (error, n, d)
        total = sum(l * e for l, e in zip(lawson, errors))
        lawson = [l * e / total for l, e in zip(lawson, errors)]
        last_d = [evaluate(d, z) for z in ZS]
    error, n, d = best
    n_rounded = [mpf(float(c)) for c in n]
    d_rounded = [mpf(float(c)) for c in d]
    print("---- asin_f64 ratio, z = a^2 <= 1/4")
    for i, c in enumerate(n_rounded):
        print("  N z^%d: %s" % (i, float(c).hex()))
    for i, c in enumerate(d_rounded):
        print("  D z^%d: %s" % (i, float(c).hex()))
    print("  relative error 2^%.2f, 2^%.2f with the coefficients rounded" %
          (float(log(error, 2)), float(log(worst(n_rounded, d_rounded), 2))))


main()
