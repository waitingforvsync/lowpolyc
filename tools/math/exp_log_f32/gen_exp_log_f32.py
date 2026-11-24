"""The tables, constants and polynomials of the f32 exp, log, log2 and pow.

The tables and the polynomials' coefficients go in src/math/exp_f32_table.c and log_f32_table.c,
and the constants in exp_f32.c and log_f32.c. The tables come from mpmath at 200 bits, rounded
once to double. The polynomials are fitted by Sollya's fpminimax (run as a subprocess) with double
coefficients, and their errors re-measured with those rounded coefficients (dirtyinfnorm). Prints
C to paste in: 32 log2(e) and ln 2, the 2^(i/32) table, exp's cubic, the log table (each
interval's scale and its log2), and the polynomials of log, and of log2 (which pow shares).

    python3 gen_exp_log_f32.py    (or: ./run.sh tables)
"""

import struct
import subprocess

import mpmath

mpmath.mp.prec = 200
EXP2_BITS = 5
EXP2_DEGREE = 3
LOG_BITS = 7
LOG_OFFSET = 0x3f348000  # 1.0 sits in the middle (in bits) of interval 75
LOG_ONE_INDEX = 75
LOG_DEGREE = 4
LOG2_DEGREE = 4


def f32_from_bits(b):
    return struct.unpack('<f', struct.pack('<I', b))[0]


def f64_bits(x):
    return struct.unpack('<Q', struct.pack('<d', float(x)))[0]


def round_f32(x):
    return struct.unpack('<f', struct.pack('<f', float(x)))[0]


def hexd(x):
    return float(x).hex()


def sollya(script):
    """Sollya's output for script."""
    return subprocess.run(['sollya'], input=script + '\nquit;\n', capture_output=True, text=True,
                          check=True).stdout


def fit(func, first, degree, interval, mode, linear='1'):
    """fpminimax's coefficients for func on interval, and the fit's error as a power of 2.

    The monomials run from first to degree. A first of 2 fixes the linear term at linear x (linear
    a Sollya expression for a double, 1 by default), so the fit is of func - linear x and the
    polynomial is linear x plus the coefficients' terms. Either way p(0) = 0 exactly.
    """
    mon = '[|' + ', '.join(str(i) for i in range(first, degree + 1)) + '|]'
    fitted = 'f' if first == 1 else f'f - ({linear}) * x'
    whole = 'q' if first == 1 else f'({linear}) * x + q'
    err = '(p - f)' if mode == 'absolute' else '((p - f) / f)'
    script = f'''prec = 300; display = hexadecimal;
f = {func};
I = {interval};
q = fpminimax({fitted}, {mon}, [|D...|], I, {mode}, 0);
p = {whole};
for i from {first} to {degree} do print(coeff(q, i));
display = decimal;
print(dirtyinfnorm({err}, I));'''
    lines = [line for line in sollya(script).splitlines()
             if line.strip() and 'precision' not in line and 'Display' not in line]
    return [float.fromhex(line) for line in lines[:-1]], float(mpmath.log(mpmath.mpf(lines[-1]), 2))


def print_poly(title, array, fitted):
    coeffs, err = fitted
    print(f'\n// {title}, error 2^{err:.2f}')
    print(f'const double {array}[{len(coeffs)}] = {{{", ".join(c.hex() for c in coeffs)}}};')


def log_table():
    """Each interval's (scale, log2(scale), least r, greatest r, least m, greatest m)."""
    entries = []
    for i in range(1 << LOG_BITS):
        lo = mpmath.mpf(f32_from_bits(LOG_OFFSET + (i << (23 - LOG_BITS))))
        hi = mpmath.mpf(f32_from_bits(LOG_OFFSET + ((i + 1) << (23 - LOG_BITS))))
        scale = 1.0 if i == LOG_ONE_INDEX else round_f32(2 / (lo + hi))
        entries.append((scale, mpmath.log(scale, 2), lo * scale - 1, hi * scale - 1, lo, hi))
    return entries


def main():
    n = 1 << EXP2_BITS
    print('// constants')
    print(f'{n} log2(e) =', hexd(n / mpmath.log(2)))
    print('ln(2)       =', hexd(mpmath.log(2)))

    # The bits of 2^(i/n) less i << 47, so that adding k << 47 for k = i mod n gives 2^(k/n)
    print(f'\n// the bits of 2^(i/{n}) less i << 47')
    for i in range(n):
        end = '\n' if i % 4 == 3 else ''
        bits = (f64_bits(mpmath.mpf(2) ** (mpmath.mpf(i) / n)) - (i << 47)) % (1 << 64)
        print(f' 0x{bits:016x}u,' + end, end='')

    print_poly(f'2^(r/{n}) - 1 on [-1/2, 1/2], absolute', 'rc_exp2_f32_coefficients',
               fit(f'2^(x/{n}) - 1', 1, EXP2_DEGREE, '[-1/2; 1/2]', 'absolute'))

    entries = log_table()
    rmin = min(e[2] for e in entries)
    rmax = max(e[3] for e in entries)
    interval = f'[{hexd(rmin)}; {hexd(rmax)}]'
    print(f'\n// log table: scale, log2(scale); r in [{float(rmin)}, {float(rmax)}]')
    for scale, log2_scale, _, _, lo, hi in entries:
        print(f'    {{{hexd(scale)}, {hexd(log2_scale)}}},   // [{float(lo)}, {float(hi)})')

    print_poly('log(1 + r) = r + ..., relative', 'rc_log_f32_coefficients',
               fit('log(1 + x)', 2, LOG_DEGREE, interval, 'relative'))
    # log2's linear coefficient is fixed at 1 / ln 2 rounded to double, which keeps the error tiny
    # near r = 0, where pow's x near 1 with a large y would magnify it, at a small cost elsewhere
    b1 = 'round(1 / log(2), D, RN)'
    higher, err = fit('log2(1 + x)', 2, LOG2_DEGREE, interval, 'relative', b1)
    b1_value = float.fromhex(sollya(f'display = hexadecimal; print({b1});').strip().splitlines()[-1])
    print_poly('log2(1 + r), relative', 'rc_log2_f32_coefficients', ([b1_value] + higher, err))


main()
