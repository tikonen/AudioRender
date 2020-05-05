cmake_minimum_required(VERSION 3.0.0)
project(vendor)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# FreeGLUT
#set(FreeGLUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/freeglut")
#add_library(FreeGLUT SHARED IMPORTED GLOBAL)
#set_target_properties(FreeGLUT PROPERTIES 
#	INTERFACE_INCLUDE_DIRECTORIES "${FreeGLUT_ROOT}/include"
#	IMPORTED_LOCATION "${FreeGLUT_ROOT}/bin/x64/freeglut.dll"
#	IMPORTED_IMPLIB "${FreeGLUT_ROOT}/lib/x64/freeglut.lib")

add_subdirectory(stb)

# imgui
add_subdirectory("imgui")
#set_property(TARGET ImGui PROPERTY FOLDER "Vendor")

add_library(nanosvg INTERFACE)
target_include_directories(nanosvg INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/nanosvg/src>
)

# RapidJSON
add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rapidjson/include/>
)

# GLM
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glm/include>
)