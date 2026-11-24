"""The constants of the trig argument reduction, from mpmath at 2000 bits.

Prints the 32-bit limbs of 2/pi that trig_reduce.c's large reduction of doubles walks, the
overlapping 64-bit windows of 2/pi the floats' large reduction in trig.h takes (defined in
trig_reduce.c), the pieces of pi/2 the short reductions in trig.h subtract one after another (each
but the last a whole multiple of a power of 2, so k times it is exact), pi/2 as a double-double,
and the double below pi/4, where the doubles' reduction starts.

    python3 trig_constants.py    (or: ./run.sh constants)
"""

import struct

from mpmath import mp, mpf, pi, floor, nint

mp.prec = 2000

LIMBS = 37

# The floats' windows: 64 bits each, 4 bits apart, the first starting 8 bits before 2/pi's first
WINDOWS = 44


def rd(v):
    """v rounded to the nearest double."""
    return float(v)


def piece(v, lsb):
    """v rounded to a whole multiple of 2^lsb."""
    return nint(v / mpf(2) ** lsb) * mpf(2) ** lsb


def show(name, v):
    """Prints v, which must be a double, in hex and decimal."""
    f = float(v)
    assert mpf(f) == v, name
    print(f"{name:24s} {f.hex():24s} {f!r}")


def show_limbs():
    """2/pi as 32-bit limbs, most significant first: 2/pi = sum B[i] 2^(-32 (i + 1))."""
    bits = int(floor(2 / pi * mpf(2) ** (32 * LIMBS)))
    limbs = [(bits >> (32 * (LIMBS - 1 - i))) & 0xffffffff for i in range(LIMBS)]
    print("2/pi limbs:")
    for i in range(0, LIMBS, 6):
        print("    " + ", ".join(f"0x{v:08x}u" for v in limbs[i:i + 6]) + ",")


def show_windows():
    """2/pi in 64-bit windows 4 bits apart: window i is floor(2/pi 2^(4 i + 56)) mod 2^64."""
    windows = [int(floor(2 / pi * mpf(2) ** (4 * i + 56))) % 2 ** 64 for i in range(WINDOWS)]
    print("2/pi windows for floats:")
    for i in range(0, WINDOWS, 4):
        print("    " + ", ".join(f"0x{v:016x}ull" for v in windows[i:i + 4]) + ",")


def show_pieces(half_pi):
    """The pieces of pi/2 for the short reductions, and what each leaves out."""

    # f64, short: pi/2 = P1 + P2 + P3 + P4, P1 33 bits, P2 down to 2^-53, P3 down to 2^-86
    p1 = piece(half_pi, -32)
    p2 = piece(half_pi - p1, -53)
    p3 = piece(half_pi - p1 - p2, -86)
    p4 = mpf(rd(half_pi - p1 - p2 - p3))
    for name, v in (("p1", p1), ("p2", p2), ("p3", p3), ("p4", p4)):
        show("f64 " + name, v)
    print("  f64 rest log2", float(mp.log(abs(half_pi - p1 - p2 - p3 - p4), 2)))

    # f32, short: pi/2 = S1 + S2, S1 33 bits (P1 above) and S2 the double nearest the rest
    s1 = piece(half_pi, -32)
    s2 = mpf(rd(half_pi - s1))
    for name, v in (("short s1", s1), ("short s2", s2)):
        show("f32 " + name, v)
    print("  f32 short rest log2", float(mp.log(abs(half_pi - s1 - s2), 2)))


def main():
    half_pi = pi / 2

    show_limbs()
    print()
    show_windows()
    print()
    show("two_over_pi", mpf(rd(2 / pi)))
    show_pieces(half_pi)

    # pi/2 as a double-double, and the double below pi/4 with its bits
    hi = mpf(rd(half_pi))
    show("half_pi hi", hi)
    show("half_pi lo", mpf(rd(half_pi - hi)))
    quarter = mpf(rd(pi / 4))
    assert quarter < pi / 4
    show("quarter_pi below", quarter)
    print(f"  bits 0x{struct.unpack('<Q', struct.pack('<d', float(quarter)))[0]:016x}")


main()
