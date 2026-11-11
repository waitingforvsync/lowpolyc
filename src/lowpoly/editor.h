// editor.h
//
// The mesh editor's top level, which each target's main.c runs.

#ifndef LOWPOLY_EDITOR_H_
#define LOWPOLY_EDITOR_H_

#include <stdint.h>

// Run the editor: natively until its window closes; on wasm, it returns once set up.
// Returns 0 on success.
int32_t editor_run(void);

#endif // LOWPOLY_EDITOR_H_
