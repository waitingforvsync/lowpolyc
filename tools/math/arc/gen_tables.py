"""The atan table and the constants of the arc functions and cbrt, from mpmath at 300 bits.

The table is the body of rc_arc_atan_table in src/math/atan_table.c: for the middle c of each
quarter of every binade from 1/16 to 16, atan(c) as the double nearest it and the double
nearest what that leaves. The constants are pi/2, pi, pi/4 and 3pi/4 as double-doubles and as the
nearest float, tan(pi/8) and tan(3pi/8), where the f32 functions change reduction, as doubles and
as the bits of the largest floats below them, and the terms of (1 + e)^(1/3) that cbrt_f64's
correction uses.

    python3 gen_tables.py [table|constants]    (or: ./run.sh tables, ./run.sh constants)

With no argument it prints both.
"""

import re
import struct
import sys

from mpmath import mp, mpf, atan, pi, tan

mp.prec = 300


def literal(x):
    """A C hex float literal without trailing zeros: 0x1.0800000000000p-4 -> 0x1.08p-4."""
    if x == 0:
        return "0.0"
    s = float(x).hex().replace("p+", "p")
    s = re.sub(r"\.?0+p", "p", s)
    return s


def split(v):
    """v as the nearest double and the nearest double to what that leaves."""
    hi = float(v)
    lo = float(v - mpf(hi))
    return hi, lo


def float_neighbour(f, step):
    """The float step units in the last place away from the float f."""
    bits = struct.unpack("I", struct.pack("f", f))[0]
    return struct.unpack("f", struct.pack("I", bits + step))[0]


def nearest_f32(v):
    """The float nearest v, checked against its neighbours (double then float can round twice)."""
    f = struct.unpack("f", struct.pack("f", float(v)))[0]
    below = float_neighbour(f, -1)
    above = float_neighbour(f, 1)
    assert abs(mpf(f) - v) < abs(mpf(below) - v)
    assert abs(mpf(f) - v) < abs(mpf(above) - v)
    return f


def print_table():
    rows = []
    for e in range(-4, 4):
        for k in range(4):
            c = mpf(2) ** e * (1 + (mpf(k) + mpf(1) / 2) / 4)
            hi, lo = split(atan(c))
            rows.append((literal(hi), literal(lo), literal(float(c))))
    width_hi = max(len(r[0]) for r in rows)
    width_lo = max(len(r[1]) for r in rows)
    for i, (hi, lo, c) in enumerate(rows):
        if i % 4 == 0:
            print("%s    // [2^%d, 2^%d)" % ("" if i == 0 else "\n", i // 4 - 4, i // 4 - 3))
        padding = " " * (width_hi + width_lo - len(hi) - len(lo))
        print("    {%s, %s},%s  // c = %s" % (hi, lo, padding, c))


def largest_f32_below(v):
    """The bits of the largest float below v, which must be below the double nearest v too."""
    bits = struct.unpack("I", struct.pack("f", float(v)))[0]
    if mpf(struct.unpack("f", struct.pack("I", bits))[0]) > v:
        bits -= 1
    above = struct.unpack("f", struct.pack("I", bits + 1))[0]
    assert mpf(above) > v and above > float(v)
    return bits


def print_constants():
    for name, v in [("pi/2", pi / 2), ("pi", pi), ("pi/4", pi / 4), ("3pi/4", 3 * pi / 4)]:
        hi, lo = split(v)
        f = nearest_f32(v)
        bits = struct.unpack("I", struct.pack("f", f))[0]
        print("%-6s hi %s  lo %s  nearest float %s (bits 0x%08x)" % (name, literal(hi), literal(lo), literal(f), bits))
    print("tan(pi/8) %s  tan(3pi/8) %s" % (literal(tan(pi / 8)), literal(tan(3 * pi / 8))))
    print("largest floats below tan(pi/8) 0x%08x  tan(3pi/8) 0x%08x" %
          (largest_f32_below(tan(pi / 8)), largest_f32_below(tan(3 * pi / 8))))
    print("1/3 %s  1/9 %s  5/81 %s  10/243 %s" %
          (literal(mpf(1) / 3), literal(mpf(1) / 9), literal(mpf(5) / 81), literal(mpf(10) / 243)))


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    if which in ("table", "both"):
        print_table()
    if which == "both":
        print()
    if which in ("constants", "both"):
        print_constants()


main()
