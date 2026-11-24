"""The tables and constants of the f64 exp, log, log2 and pow, from mpmath at 300 bits.

The log table chooses, for each of 256 intervals of the significand m in [1, 2), the c = j / 512
that keeps r = m c - 1 smallest across it (exactly, with rationals), and holds -log(c) as a
multiple of 2^-42 and the nearest double to what is left. The log2 table has the same intervals
and the same c, with -log2(c). The exp table holds 2^(j/128) as hi (1 + lo), hi the nearest double
and lo the nearest double to the relative rest. Prints the ranges of r, the tables and the
constants (ln 2, ln 2 / 128 and 1 / ln 2 split for exact products, 1 / ln 2 scaled by 2^-61 for
log2, 128 / ln 2, and the overflow and underflow thresholds), and writes the tables laid out as C
to OUTDIR/log_table.inc, OUTDIR/log2_table.inc and OUTDIR/exp_table.inc, as they appear in
log_f64_table.c, log2_f64_table.c and exp_f64_kernel.c.

    python3 gen_tables.py OUTDIR    (or: ./run.sh tables, which also compares them with the tree)
"""

import os
import sys
from fractions import Fraction as F

import mpmath as mp

mp.mp.prec = 300

STEP = F(1, 2 ** 42)


def exact(v):
    """An mpf as an exact Fraction."""
    v = mp.mpf(v)
    man, exp = v.man_exp
    q = F(man) * (F(2) ** exp) if exp >= 0 else F(man, 2 ** -exp)
    return -q if v < 0 else q


def to_double(q):
    """The double nearest the Fraction q (Python's float(Fraction) rounds correctly)."""
    return float(q)


def round_to(q, step):
    """q rounded to the nearest multiple of step."""
    n = q / step
    return F(round(n)) * step


def hexd(d):
    return "0.0" if d == 0 else float.hex(d)


def choose_inverses(count):
    """Each of count intervals' j, c being j / (2 count), and the least and greatest r over all."""
    js = []
    r_lo, r_hi = F(0), F(0)
    for i in range(count):
        a = 1 + F(i, count)
        b = 1 + F(i + 1, count)
        cands = [2 * count] if i == 0 else [count] if i == count - 1 else range(count, 2 * count + 1)
        best = None
        for j in cands:
            c = F(j, 2 * count)
            worst = max(abs(a * c - 1), abs(b * c - 1))
            if best is None or worst < best[0]:
                best = (worst, j, a * c - 1, b * c - 1)
        js.append(best[1])
        r_lo = min(r_lo, best[2])
        r_hi = max(r_hi, best[3])
    return js, r_lo, r_hi


def write_log_table(name, js, log_of, path):
    """Writes the inverse table and -log_of(c) for each, as name's arrays, to path, and prints them."""
    count = len(js)
    print("%s inverse table (j_i, c = j / %d):" % (name, 2 * count))
    for row in range(0, count, 16):
        print("    " + ", ".join("%d" % j for j in js[row:row + 16]) + ",")

    print("%s table (-%s(c) hi, lo):" % (name, name))
    with open(path, "w") as inc:
        inc.write("const uint16_t rc_%s_f64_inverse[%d] = {\n" % (name, count))
        for row in range(0, count, 16):
            inc.write("    " + ", ".join("%d" % j for j in js[row:row + 16]) + ",\n")
        inc.write("};\n\n\nconst rc_exp_log_f64_pair rc_%s_f64_table[%d] = {\n" % (name, count))
        for j in js:
            t = exact(-log_of(mp.mpf(j) / (2 * count)))
            hi = round_to(t, STEP)
            lo = to_double(t - hi)
            assert float(hi) == hi
            pair = "{%s, %s}," % (hexd(float(hi)), hexd(lo))
            print("    " + pair)
            inc.write("    %-48s // %d / %d\n" % (pair, j, 2 * count))
        inc.write("};\n")


def write_exp_table(path):
    print("exp table (2^(j/128) = hi (1 + lo)):")
    with open(path, "w") as inc:
        inc.write("const rc_exp_log_f64_pair rc_exp_f64_table[128] = {\n")
        for j in range(128):
            t = exact(mp.power(2, mp.mpf(j) / 128))
            hi = to_double(t)
            lo = to_double((t - F(hi)) / F(hi))
            print("    {%s, %s}," % (hexd(hi), hexd(lo)))
            inc.write("    {%s, %s},\n" % (hexd(hi), hexd(lo)))
        inc.write("};\n")


def print_constants(ln2):
    ln2_128 = ln2 / 128
    c1 = round_to(ln2_128, STEP)
    c2 = to_double(ln2_128 - c1)
    print("ln2/128 hi = %s  lo = %s" % (hexd(float(c1)), hexd(c2)))
    print("128/ln2 = %s" % hexd(to_double(1 / ln2_128)))
    inv_ln2 = 1 / ln2
    ih = round_to(inv_ln2, F(1, 2 ** 26))
    il = to_double(inv_ln2 - ih)
    print("1/ln2 hi (27 bits) = %s  lo = %s" % (hexd(float(ih)), hexd(il)))
    scale = F(1, 2 ** 61)
    print("2^-61/ln2 = %s  hi = %s  lo = %s (log2's, for 2^61 r)" % (
        hexd(to_double(inv_ln2 * scale)), hexd(float(ih * scale)), hexd(to_double((inv_ln2 - ih) * scale))))
    print("ln(2^1024 (1 - 2^-54)) = %r" % float(exact(mp.log(mp.mpf(2) ** 1024 * (1 - mp.mpf(2) ** -54)))))
    print("ln(2^-1075) = %r" % float(exact(mp.log(mp.mpf(2) ** -1075))))


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)

    ln2 = exact(mp.log(2))
    ln2_hi = round_to(ln2, STEP)
    ln2_lo = to_double(ln2 - ln2_hi)
    print("ln2_hi = %s  ln2_lo = %s" % (hexd(float(ln2_hi)), hexd(ln2_lo)))

    js, r_lo, r_hi = choose_inverses(256)
    print("// log r range: [%r, %r)" % (float(r_lo), float(r_hi)))
    write_log_table("log", js, mp.log, os.path.join(outdir, "log_table.inc"))

    write_log_table("log2", js, lambda v: mp.log(v) / mp.log(2), os.path.join(outdir, "log2_table.inc"))

    write_exp_table(os.path.join(outdir, "exp_table.inc"))
    print_constants(ln2)


main()
