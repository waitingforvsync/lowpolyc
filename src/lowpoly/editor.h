// editor.h
//
// The mesh editor's top level. A stub for now: it links with richc and does
// nothing else. Each target's main.c calls editor_run.

#ifndef LOWPOLY_EDITOR_H_
#define LOWPOLY_EDITOR_H_

#include <stdint.h>

// Run the editor. Returns 0 on success.
int32_t editor_run(void);

#endif // LOWPOLY_EDITOR_H_
