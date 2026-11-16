# check_glsl.cmake
#
# Compiles every GLSL program the shader tests saved, with glslang, and fails if any doesn't
# compile, after showing what glslang says about each. Run as:
#
#   cmake -DGLSLANG=<glslangValidator> -DDIRECTORY=<glsl_out> -P check_glsl.cmake

file(GLOB programs ${DIRECTORY}/*.vert ${DIRECTORY}/*.frag)
list(LENGTH programs count)
if(count EQUAL 0)
    message(FATAL_ERROR "The shader tests saved no GLSL to check in ${DIRECTORY}")
endif()

set(failed 0)
foreach(program ${programs})
    execute_process(
        COMMAND ${GLSLANG} ${program}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output)
    if(NOT result EQUAL 0)
        message("${output}")
        math(EXPR failed "${failed} + 1")
    endif()
endforeach()

if(failed GREATER 0)
    message(FATAL_ERROR "${failed} of ${count} GLSL programs don't compile")
endif()
message(STATUS "All ${count} GLSL programs compile")
