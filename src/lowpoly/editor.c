// editor.c
//
// The mesh editor's top level. For now it only proves that richc links in.

#include "editor.h"

#include "richc/arena.h"


int32_t editor_run(void)
{
    rc_arena arena = rc_arena_make_default();

    rc_arena_release(&arena);
    return 0;
}


#ifdef LOWPOLY_TESTS

#include "richc/test.h"


RC_TEST(editor, run_succeeds)
{
    RC_CHECK(editor_run(), ==, 0);
}

#endif
