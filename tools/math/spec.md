# richc transcendentals: specification

This specifies richc's 24 transcendental functions: what each must return, how accurately, and
under what constraints. It deliberately says nothing about how to compute them. Implement them
from this document, your own knowledge of numerical methods, and polynomial fits and constants you
derive yourself.

## 1. The functions

Declared in `src/richc/core/include/richc/math/math.h` (do not change the declarations):

| Function | f32 | f64 | Meaning |
|---|---|---|---|
| sin | `float rc_sin_f32(float x)` | `double rc_sin_f64(double x)` | sine of x radians |
| cos | `rc_cos_f32` | `rc_cos_f64` | cosine of x radians |
| tan | `rc_tan_f32` | `rc_tan_f64` | tangent of x radians |
| asin | `rc_asin_f32` | `rc_asin_f64` | arcsine, in [-pi/2, pi/2] |
| acos | `rc_acos_f32` | `rc_acos_f64` | arccosine, in [0, pi] |
| atan | `rc_atan_f32` | `rc_atan_f64` | arctangent, in [-pi/2, pi/2] |
| atan2 | `float rc_atan2_f32(float y, float x)` | `double rc_atan2_f64(double y, double x)` | angle of the point (x, y) from the +x axis, in [-pi, pi] |
| exp | `rc_exp_f32` | `rc_exp_f64` | e^x |
| log | `rc_log_f32` | `rc_log_f64` | natural logarithm |
| log2 | `rc_log2_f32` | `rc_log2_f64` | base-2 logarithm |
| pow | `float rc_pow_f32(float x, float y)` | `double rc_pow_f64(double x, double y)` | x^y |
| cbrt | `rc_cbrt_f32` | `rc_cbrt_f64` | real cube root |

Every function is defined for every input bit pattern, including subnormals, infinities and NaNs,
and the trigonometric functions for every finite argument however large.

## 2. Accuracy

Error is measured in ulps of the result's type, at the true value: |computed - true| / ulp(true),
where ulp(t) is 2^(e - p + 1) for t in [2^e, 2^(e+1)), p being 24 (f32) or 53 (f64), and for
subnormal and zero t the ulp is the smallest subnormal, 2^-149 or 2^-1074. A true value beyond the
largest finite number must round to the same infinity as the correctly rounded result.

Each function must stay within its limit on every input:

| Function | f32 limit (ulp) | f64 limit (ulp) |
|---|---|---|
| sin | 0.51 | 0.85 |
| cos | 0.51 | 0.85 |
| tan | 0.81 | 0.95 |
| asin | 0.75 | 0.95 |
| acos | 0.92 | 0.95 |
| atan | 0.86 | 0.90 |
| atan2 | 1.6 | 1.6 |
| exp | 0.51 | 0.52 |
| log | 0.82 | 0.53 |
| log2 | 0.76 | 0.56 |
| pow | 0.82 | 0.52 |
| cbrt | 0.501 | 0.70 |

A limit below 1 means an exactly representable true value must be returned exactly (every other
number is at least 1 ulp away). Nearly every limit below about 0.52 effectively means "correctly
rounded but for very rare hard cases".

The limits are enforced by `richc_math_accuracy` (src/richc/core/test/math_accuracy.c), which:
- runs each f32 function of one argument on all 2^32 inputs, against glibc's double function;
- runs each f64 function on random bit patterns and on random values in a telling range, against
  glibc's long double function (sin/cos/tan: |x| < 1e6; asin/acos: [-1, 1]; atan: |x| < 1e3;
  exp: [-750, 750]; log/log2: (0, 1e10); cbrt: |x| < 1e10);
- runs atan2 and pow on random bit pairs and on random pairs in a range (atan2: both in
  [-1e3, 1e3]; pow: x in [0, 100), y in [-100, 100)), and on every pair of a list of special values.
The limits are not to be loosened. If one genuinely cannot be met, stop and report.

## 3. Special values

"pi", "pi/2", "pi/4" and "3pi/4" below mean those numbers correctly rounded to the result type.
NaN results may have any payload and sign; a NaN input gives a NaN result unless stated otherwise.

**Symmetry, bit for bit:** sin, tan, asin, atan and cbrt are odd: f(-x) is exactly -f(x) for every
x, zeros included. cos is even: cos(-x) is exactly cos(x). atan2(-y, x) is exactly -atan2(y, x).

