"""Checks the fast two-sums after the f64 log reductions are exact.

rc_log_f64 and rc_pow_f64 add r to the head of k ln2 - log(c), and rc_log2_f64 adds a head of
r / ln2 to k - log2(c) for x in [1/2, 2). Each sum's error is found exactly if its head is zero or
at least as big a power of 2 as anything it adds over the interval. For every interval of a table
and the exponents where the head is smallest (k = 0 and -1, and 1 for good measure) this checks
that; for every other k the head is at least ln2 / 2. Reads the tables from the C files that hold
them, rc_<name>_f64_inverse and then rc_<name>_f64_table, and prints any failure and the counts.

    python3 check_fast_sum.py LOG_TABLE LOG2_TABLE    (or: ./run.sh check)
"""

import math
import re
import sys
from fractions import Fraction as F

LN2_HI = float.fromhex("0x1.62e42fefa3800p-1")
INV_LN2 = F(1442695040888963407359924681001892137, 10 ** 36)


def read_table(path):
    """The j and the heads of the table in the C file at path."""
    txt = open(path).read()
    inverse = txt[txt.index("_inverse["):]
    js = [int(v) for v in re.findall(r"\b(\d+),", inverse.split("};")[0].split("{", 1)[1])]
    his = [float.fromhex(h) if h != "0.0" else 0.0 for h in re.findall(r"\{(\S+), \S+\},", inverse.split("};")[1])]
    assert len(js) == len(his) and len(js) in (128, 256)
    return js, his


def check(name, path, unit, scale):
    """Failures of one table: k unit - log(c) against r scale over each interval."""
    js, his = read_table(path)
    count = len(js)
    bad = 0
    for i in range(count):
        c = F(js[i], 2 * count)
        r_max = max(abs((1 + F(i, count)) * c - 1), abs((1 + F(i + 1, count)) * c - 1)) * scale
        for k in (0, -1, 1):
            head = k * unit + his[i]
            if head == 0:
                continue
            if math.frexp(abs(head))[1] < math.frexp(float(r_max))[1]:
                bad += 1
                print("fails", name, i, k, head, float(r_max))
    print("%s: %d intervals checked, failures: %d" % (name, count, bad))
    return bad


def main():
    bad = check("log", sys.argv[1], LN2_HI, 1) + check("log2", sys.argv[2], 1.0, INV_LN2)
    sys.exit(1 if bad else 0)


main()
