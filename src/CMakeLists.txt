# ---- source tree ----

add_subdirectory(richc)

# lowpoly needs the app layer, which a build of richc's shader tool alone leaves out
if(RICHC_BUILD_APP)
    add_subdirectory(lowpoly)
endif()
