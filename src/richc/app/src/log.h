// log.h
//
// Private to richc_app: reporting failures from code shared between targets. Each platform
// implements it: stderr natively, the console on wasm.

#ifndef RC_APP_LOG_H_
#define RC_APP_LOG_H_

#include "richc/str.h"

// Write message as one line.
void rc_app_log(rc_str message);

#endif // RC_APP_LOG_H_
