# ---- core static library ----

add_library(richc STATIC
    src/arena.c
    src/bitset.c
    src/file.c
    src/hash.c
    src/hash_index.c
    src/mstr.c
    src/mstr_float.c
    src/str.c
    include/richc/arena.h
    include/richc/array.template.h
    include/richc/bitset.h
    include/richc/bytes.h
    include/richc/file.h
    include/richc/genpool.template.h
    include/richc/handle.template.h
    include/richc/hash.h
    include/richc/hash_index.h
    include/richc/hash_index.template.h
    include/richc/hash_trie.template.h
    include/richc/hash_trie_foreach.template.h
    include/richc/macros.h
    include/richc/mem.h
    include/richc/mstr.h
    include/richc/ops.h
    include/richc/pool.template.h
    include/richc/print.h
    include/richc/str.h
    src/fs.h
    src/heap.h
)

target_include_directories(richc
    PUBLIC  include
    PRIVATE src)

# One platform backend per target, chosen here rather than by #if
if(RICHC_WASM)
    target_sources(richc PRIVATE
        src/platform/wasm/fs.c
        src/platform/wasm/heap.c
        src/platform/wasm/print.c
        src/platform/wasm/runtime.c)

    # The page scripts that supply the file and print imports. The app's own page copies them into
    # its site, under richc/.
    set_property(TARGET richc PROPERTY RICHC_WEB_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/fs.js
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wasm/print.js)
elseif(WIN32)
    target_sources(richc PRIVATE
        src/platform/win32/fs.c
        src/platform/win32/heap.c
        src/platform/win32/print.c)
else()
    target_sources(richc PRIVATE
        src/platform/posix/fs.c
        src/platform/posix/heap.c
        src/platform/posix/print.c)
endif()

# ---- test framework (native only) ----

# Built whenever the target is native, so lowpoly's tests can use it even when
# richc's own tests are off.
if(NOT RICHC_WASM)
    add_library(richc_test STATIC
        src/test.c
        include/richc/test.h
    )

    target_link_libraries(richc_test PUBLIC richc)
    if(NOT WIN32)
        # libm is part of the CRT on Windows; elsewhere it is a separate library
        target_link_libraries(richc_test PRIVATE m)
    endif()
endif()

# ---- tests ----

if(RICHC_BUILD_TESTS)
    add_subdirectory(test)
endif()
