// app.c
//
// wasm32: the app on a web page. The page's script (app.js) owns the canvas and the loop:
// rc_app_run sets the app up and returns, then the script calls rc_wasm_frame on each animation
// frame and rc_wasm_key for each key event.

#include "richc/app/app.h"

#include "log.h"

#define RC_WASM_IMPORT_(name) __attribute__((import_module("richc"), import_name(name)))
#define RC_WASM_EXPORT_(name) __attribute__((export_name(name)))

// Set the page's title from UTF-8 text.
RC_WASM_IMPORT_("app_set_title") void rc_wasm_set_title(const char *data, uint32_t len);

// Write a line to the console as an error.
RC_WASM_IMPORT_("app_log") void rc_wasm_log(const char *data, uint32_t len);

// The app, copied from rc_app_run's desc, which need only last as long as the call.
static rc_app_desc rc_wasm_app;

// Whether rc_app_run has run, so the page's calls have an app to go to.
static bool rc_wasm_running;

int32_t rc_app_run(const rc_app_desc *desc)
{
    RC_ASSERT(desc);
    rc_wasm_app = *desc;
    rc_wasm_running = true;
    if (!rc_str_is_empty(desc->title)) {
        rc_wasm_set_title(desc->title.data, desc->title.len);
    }
    if (desc->init) {
        desc->init(desc->user);
    }
    return 0;
}


void rc_app_log(rc_str message)
{
    rc_wasm_log(message.data, message.len);
}


// ---- called by the page ----

// One displayed frame. The browser presents when the animation frame callback returns.
RC_WASM_EXPORT_("rc_wasm_frame") void rc_wasm_frame(void);

void rc_wasm_frame(void)
{
    if (rc_wasm_running && rc_wasm_app.frame) {
        rc_wasm_app.frame(rc_wasm_app.user);
    }
}


// A key event: type is an rc_app_event_type and key an rc_key, which app.js maps from the DOM's
// key codes. app.js drops repeats of a held key.
RC_WASM_EXPORT_("rc_wasm_key") void rc_wasm_key(uint32_t type, uint32_t key);

void rc_wasm_key(uint32_t type, uint32_t key)
{
    if (rc_wasm_running && rc_wasm_app.event) {
        rc_app_event event = {.type = (uint8_t)type, .key = (uint16_t)key};

        rc_wasm_app.event(rc_wasm_app.user, &event);
    }
}
