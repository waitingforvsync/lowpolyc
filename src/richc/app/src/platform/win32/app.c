// app.c
//
// Win32: not written yet. It needs rc_app_run (a window, its message loop and the frame loop) and
// rc_app_log, as the X11 platform has, plus a GL context (WGL) or another gfx backend.
//
// The keyboard maps onto Win32 as it does onto X11, through the shared rules in keys.h:
//
//   scancodes  WM_KEYDOWN and WM_KEYUP's lParam (bits 16 to 23, with bit 24 marking extended
//              keys such as the arrows), through a table to rc_scancode
//   repeats    WM_KEYDOWN with lParam bit 30 set (the key was already down) is a key repeat
//   keycodes   ToUnicodeEx on the key with an empty key state gives its layout character for
//              rc_app_keycode
//   chars      WM_CHAR, after TranslateMessage has applied the layout and dead keys (joining
//              UTF-16 surrogate pairs); editing keys from WM_KEYDOWN's virtual key, repeats
//              included, since arrows and the like send no WM_CHAR
//   modifiers  GetKeyState for shift, ctrl, alt and the Windows keys, and the toggles of caps lock
//              and num lock
//   focus      WM_KILLFOCUS sends key ups for every key still down

#error "richc_app has no Win32 platform yet: configure with RICHC_BUILD_APP=OFF"
