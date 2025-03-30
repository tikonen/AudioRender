cmake_minimum_required(VERSION 3.0.0)
project(AudioRender)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)


# force binaries from all configurations to a single directory
set(_build_output_dir ${CMAKE_BINARY_DIR}/bin)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${_build_output_dir} )
    #set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${_build_output_dir} )
    #set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${_build_output_dir} )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set(_src_root_path "${PROJECT_SOURCE_DIR}")

###########################################################################
# Common library

add_subdirectory(vendor)

###########################################################################
# Executables

add_subdirectory(AudioRenderAPI)
add_subdirectory(AudioRenderAppCommon)
add_subdirectory(AudioRenderAPITest)
add_subdirectory(AudioRenderCAPI)
add_subdirectory(LunarLander)
