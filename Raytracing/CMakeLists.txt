cmake_minimum_required(VERSION 3.10)
project(Raytracer)

set(CMAKE_CXX_STANDARD 17)

option(RAYTRACER_ENABLE_NATIVE_OPTIMIZATIONS "Use -march=native (or equivalent) for Release builds" ON)
option(RAYTRACER_GENERATE_DSYM "Generate dSYM bundles on Apple platforms" ON)

add_executable(raytracer
    src/main.cpp
    src/AxisAlignedRect.cpp
    src/Color.cpp
    src/PngWriter.cpp
    src/Renderer.cpp
    src/Scene.cpp
    src/Utils.cpp
    src/Vec3.cpp
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Favor profiler-friendly Debug builds and maximum throughput Release builds.
    target_compile_options(raytracer PRIVATE
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-ggdb3>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        $<$<CONFIG:Debug>:-fno-inline-functions>
        $<$<CONFIG:Debug>:-fno-inline>
        $<$<CONFIG:Debug>:-fno-optimize-sibling-calls>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-ffast-math>
        $<$<CONFIG:Release>:-gline-tables-only>
        $<$<CONFIG:Release>:-funroll-loops>
        $<$<CONFIG:Release>:-fomit-frame-pointer>
        $<$<CONFIG:Release>:-fno-math-errno>
        $<$<CONFIG:Release>:-fno-trapping-math>
        $<$<AND:$<CONFIG:Release>,$<BOOL:${RAYTRACER_ENABLE_NATIVE_OPTIMIZATIONS}>>:-march=native>
        $<$<AND:$<CONFIG:Release>,$<BOOL:${RAYTRACER_ENABLE_NATIVE_OPTIMIZATIONS}>>:-mtune=native>
    )
    if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.9")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()
elseif(MSVC)
    target_compile_options(raytracer PRIVATE
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/Zo>
        $<$<CONFIG:Debug>:/Oy->
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/fp:fast>
    )
endif()

if(APPLE AND RAYTRACER_GENERATE_DSYM)
    find_program(DSYMUTIL_TOOL dsymutil)
    if(DSYMUTIL_TOOL)
        add_custom_command(TARGET raytracer POST_BUILD
            COMMAND ${DSYMUTIL_TOOL} "$<TARGET_FILE:raytracer>"
            COMMENT "Generating dSYM bundle for Instruments"
            VERBATIM
        )
    else()
        message(WARNING "dsymutil tool not found; Apple Instruments will not show symbols for Release builds.")
    endif()
endif()

set_source_files_properties(src/AxisAlignedRect.cpp PROPERTIES
    COMPILE_OPTIONS "$<$<CONFIG:Debug>:-fno-inline>"
)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else()
    message(STATUS "Doxygen not found. Documentation target 'doc' will be unavailable.")
endif()
