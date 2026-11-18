// math/solve.h
//
// The real roots of polynomials, worked out in closed form. A leading coefficient near zero drops
// to the polynomial of one lower degree, rather than dividing by it.

#ifndef RC_MATH_SOLVE_H_
#define RC_MATH_SOLVE_H_

#include "richc/macros.h"

// The real roots of a quadratic: none, one or two of them.
typedef struct rc_quadratic_roots {
    uint32_t num_roots;
    float root[2];
} rc_quadratic_roots;

// The real roots of a cubic: one or three of them, or fewer if it drops to a lower degree.
typedef struct rc_cubic_roots {
    uint32_t num_roots;
    float root[3];
} rc_cubic_roots;

// The real roots of a t^2 + b t + c = 0. If a is near zero, the root of b t + c = 0, if any.
rc_quadratic_roots rc_solve_quadratic(float a, float b, float c);

// The real roots of a t^3 + b t^2 + c t + d = 0. If a is near zero, the roots of the quadratic.
rc_cubic_roots rc_solve_cubic(float a, float b, float c, float d);

#endif // RC_MATH_SOLVE_H_
