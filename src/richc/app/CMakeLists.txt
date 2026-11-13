# ---- app library ----

add_library(richc_app STATIC
    src/keys.c
    include/richc/app/app.h
    include/richc/app/keys.h
    include/richc/gfx/gfx.h
    src/gl_context.h
    src/keys.h
    src/log.h
)

target_include_directories(richc_app
    PUBLIC  include
    PRIVATE src)

target_link_libraries(richc_app PUBLIC richc)

# One platform per target, chosen here rather than by #if
if(RICHC_WASM)
    target_sources(richc_app PRIVATE
        src/gfx/gl/gfx.c
        src/gfx/gl/gl.h
        src/platform/wasm/app.c
        src/platform/wasm/gl_context.c)

    # The page scripts that supply the module's imports. The app's own page copies them into
    # its site, under richc/.
    set_property(TARGET richc_app PROPERTY RICHC_WEB_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/app.js
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/gl.js)
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)

    # Just EGL: FindOpenGL wants libOpenGL as well, which we never use
    find_path(RICHC_EGL_INCLUDE_DIR EGL/egl.h REQUIRED)
    find_library(RICHC_EGL_LIBRARY EGL REQUIRED)
    target_include_directories(richc_app PRIVATE ${RICHC_EGL_INCLUDE_DIR})
    target_sources(richc_app PRIVATE
        src/gfx/gl/gfx.c
        src/gfx/gl/gl.c
        src/gfx/gl/gl.h
        src/platform/x11/app.c
        src/platform/x11/gl_context.c
        src/platform/x11/x11.h)
    target_link_libraries(richc_app PRIVATE X11::X11 ${RICHC_EGL_LIBRARY})
elseif(WIN32)
    # Not written yet: the stub stops the build with an #error
    target_sources(richc_app PRIVATE src/platform/win32/app.c)
elseif(APPLE)
    # Not written yet: the stub stops the build with an #error
    target_sources(richc_app PRIVATE src/platform/cocoa/app.c)
else()
    message(FATAL_ERROR "richc_app has no platform for ${CMAKE_SYSTEM_NAME} yet: configure with RICHC_BUILD_APP=OFF")
endif()

if(RICHC_BUILD_TESTS)
    add_subdirectory(test)
endif()
