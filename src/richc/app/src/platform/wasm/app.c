// app.c
//
// wasm32. Stands in until the canvas and frame loop exist: rc_app_run calls init and deinit, and
// no frames run.

#include "richc/app/app.h"


int32_t rc_app_run(const rc_app_desc *desc)
{
    RC_ASSERT(desc);
    if (desc->init) {
        desc->init(desc->user);
    }
    if (desc->deinit) {
        desc->deinit(desc->user);
    }
    return 0;
}
