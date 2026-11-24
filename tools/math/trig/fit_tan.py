"""The rational kernels of tan (src/math/trig.h): Remez in mpmath, then fpminimax in Sollya.

tan r = r N(z) / D(z) with z = r^2 on [0, 0.7854], just past pi/4, minimising the relative error.
Sollya fits polynomials only, so the rational comes from our own Remez exchange: at m + n + 1
reference points the error N / (D g) - 1, g(z) = tan(r) / r, alternates in sign with equal size,
which is a linear system once the D multiplying the error is taken from the step before; the
points then move to the extremes of the error, until they settle.

  f32: N = 1 + a1 z + a2 z^2, D = 1 + b1 z + b2 z^2, all free, rounded to doubles.
  f64: N = 1 + A z + a2 z^2 + a3 z^3 + a4 z^4 and D = 1 - z/2 + b2 z^2 + b3 z^3 + b4 z^4. D's -1/2
       keeps 1 - z/2 exact and A, the double nearest -1/6, keeps A r^3 to one rounding (see
       trig.h). D's free coefficients from the Remez are rounded to doubles, and then N's are
       fitted again by fpminimax against tan(r) D(z) / r, which wins back what rounding cost.

Prints each rational's coefficients in hex and its relative error over 20000 points, as doubles.

    python3 fit_tan.py    (or: ./run.sh fits, after fit_trig.sollya; tables.py imports it)
"""

import re
import subprocess

from mpmath import mp, mpf, tan, sqrt, matrix, lu_solve, log

mp.prec = 200

R = mpf("0.7854")
Z = R * R


def g(z):
    """tan(r) / r for z = r^2."""
    if z == 0:
        return mpf(1)
    r = sqrt(z)
    return tan(r) / r


def poly(c, z):
    """c[0] + c[1] z + c[2] z^2 + ..."""
    v = mpf(0)
    for a in reversed(c):
        v = v * z + a
    return v


def error(a, b, z):
    """The relative error of N / D against g at z."""
    return poly(a, z) / (poly(b, z) * g(z)) - 1


def extremes(a, b, count):
    """The count largest alternating extremes of the error on a fine grid, as their z."""
    grid = [Z * mpf(i) / 3000 for i in range(1, 3001)]
    errs = [error(a, b, z) for z in grid]
    peaks = []
    for i, e in enumerate(errs):
        left = errs[i - 1] if i > 0 else None
        right = errs[i + 1] if i + 1 < len(errs) else None
        if (left is None or abs(e) >= abs(left)) and (right is None or abs(e) >= abs(right)):
            peaks.append((grid[i], e))
    alternating = []
    for z, e in peaks:
        if alternating and (alternating[-1][1] > 0) == (e > 0):
            if abs(e) > abs(alternating[-1][1]):
                alternating[-1] = (z, e)
        else:
            alternating.append((z, e))
    while len(alternating) > count:
        alternating.pop(0 if abs(alternating[0][1]) < abs(alternating[-1][1]) else -1)
    return [z for z, e in alternating]


def remez(a_fixed, b_fixed, m, n):
    """N of degree m and D of degree n in z, whose leading coefficients a_fixed and b_fixed (1 and
    any fixed after it) are given; returns the coefficients of N and D."""
    free_a = m + 1 - len(a_fixed)
    free_b = n + 1 - len(b_fixed)
    count = free_a + free_b + 1
    points = [Z * mpf(i + 1) / count for i in range(count)]
    a = a_fixed + [mpf(0)] * free_a
    b = b_fixed + [mpf(0)] * free_b
    for _ in range(30):
        for _ in range(20):
            system = matrix(count, count)
            rhs = matrix(count, 1)
            for i, z in enumerate(points):
                gz = g(z)
                for j in range(free_a):
                    system[i, j] = z ** (len(a_fixed) + j)
                for j in range(free_b):
                    system[i, free_a + j] = -gz * z ** (len(b_fixed) + j)
                system[i, count - 1] = -(-1) ** i * gz * poly(b, z)
                rhs[i] = gz * poly(b_fixed, z) - poly(a_fixed, z)
            solution = lu_solve(system, rhs)
            a = a_fixed + [solution[j] for j in range(free_a)]
            b = b_fixed + [solution[free_a + j] for j in range(free_b)]
        moved = extremes(a, b, count)
        if len(moved) == count:
            points = moved
    return a, b


def to_double(c):
    """Each coefficient rounded to the nearest double."""
    return [mpf(float(x)) for x in c]


def refit_numerator(a_fixed, b, m):
    """N's free coefficients fitted again by Sollya's fpminimax, relative to tan(r) D(z) / r, for
    the given D in doubles; returns N."""
    d = " + ".join(f"{float(c).hex()} * x^{2 * i}" for i, c in enumerate(b))
    head = " + ".join(f"{float(c).hex()} * x^{2 * i + 1}" for i, c in enumerate(a_fixed))
    degrees = ", ".join(str(2 * i + 1) for i in range(len(a_fixed), m + 1))
    script = f"""prec = 300; display = hexadecimal;
I = [2^-40; 0.7854]; d = {d};
p = fpminimax(tan(x) * d, [|{degrees}|], [|D...|], I, relative, floating, {head});
for i from {2 * len(a_fixed) + 1} to {2 * m + 1} by 2 do print(coeff(p, i));
quit;
"""
    out = subprocess.run(["sollya"], input=script, capture_output=True, text=True, check=True).stdout
    found = re.findall(r"-?0x[0-9a-f.]+p[-+]?\d+", out)
    return a_fixed + [mpf(float.fromhex(v)) for v in found]


def show(name, a, b):
    """Prints a rational's coefficients and its relative error as doubles."""
    worst = max(abs(error(a, b, Z * mpf(i) / 20000)) for i in range(1, 20001))
    print(f"{name}: relative error 2^{float(log(worst, 2)):.2f}")
    for label, c in (("N", a), ("D", b)):
        for i, v in enumerate(c):
            print(f"  {label} z^{i}: {float(v).hex():24s} {float(v)!r}")


def fits():
    """Both rationals as {name: (N, D)}, coefficients as doubles, lowest power first."""
    a, b = remez([mpf(1)], [mpf(1)], 2, 2)
    f32 = (to_double(a), to_double(b))

    sixth = mpf(float(-mpf(1) / 6))
    a, b = remez([mpf(1), sixth], [mpf(1), mpf(-0.5)], 4, 4)
    b = to_double(b)
    return {"tan_f32": f32, "tan_f64": (refit_numerator([mpf(1), sixth], b, 4), b)}


def main():
    for name, (a, b) in fits().items():
        show(name, a, b)


if __name__ == "__main__":
    main()
