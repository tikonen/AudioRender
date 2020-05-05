cmake_minimum_required(VERSION 3.0.0)
project(ImGui)

set(_src_root_path "${PROJECT_SOURCE_DIR}")

file(
    GLOB_RECURSE _source_list
    LIST_DIRECTORIES false
    "${_src_root_path}/src/*.[ch]*"
)

file(
    GLOB_RECURSE _public_headers_list
    LIST_DIRECTORIES false
    "${_src_root_path}/inc/*.h*"
)

set(_target "ImGui")

add_library(${_target} STATIC ${_source_list} ${_public_headers_list})

source_group("Public Headers" FILES ${_public_headers_list})

target_include_directories(${_target} PUBLIC ${_src_root_path}/inc)

set_property(TARGET ${_target} PROPERTY FOLDER "Vendor")

