"""The kernel tables of src/math/trig.h, rebuilt from the fits and checked against the source.

Runs fit_trig.sollya for the sine and cosine polynomials and fit_tan.py for tan's rationals, lays
them out as trig.h does (each table indexed by the quadrant, or its parity, with the factors and
signs that pick sine or cosine, numerator or denominator), and prints the tables as C. Then it
checks every value of each table in trig.h, in order and down to the sign of each zero, and the
coefficients written into rc_trig_tan_quadrant (with twice D's z^2 one, in D's slope), and exits 1
if any differ.

    python3 tables.py TRIG_H    (or: ./run.sh tables)
"""

import re
import subprocess
import sys

import fit_tan

HEX = re.compile(r"-?0x[0-9a-fA-F.]+p[-+]?\d+")
NUMBER = re.compile(r"(?<![\w.])-?(?:0x[0-9a-fA-F.]+p[-+]?\d+|\d+\.\d+)(?![\w.])")


def sollya_fits():
    """fit_trig.sollya's polynomials as {name: {degree: coefficient}}."""
    out = subprocess.run(["sollya", "fit_trig.sollya"], capture_output=True, text=True, check=True).stdout
    fits = {}
    current = None
    for line in out.splitlines():
        heading = re.match(r"(\w+) relative error", line)
        if heading:
            current = fits.setdefault(heading.group(1), {})
            continue
        term = re.match(r"\s+x\^ (\S+) : (\S+)$", line)
        if current is not None and term and term.group(2).startswith(("0x", "-0x")):
            current[int(float.fromhex(term.group(1)))] = float.fromhex(term.group(2))
    return fits


def tables(poly, rational):
    """The tables of trig.h as {name: [values in source order]}."""
    sin32 = [poly["sin_f32"][d] for d in (3, 5, 7, 9)]
    cos32 = [poly["cos_f32"][d] for d in (2, 4, 6, 8)]
    sin64 = [poly["sin_f64"][d] for d in (5, 7, 9, 11, 13, 15)]
    cos64 = [poly["cos_f64"][d] for d in (4, 6, 8, 10, 12, 14)]
    sixth = poly["sin_f64"][3]
    n32, d32 = ([float(c) for c in side[1:]] for side in rational["tan_f32"])
    n64, d64 = ([float(c) for c in side[1:]] for side in rational["tan_f64"])

    f32 = [1.0, 0.0, -1.0, 0.0, -0.0, 1.0, -0.0, -1.0]
    for s, c in zip(sin32, cos32):
        f32 += [s, c, s, c]
    tan32 = [1.0, 0.0, 0.0, 1.0, -0.0, -1.0, 1.0, -0.0]
    for n, d in zip(n32, d32):
        tan32 += [n, d, d, n]
    f64 = [1.0, 0.0, -1.0, 0.0, -0.0, 1.0, -0.0, -1.0, sixth, 0.0, -sixth, 0.0, -0.0, -0.5, -0.0, 0.5]
    for s, c in zip(sin64, cos64):
        f64 += [s, c]
    return {
        "rc_trig_sin_f32": f32,
        "rc_trig_tan_f32": tan32,
        "rc_trig_sin": f64,
        "rc_trig_tan_quadrant": n64 + d64[1:] + [2.0 * d64[1]],
    }


def source_values(source, name):
    """The values of the table called name in trig.h, or the hex constants in the function of that
    name, in order."""
    table = re.search(r"\b" + name + r" = \{\n(.*?)\n\};", source, re.S)
    if table:
        return [float.fromhex(v) if "x" in v else float(v) for v in NUMBER.findall(table.group(1))]
    body = re.search(r"\b" + name + r"\([^)]*\)\n\{\n(.*?)\n\}", source, re.S)
    if not body:
        sys.exit(f"tables.py: no {name} in trig.h")
    return [float.fromhex(v) for v in HEX.findall(body.group(1))]


def show(name, values):
    """Prints a table's values, four to a line."""
    print(f"{name}:")
    for i in range(0, len(values), 4):
        print("    " + ", ".join(v.hex() for v in values[i:i + 4]))


def main():
    source = open(sys.argv[1]).read()
    bad = 0
    for name, values in tables(sollya_fits(), fit_tan.fits()).items():
        show(name, values)
        found = source_values(source, name)
        if [v.hex() for v in found] == [v.hex() for v in values]:
            print(f"{name}: {len(values)} values match {sys.argv[1]}")
        else:
            print(f"{name}: DIFFERS from {sys.argv[1]}")
            bad += 1
    sys.exit(1 if bad else 0)


main()
