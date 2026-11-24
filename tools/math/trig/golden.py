"""The golden rows of test_math_trig.c, each result checked against mpmath.

Runs dump (dump.c, built against the library) on the golden inputs, works out each result's
error in ulps against mpmath's sin, cos and tan at 3000 bits, and prints the rows for the tests
with those errors as comments, then the worst error of each function as a final comment.

    python3 golden.py DUMP    (or: ./run.sh golden, which builds dump and checks the rows)
"""

import struct
import subprocess
import sys

from mpmath import mp, mpf, sin, cos, tan, floor, log

mp.prec = 3000


def f64(b):
    return struct.unpack("<d", struct.pack("<Q", b))[0]


def f32(b):
    return struct.unpack("<f", struct.pack("<I", b))[0]


def b64(x):
    return struct.unpack("<Q", struct.pack("<d", x))[0]


def b32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def ulps(ours, true, p, emin):
    """|ours - true| in ulps of true, for precision p and least normal exponent emin."""
    if ours != ours or abs(ours) == float("inf"):
        return None
    t = abs(true)
    if t == 0:
        return float(abs(mpf(ours)) / mpf(2) ** (emin - p + 1))
    e = int(floor(log(t, 2)))
    e = max(e, emin)
    return float(abs(mpf(ours) - true) / mpf(2) ** (e - p + 1))


def inputs64():
    """The f64 golden inputs: every path of the reduction, and the hardest arguments to reduce."""
    xs = [5e-324, 1e-300, 2.0 ** -30, 1e-8, 0.1, 0.5, 0.7853981633974482, 0.7853981633974483, 1.0,
          1.5707963267948966, 2.0, 3.141592653589793, 4.71238898038469, 6.283185307179586, 10.0,
          100.0, 6411027962775774 * 2.0 ** -47, 6411027962775774 * 2.0 ** -46, 1e5, 1048575.75,
          1048576.0, 7763785107565477 * 2.0 ** -29, 1e22, 6617649673795284 * 2.0 ** 15,
          5916243447979695 * 2.0 ** 79, 1e300, 6381956970095103 * 2.0 ** 797,
          8444920710073313 * 2.0 ** 939, 1.7976931348623157e308]
    return [b64(x) for x in xs]


def inputs32():
    """The f32 golden inputs, chosen the same way."""
    xs = [1.4e-45, 1e-30, 2.0 ** -20, 0.1, 0.5, f32(0x3f490fda), f32(0x3f490fdb), 1.0,
          f32(0x3fc90fdb), 2.0, f32(0x40490fdb), f32(0x4096cbe4), 10.0, 100.0, 16573937 * 2.0 ** -16,
          1e5, f32(0x497fffff), 1048576.0, 10695401 * 2.0 ** 2, f32(0x4cffffff), 134217728.0, 1e9, 10741887 * 2.0 ** 11,
          9012376 * 2.0 ** 53, 10057457 * 2.0 ** 62, 16367173 * 2.0 ** 72, 14173733 * 2.0 ** 88,
          3.4028234663852886e38]
    return [b32(x) for x in xs]


def run(dump, kind, bits):
    """dump's rows for the inputs bits: the input and its sin, cos and tan, as bits."""
    out = subprocess.run([dump, kind], input="\n".join(f"{b:x}" for b in bits), capture_output=True,
                         text=True, check=True).stdout
    return [[int(v, 16) for v in line.split()] for line in out.split("\n") if line]


def main():
    dump = sys.argv[1]
    worst = {}
    widths = (("f64", inputs64(), f64, 53, -1022, 16), ("f32", inputs32(), f32, 24, -126, 8))
    for kind, bits, conv, p, emin, width in widths:
        for row in run(dump, kind, bits):
            x = mpf(conv(row[0]))
            cells = []
            for name, fn, rb in (("sin", sin, row[1]), ("cos", cos, row[2]), ("tan", tan, row[3])):
                u = ulps(conv(rb), fn(x), p, emin)
                key = name + "_" + kind
                worst[key] = max(worst.get(key, 0), u)
                cells.append(f"{u:.3f}")
            values = ", ".join(f"0x{v:0{width}x}" for v in row)
            print(f"    {{{values}}},  // {conv(row[0])!r} ulps {' '.join(cells)}")
    print("// worst:", worst)


main()
