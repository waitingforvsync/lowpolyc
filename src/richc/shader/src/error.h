// error.h
//
// Private to richc_shader: making an rc_wgsl_error from a place in the source.

#ifndef RC_SHADER_ERROR_H_
#define RC_SHADER_ERROR_H_

#include "richc/shader/wgsl.h"

// The error message at offset bytes into source, with its line and column worked out.
rc_wgsl_error rc_wgsl_error_at(rc_str source, uint32_t offset, rc_str message);

#endif // RC_SHADER_ERROR_H_
