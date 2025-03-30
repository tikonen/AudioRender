cmake_minimum_required(VERSION 3.0.0)
project(AudioRenderAppCommon)

set(_target AudioRenderAppCommon)

file(GLOB_RECURSE _source_files
  "${CMAKE_CURRENT_SOURCE_DIR}/*.[c|h]pp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(${_target} STATIC ${_source_files})
target_compile_definitions(${_target} PRIVATE -D_UNICODE -DUNICODE)

target_include_directories(${_target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# required for winrt. Also /EHsc flag must be set to enable exceptions
set_target_properties(${_target} PROPERTIES CXX_STANDARD 17)

target_link_libraries(${_target} AudioRenderAPI)

target_link_libraries(${_target} ImGui)
target_link_libraries(${_target} stb) # Raster image loading
target_link_libraries(${_target} nanosvg) # Vector image loading

#
## WASAPI Audio
#target_link_libraries(${_target} "Mfplat.lib")
#target_link_libraries(${_target} "Avrt.lib")
#
## WinUSB
#target_link_libraries(${_target} "winusb.lib")
#target_link_libraries(${_target} "setupapi.lib")
#
#install(TARGETS ${_target} RUNTIME DESTINATION .)
