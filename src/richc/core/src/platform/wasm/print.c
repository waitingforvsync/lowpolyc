// print.c
//
// wasm32. Both streams go to the page, where print.js beside this gathers the text into lines for
// the console.

#include "richc/print.h"

// Write len bytes of UTF-8 to the page's standard output.
__attribute__((import_module("richc"), import_name("print")))
void rc_wasm_print_(const char *data, uint32_t len);

// Write len bytes of UTF-8 to the page's standard error.
__attribute__((import_module("richc"), import_name("print_err")))
void rc_wasm_print_err_(const char *data, uint32_t len);

void rc_print(rc_str text)
{
    rc_wasm_print_(text.data, text.len);
}


void rc_print_err(rc_str text)
{
    rc_wasm_print_err_(text.data, text.len);
}
