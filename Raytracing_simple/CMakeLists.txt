cmake_minimum_required(VERSION 3.10)
project(Raytracer)

set(CMAKE_CXX_STANDARD 17)

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

target_compile_options(raytracer PRIVATE
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-O2>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    $<$<CONFIG:Debug>:-fno-inline-functions>
    $<$<CONFIG:Debug>:-fno-inline>
)

set_source_files_properties(src/AxisAlignedRect.cpp PROPERTIES
    COMPILE_OPTIONS "$<$<CONFIG:Debug>:-fno-inline>"
)
