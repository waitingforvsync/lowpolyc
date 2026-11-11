// main.c
//
// The wasm entry point. There is no main on wasm32: the page's script
// (web/lowpoly.js) calls start once the module has loaded.

#include "editor.h"


__attribute__((export_name("start"))) int32_t start(void)
{
    return editor_run();
}
