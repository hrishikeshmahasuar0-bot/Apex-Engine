cmake_minimum_required(VERSION 3.10)
project(GraphicsEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(OpenGL REQUIRED)

# GLFW setup
set(GLFW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(GLFW_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/lib")
set(GLFW_LIBRARIES glfw3 opengl32 gdi32 user32 kernel32 shell32)

# GLAD is included directly in src/

# Add executable
add_executable(GraphicsEngine
    src/main.cpp
    src/glad.c
)

# Include directories
target_include_directories(GraphicsEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(GraphicsEngine
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
)

# Copy DLLs to output directory
add_custom_command(TARGET GraphicsEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/glfw3.dll"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
)

# Copy shaders to output directory
add_custom_command(TARGET GraphicsEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders"
)

# Copy assets to output directory
add_custom_command(TARGET GraphicsEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
)

# Print build information
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")