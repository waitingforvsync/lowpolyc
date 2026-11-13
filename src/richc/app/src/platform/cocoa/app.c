// app.c
//
// macOS (Cocoa): not written yet, and will be Objective-C (app.m). It needs rc_app_run (an
// NSApplication, a window and the frame loop) and rc_app_log, as the X11 platform has, plus a gfx
// backend (Metal, or GL through ANGLE).
//
// The keyboard maps onto Cocoa as it does onto X11, through the shared rules in keys.h:
//
//   scancodes  NSEvent's keyCode (the kVK_ virtual key codes, which name positions), through a
//              table to rc_scancode
//   repeats    keyDown: with isARepeat set is a key repeat
//   keycodes   UCKeyTranslate on the current keyboard layout with no modifiers gives the layout
//              character for rc_app_keycode
//   chars      interpretKeyEvents: through NSTextInputClient's insertText: for text, which applies
//              the layout and dead keys; editing keys from the characters, whose function-key code
//              points are already rc_key's (backspace arrives as 0x7F and becomes 0x08)
//   modifiers  NSEvent's modifierFlags (command is super, option is alt)
//   focus      windowDidResignKey: sends key ups for every key still down

#error "richc_app has no macOS platform yet: configure with RICHC_BUILD_APP=OFF"
