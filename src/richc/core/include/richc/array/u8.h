// array/u8.h
//
// The view, span and array of uint8_t.
//
// A different type from rc_view_bytes and its kin in bytes.h, which are the same elements under
// another name, with bridges to strings.

#ifndef RC_ARRAY_U8_H_
#define RC_ARRAY_U8_H_

#define RC_ARRAY_TYPE uint8_t
#define RC_ARRAY_NAME u8
#include "richc/array.template.h"

#endif // RC_ARRAY_U8_H_
