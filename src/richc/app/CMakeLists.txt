# ---- app library ----

add_library(richc_app STATIC
    include/richc/app/app.h
    include/richc/gfx/gfx.h
    src/gl_context.h
    src/log.h
)

target_include_directories(richc_app
    PUBLIC  include
    PRIVATE src)

target_link_libraries(richc_app PUBLIC richc)

# One platform per target, chosen here rather than by #if
if(RICHC_WASM)
    # The GL backend comes to wasm with gl.js; until then the placeholder stands in
    target_sources(richc_app PRIVATE
        src/placeholder.c
        src/platform/wasm/app.c)
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED COMPONENTS EGL)
    target_sources(richc_app PRIVATE
        src/gfx/gl/gfx.c
        src/gfx/gl/gl.c
        src/gfx/gl/gl.h
        src/platform/x11/app.c
        src/platform/x11/gl_context.c
        src/platform/x11/x11.h)
    target_link_libraries(richc_app PRIVATE X11::X11 OpenGL::EGL)
else()
    message(FATAL_ERROR "richc_app has no windowing backend for this platform yet: configure with RICHC_BUILD_APP=OFF")
endif()
