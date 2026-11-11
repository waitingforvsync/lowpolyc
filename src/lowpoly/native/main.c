// main.c
//
// The native entry point. When built with LOWPOLY_TESTS, `lowpoly --test [filter]` runs the tests
// compiled into the executable instead of the editor.

#include "editor.h"

#include <stdbool.h>

#ifdef LOWPOLY_TESTS
#  include "richc/test.h"
#endif


#ifdef LOWPOLY_TESTS
// True if the two strings are equal.
static bool is_same_string(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
#endif


int main(int argc, char **argv)
{
#ifdef LOWPOLY_TESTS
    if (argc > 1 && is_same_string(argv[1], "--test")) {
        return rc_test_run(argc > 2 ? argv[2] : "") != 0;
    }
#else
    (void)argc;
    (void)argv;
#endif

    return editor_run();
}
