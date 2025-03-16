cmake_minimum_required(VERSION 3.0.0)
project(AudioRenderAPI)

set(_target AudioRenderAPI)

include(../cmake/vs_precompiled.cmake)

file(GLOB_RECURSE _source_files
  "${CMAKE_CURRENT_SOURCE_DIR}/*.[c|h]pp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_msvc_precompiled_header(pch.h "${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp" _source_files)

add_library(${_target} STATIC ${_source_files})
target_compile_definitions(${_target} PRIVATE -D_UNICODE -DUNICODE)

target_include_directories(${_target} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../DACIntegratorDriver/Common)

# required for winrt. Also /EHsc flag must be set to enable exceptions
set_target_properties(${_target} PROPERTIES CXX_STANDARD 17)

#target_link_libraries(${_target} rapidjson)
target_link_libraries(${_target} stb) # Raster image loading
target_link_libraries(${_target} nanosvg) # Vector image loading

# WASAPI Audio
target_link_libraries(${_target} "Mfplat.lib")
target_link_libraries(${_target} "windowsapp.lib")

# WinUSB
target_link_libraries(${_target} "winusb.lib")
target_link_libraries(${_target} "setupapi.lib")

install(TARGETS ${_target} RUNTIME DESTINATION .)
