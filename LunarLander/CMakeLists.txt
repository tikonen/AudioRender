cmake_minimum_required(VERSION 3.0.0)
project(LunarLander)

set(_target LunarLander)

include(../cmake/vs_precompiled.cmake)

file(GLOB_RECURSE _source_files
  "${CMAKE_CURRENT_SOURCE_DIR}/*.[c|h]pp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(${_target} ${_source_files})
target_compile_definitions(${_target} PRIVATE -D_UNICODE -DUNICODE)

# required for winrt. Also /EHsc flag must be set to enable exceptions
set_target_properties(${_target} PROPERTIES
        CXX_STANDARD 17)

target_link_libraries(${_target} AudioRenderAPI)

#target_link_libraries(${_target} rapidjson)
target_link_libraries(${_target} glm) # Math library
target_link_libraries(${_target} stb) # Raster image loading
target_link_libraries(${_target} nanosvg) # Vector image loading

# WASAPI Audio
target_link_libraries(${_target} "Mfplat.lib")
#target_link_libraries(${_target} "Dsound.lib")
#target_link_libraries(${_target} "dxguid.lib")
target_link_libraries(${_target} "windowsapp.lib")

# Visualization support
target_link_libraries(${_target} ImGui) 
target_link_libraries(${_target} "D3D10.lib")

# Set working directory to target exe directory, app looks files from its current directory
set_property(TARGET ${_target} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${_target}>")

set(_runtime_path "${CMAKE_SOURCE_DIR}/runtime")

# Copy changed files from runtime to the binary folder after
# successful build
add_custom_command(TARGET ${_target}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -Dsrc_dir="${_runtime_path}"
    -Ddst_dir="$<TARGET_FILE_DIR:${_target}>"
    -P "${CMAKE_SOURCE_DIR}/cmake/update_directory.cmake"
)

install(TARGETS ${_target} RUNTIME DESTINATION .)

