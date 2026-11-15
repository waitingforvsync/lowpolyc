// print.h
//
// Text to the standard output and error streams, written as it is: no formatting, and no line
// break added, so put in your own. Build anything fancier in an rc_mstr first, with the
// rc_mstr_append_* functions turning numbers into text.
//
// Natively the text goes straight to the stream, unbuffered. On wasm it goes to the page's console,
// a line at a time: richc/print.js holds on to text until a line break comes.

#ifndef RC_PRINT_H_
#define RC_PRINT_H_

#include "richc/str.h"

// Write text to the standard output.
void rc_print(rc_str text);

// Write text to the standard error.
void rc_print_err(rc_str text);

#endif // RC_PRINT_H_