| Function | Input | Result |
|---|---|---|
| sin, tan | +-0 | +-0 (same sign) |
| sin, cos, tan | +-inf | NaN |
| cos | +-0 | 1 |
| asin | +-0 | +-0 |
| asin | +-1 | +-pi/2 |
| asin, acos | \|x\| > 1, +-inf | NaN |
| acos | 1 | +0 |
| acos | -1 | pi |
| acos | +-0 | pi/2 |
| atan | +-0 | +-0 |
| atan | +-inf | +-pi/2 |
| exp | +-0 | 1 |
| exp | +inf | +inf |
| exp | -inf | +0 |
| exp | x large enough that e^x overflows | +inf |
| exp | x small enough that e^x rounds to 0 | +0 |
| log, log2 | +-0 | -inf |
| log, log2 | 1 | +0 |
| log, log2 | x < 0 (including -inf) | NaN |
| log, log2 | +inf | +inf |
| log2 | 2^k (any integer k, subnormal powers too) | k, exactly |
| cbrt | +-0 | +-0 |
| cbrt | +-inf | +-inf |
| cbrt | an exact cube of a representable number | its root, exactly |

atan2(y, x), as C17 Annex F.10.1.4 has it:

| y | x | Result |
|---|---|---|
| +-0 | -0 | +-pi |
| +-0 | +0 | +-0 |
| +-0 | x < 0 | +-pi |
| +-0 | x > 0 | +-0 |
| y < 0 | +-0 | -pi/2 |
| y > 0 | +-0 | pi/2 |
| +-y, y > 0 finite | -inf | +-pi |
| +-y, y > 0 finite | +inf | +-0 |
| +-inf | finite x | +-pi/2 |
| +-inf | -inf | +-3pi/4 |
| +-inf | +inf | +-pi/4 |
| NaN in either | | NaN |

pow(x, y), as C17 Annex F.10.4.4 has it. "y odd" means y is an odd integer; every f32 of magnitude
2^24 or more, and every f64 of magnitude 2^53 or more, is an even integer.

| x | y | Result |
|---|---|---|
| any, even NaN | +-0 | 1 |
| +1 | any, even NaN | 1 |
| -1 | +-inf | 1 |
| +-0 | y < 0, y odd | +-inf (the sign of x) |
| +-0 | y < 0, finite, not odd | +inf |
| +-0 | -inf | +inf |
| +-0 | y > 0, y odd | +-0 (the sign of x) |
| +-0 | y > 0, not odd | +0 |
| \|x\| < 1 | -inf | +inf |
| \|x\| > 1 | -inf | +0 |
| \|x\| < 1 | +inf | +0 |
| \|x\| > 1 | +inf | +inf |
| -inf | y < 0, y odd | -0 |
| -inf | y < 0, not odd | +0 |
| -inf | y > 0, y odd | -inf |
| -inf | y > 0, not odd | +inf |
| +inf | y < 0 | +0 |
| +inf | y > 0 | +inf |
| finite x < 0 | finite y, not an integer | NaN |
| finite x < 0 | finite integer y | \|x\|^y, negated if y is odd |
| any other with a NaN | | NaN |

Overflow gives an infinity and underflow a zero, of the sign the rules above give, and results
that land among the subnormals must still be within the limit, measured in subnormal ulps.

## 4. Constraints

- **The same bits on every target.** The code may rely only on IEEE 754 basic operations (+, -, *,
  /, conversions) in float and double, the richc functions in math.h (rc_sqrt_*, rc_floor_*,
  rc_trunc_*, rc_round_even_*, rc_abs_*, rc_copysign_*, rc_bitcast_*, ...), and integer
  arithmetic. No fused multiply-add (we build with -ffp-contract=off, and there is no rc_fma), no
  long double, no __float128, no compiler builtins other than through richc, and no dependence on
  the rounding mode (it is always round-to-nearest-even), on flush-to-zero, or on NaN payloads.
  Never convert an out-of-range float to an integer. f32 functions may compute in double.
- **No libc**, and no libm. Library code includes only richc headers.
- **Speed.** Comparable to a good libm: within about twice glibc's time for the same function, on
  ordinary arguments, measured natively. Trigonometric reduction of huge arguments may be slower.
  Write branch-free where it is practical, so loops over a function can vectorise.
- **Size.** Tables are fine, but keep each function's to a few KiB (8 KiB at most), and share them
  where functions can.
- **Linking.** One source file per public function and precision, named `<name>_f32.c` and
  `<name>_f64.c` in src/richc/core/src/math/, so a program links only what it calls. Shared helpers
  and tables go in other files there, declared in a private header of your naming, with internal
  names starting `rc_`.
- **Thread safety and purity.** No state, no globals other than const tables, and no lazy
  initialisation.
- **Style.** CODING_STANDARDS.md applies in full. Each file starts with our banner saying what it
  is and, briefly, how it works (reduction, approximation, error budget). There is no third-party
  notice to carry: this is original work.
