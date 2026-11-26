// wasm_thread.h
//
// What the wasm futex needs to know from the wasm threads, in a build with them. Private to the
// wasm backend.

#ifndef RC_WASM_THREAD_H_
#define RC_WASM_THREAD_H_

#include <stdbool.h>

// Whether the calling thread may sleep in memory.atomic.wait32. Workers may, but a page's main
// thread may not (the wait would trap), so it spins instead; under node it may.
bool rc_wasm_thread_may_block(void);

#endif // RC_WASM_THREAD_H_
