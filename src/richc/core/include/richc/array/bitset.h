// array/bitset.h
//
// The view, span and array of rc_bitset.
//
// Each rc_bitset owns memory of its own, which releasing the array does not touch.

#ifndef RC_ARRAY_BITSET_H_
#define RC_ARRAY_BITSET_H_

#include "richc/bitset.h"

#define RC_ARRAY_TYPE rc_bitset
#define RC_ARRAY_NAME bitset
#include "richc/array.template.h"

#endif // RC_ARRAY_BITSET_H_
