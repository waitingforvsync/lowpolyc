# ---- app library ----

# Nothing in it yet, so an interface library for now: dependents can already
# link richc_app. It becomes a static library with its first source file.
add_library(richc_app INTERFACE)
target_link_libraries(richc_app INTERFACE richc)
