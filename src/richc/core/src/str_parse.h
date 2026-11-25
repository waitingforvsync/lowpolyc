// str_parse.h
//
// What parsing integers and floats share, private to core: str_parse.c parses integers and
// str_parse_float.c floats.

#ifndef RC_STR_PARSE_H_
#define RC_STR_PARSE_H_

#include "richc/str.h"

// The value of c as a digit in any base up to 36, letters in either case, or 36 if it is none.
uint32_t rc_parse_digit_value(char c);

// True if s starts with a minus sign.
bool rc_parse_is_minus(rc_str s);

// True if s starts with 0 and letter, in either case, then a digit of base.
bool rc_parse_has_prefix(rc_str s, char letter, uint32_t base);

// The error for a parse of all of a string: a number with text after it is a syntax error.
uint8_t rc_parse_whole_error(uint8_t error, rc_str rest);

#endif // RC_STR_PARSE_H_
