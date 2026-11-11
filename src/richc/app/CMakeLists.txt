# ---- app library ----

add_library(richc_app STATIC
    src/placeholder.c
    include/richc/app/app.h
    include/richc/gfx/gfx.h
)

target_include_directories(richc_app
    PUBLIC  include
    PRIVATE src)

target_link_libraries(richc_app PUBLIC richc)

# One windowing backend per target, chosen here rather than by #if
if(RICHC_WASM)
    target_sources(richc_app PRIVATE src/platform/wasm/app.c)
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_sources(richc_app PRIVATE src/platform/x11/app.c)
    target_link_libraries(richc_app PRIVATE X11::X11)
else()
    message(FATAL_ERROR "richc_app has no windowing backend for this platform yet: configure with RICHC_BUILD_APP=OFF")
endif()
