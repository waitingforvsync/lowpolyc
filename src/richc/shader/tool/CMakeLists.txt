# ---- shader tool ----

# Translates a .wgsl file into a C header holding its shader package. It runs during the build,
# so it is built for the machine doing the building: see rc_add_shaders.
add_executable(richc_shader_tool
    main.c
)

target_link_libraries(richc_shader_tool PRIVATE richc_shader)
