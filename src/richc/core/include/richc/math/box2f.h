// math/box2f.h
//
// A 2D axis-aligned box of floats, half open: min is inside it and max is not, as in a pixel grid.
// A 10 by 10 box at the origin has max (10, 10), and its right and bottom edges are outside it.
//
// min is never above max in either component. Every constructor makes it so and every query relies
// on it, but C can't stop a box being built by hand, so the corners are internal members (with a
// trailing underscore) to be read through rc_box2f_min and rc_box2f_max.

#ifndef RC_MATH_BOX2F_H_
#define RC_MATH_BOX2F_H_

#include "richc/math/vec2f.h"

typedef struct rc_box2f {
    rc_vec2f min_;
    rc_vec2f max_;
} rc_box2f;

// ---- construction ----

// The box with corners a and b, in either order.
static inline rc_box2f rc_box2f_make(rc_vec2f a, rc_vec2f b)
{
    return (rc_box2f) {
        .min_ = rc_vec2f_component_min(a, b),
        .max_ = rc_vec2f_component_max(a, b)
    };
}

// The box from pos to pos + size. size must not be negative.
static inline rc_box2f rc_box2f_make_pos_size(rc_vec2f pos, rc_vec2f size)
{
    return (rc_box2f) {.min_ = pos, .max_ = rc_vec2f_add(pos, size)};
}

// The box with corners a and b, in either order, grown by margin on every side.
static inline rc_box2f rc_box2f_make_with_margin(rc_vec2f a, rc_vec2f b, float margin)
{
    rc_vec2f grow = rc_vec2f_make(margin, margin);

    return (rc_box2f) {
        .min_ = rc_vec2f_sub(rc_vec2f_component_min(a, b), grow),
        .max_ = rc_vec2f_add(rc_vec2f_component_max(a, b), grow)
    };
}

// ---- accessors ----

// The corner inside the box.
static inline rc_vec2f rc_box2f_min(rc_box2f box)
{
    return box.min_;
}

// The corner just outside the box.
static inline rc_vec2f rc_box2f_max(rc_box2f box)
{
    return box.max_;
}

// The width and height of the box.
static inline rc_vec2f rc_box2f_size(rc_box2f box)
{
    return rc_vec2f_sub(box.max_, box.min_);
}

// ---- queries ----

// True if the box holds no area, having no width or no height.
static inline bool rc_box2f_is_empty(rc_box2f box)
{
    return box.min_.x >= box.max_.x || box.min_.y >= box.max_.y;
}

// True if the span from inner_min to inner_max lies within outer_min to outer_max.
static inline bool rc_box2f_span_within_(float outer_min, float outer_max, float inner_min, float inner_max)
{
    return outer_min <= inner_min && inner_max <= outer_max;
}

// True if all of b is inside a. Both being half open, comparing the corners directly is right.
static inline bool rc_box2f_contains(rc_box2f a, rc_box2f b)
{
    return rc_box2f_span_within_(a.min_.x, a.max_.x, b.min_.x, b.max_.x) &&
        rc_box2f_span_within_(a.min_.y, a.max_.y, b.min_.y, b.max_.y);
}

// True if the half-open spans [a_min, a_max) and [b_min, b_max) share a point.
static inline bool rc_box2f_spans_overlap_(float a_min, float a_max, float b_min, float b_max)
{
    return a_min < b_max && b_min < a_max;
}

// True if the spans of a and b overlap on both axes. Boxes that only touch at an edge don't, but
// an empty box inside the other one does.
static inline bool rc_box2f_intersects(rc_box2f a, rc_box2f b)
{
    return rc_box2f_spans_overlap_(a.min_.x, a.max_.x, b.min_.x, b.max_.x) &&
        rc_box2f_spans_overlap_(a.min_.y, a.max_.y, b.min_.y, b.max_.y);
}

// True if the half-open span [min, max) holds value.
static inline bool rc_box2f_span_holds_(float min, float max, float value)
{
    return min <= value && value < max;
}

// True if p is inside the box: on or past min, and short of max.
static inline bool rc_box2f_contains_point(rc_box2f box, rc_vec2f p)
{
    return rc_box2f_span_holds_(box.min_.x, box.max_.x, p.x) && rc_box2f_span_holds_(box.min_.y, box.max_.y, p.y);
}

// ---- combination ----

// The smallest box holding both a and b.
static inline rc_box2f rc_box2f_union(rc_box2f a, rc_box2f b)
{
    return (rc_box2f) {
        .min_ = rc_vec2f_component_min(a.min_, b.min_),
        .max_ = rc_vec2f_component_max(a.max_, b.max_)
    };
}

// The largest box inside both a and b. When they don't overlap it is empty, with max pulled back
// to min, so it is still a proper box; test it with rc_box2f_is_empty.
static inline rc_box2f rc_box2f_intersection(rc_box2f a, rc_box2f b)
{
    rc_vec2f min = rc_vec2f_component_max(a.min_, b.min_);
    rc_vec2f max = rc_vec2f_component_min(a.max_, b.max_);

    return (rc_box2f) {
        .min_ = min,
        .max_ = rc_vec2f_component_max(min, max)
    };
}

// The box grown to take in p as a corner. A p beyond max becomes the new max, which is outside the
// box, as max always is.
static inline rc_box2f rc_box2f_expand(rc_box2f box, rc_vec2f p)
{
    return (rc_box2f) {
        .min_ = rc_vec2f_component_min(box.min_, p),
        .max_ = rc_vec2f_component_max(box.max_, p)
    };
}

// ---- transformation ----

// The box moved by delta.
static inline rc_box2f rc_box2f_translate(rc_box2f box, rc_vec2f delta)
{
    return (rc_box2f) {
        .min_ = rc_vec2f_add(box.min_, delta),
        .max_ = rc_vec2f_add(box.max_, delta)
    };
}

// ---- comparison ----

// True if a and b have the same corners.
static inline bool rc_box2f_is_equal(rc_box2f a, rc_box2f b)
{
    return rc_vec2f_is_equal(a.min_, b.min_) && rc_vec2f_is_equal(a.max_, b.max_);
}

// True if each corner of a is less than tolerance from b's.
static inline bool rc_box2f_is_nearly_equal(rc_box2f a, rc_box2f b, float tolerance)
{
    return rc_vec2f_is_nearly_equal(a.min_, b.min_, tolerance) && rc_vec2f_is_nearly_equal(a.max_, b.max_, tolerance);
}

#endif // RC_MATH_BOX2F_H_
