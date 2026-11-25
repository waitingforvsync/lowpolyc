// array/mstr.h
//
// The view, span and array of rc_mstr.
//
// Each rc_mstr owns memory of its own, which releasing the array does not touch.

#ifndef RC_ARRAY_MSTR_H_
#define RC_ARRAY_MSTR_H_

#include "richc/mstr.h"

#define RC_ARRAY_TYPE rc_mstr
#define RC_ARRAY_NAME mstr
#include "richc/array.template.h"

#endif // RC_ARRAY_MSTR_H_
