cmake_minimum_required(VERSION 3.0.0)
project(stb)

set(_sources
    stb_image.h
    stb_image.cpp
    stb_image_resize.h
    stb_image_resize.cpp
    stb_image_write.h
    stb_image_write.cpp
)

set(_target "stb")

add_library(${_target} STATIC ${_sources})
target_include_directories(${_target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${_target} PUBLIC 
	STBI_MSC_SECURE_CRT
	STBI_FAILURE_USERMSG)

set_property(TARGET ${_target} PROPERTY FOLDER "Vendor")
