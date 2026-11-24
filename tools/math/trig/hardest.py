"""The floating-point numbers closest to a multiple of pi/2, binade by binade.

For x = m 2^e (m a p-bit integer), |x - k pi/2| = 2^e |m - k alpha| with alpha = (pi/2) / 2^e.
The smallest ||k alpha|| for k up to K is at the last continued-fraction denominator q <= K, so
that gives a rigorous lower bound for the binade; candidates built from the convergents near the
binade's k range give actual hard arguments. These bound how much a reduction can cancel, and
the hardest of them are among the golden inputs of test_math_trig.c.

Prints the 25 closest approaches, the lower bound over all binades and over |x| below a few
thresholds, and writes every binade's closest approach (e, log2|r|, m, k per line) to
OUTDIR/binades{p}.txt for checking a reduction's error against.

    python3 hardest.py 24 OUTDIR    (floats)
    python3 hardest.py 53 OUTDIR    (doubles)
    (or: ./run.sh hardest, for both)
"""

import os
import sys

from mpmath import mp, mpf, pi, floor, nint, log

mp.prec = 3000


def convergents(a, count):
    """The first count continued-fraction convergents of a, as (h, k, partial quotient)."""
    h0, h1, k0, k1 = 0, 1, 1, 0
    x = a
    out = []
    for _ in range(count):
        ai = int(floor(x))
        h0, h1 = h1, ai * h1 + h0
        k0, k1 = k1, ai * k1 + k0
        out.append((h1, k1, ai))
        frac = x - ai
        if frac == 0:
            break
        x = 1 / frac
    return out


def candidates(alpha, k1, k2):
    """The multipliers k in [k1, k2] worth trying: all of them if few, else ones near convergents."""
    if k2 - k1 < 3000:
        return range(k1, k2 + 1)

    cs = convergents(alpha, 400)
    cands = set()
    for i in range(1, len(cs)):
        q, qprev = cs[i][1], cs[i - 1][1]
        if q > 4 * k2:
            break
        for a in range(0, 2 * (cs[i + 1][2] if i + 1 < len(cs) else 1) + 3):
            for b in (-2, -1, 0, 1, 2):
                for c in range(1, 4):
                    k = c * (a * q + qprev) + b * q
                    if k1 <= k <= k2:
                        cands.add(k)
            if a > 50:
                break
        for c in range(max(1, k1 // q), k2 // q + 2):
            if c - k1 // q > 64:
                break
            k = c * q
            if k1 <= k <= k2:
                cands.add(k)
    return cands


def binade(p, e, halfpi):
    """The closest approach in binade e as (log2|r|, e, m, k, log2 lower bound), or None."""
    lo_m, hi_m = 2 ** (p - 1), 2 ** p - 1
    alpha = halfpi / mpf(2) ** e
    k1 = int(floor(lo_m / alpha))
    k2 = int(floor(hi_m / alpha)) + 1
    if k2 < 1:
        return None

    k1 = max(k1, 1)
    best = None
    for k in candidates(alpha, k1, k2):
        m = int(nint(k * alpha))
        if m < lo_m or m > hi_m:
            continue
        d = abs(m - k * alpha)
        if best is None or d < best[0]:
            best = (d, m, k)

    # Rigorous bound: the last convergent denominator <= k2
    cs = convergents(alpha, 400) if k2 >= 2 else []
    bound = None
    for h, q, _ in cs:
        if q > k2:
            break
        if q >= 1:
            bound = abs(q * alpha - h)
    if best is None:
        return None

    d, m, k = best
    r = d * mpf(2) ** e
    lb = bound * mpf(2) ** e if bound is not None else None
    return (float(log(r, 2)), e, m, k, float(log(lb, 2)) if lb else None)


def main():
    p = int(sys.argv[1])
    outdir = sys.argv[2]
    emin, emax = (-149, 104) if p == 24 else (-1074, 971)
    halfpi = pi / 2

    results = []
    for e in range(emin, emax + 1):
        found = binade(p, e, halfpi)
        if found is not None:
            results.append(found)

    results.sort()
    for lr, e, m, k, lb in results[:25]:
        print(f"log2|r| {lr:8.3f}  bound {lb:8.3f}  x = {m} * 2^{e}  k = {k}")
    print("min lower bound over binades:", min(r[4] for r in results if r[4] is not None))
    for thr in (13, 18, 20, 23, 28, 32):
        sub = [r for r in results if r[1] + p <= thr]
        if sub:
            print(f"|x| < 2^{thr}: min log2|r| {min(s[0] for s in sub):.3f} "
                  f"bound {min(s[4] for s in sub if s[4] is not None):.3f}")

    # Every binade's closest approach, for checking a reduction's error against it
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, f"binades{p}.txt"), "w") as out:
        for lr, e, m, k, lb in sorted(results, key=lambda r: r[1]):
            out.write(f"{e} {lr:.3f} {m} {k}\n")


main()
