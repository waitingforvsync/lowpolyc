// keys.h
//
// Private to richc_app: the keycode rules from richc/app/keys.h, applied in one place. Each
// platform finds a key's scancode and the character its layout gives it, and these turn that into
// the keycode, so every platform gives the same answer.

#ifndef RC_APP_PRIVATE_KEYS_H_
#define RC_APP_PRIVATE_KEYS_H_

#include "richc/app/keys.h"
#include "richc/macros.h"

// The keycode (an rc_key) of the key at scancode, which types layout_char in the current layout
// with no modifiers, or 0 if the platform cannot tell or the key types nothing.
uint32_t rc_app_keycode(rc_scancode scancode, uint32_t layout_char);

// True if codepoint is text a char event can carry: not a control character.
bool rc_app_is_text(uint32_t codepoint);

#endif // RC_APP_PRIVATE_KEYS_H_
