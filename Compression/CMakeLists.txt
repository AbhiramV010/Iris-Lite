cmake_minimum_required(VERSION 3.16)
project(PerceptualCompressor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# OpenCV
# ---------------------------------------------------------
# This MUST point to the folder containing OpenCVConfig.cmake
set(OpenCV_DIR "C:/dev/opencv-install")

# Ensure both Debug and Release configs exist
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

# ---------------------------------------------------------
# Project include directories
# ---------------------------------------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external
)

# ---------------------------------------------------------
# Source files
# ---------------------------------------------------------
file(GLOB SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

add_executable(PerceptualCompressor ${SOURCES})

# ---------------------------------------------------------
# Link OpenCV
# ---------------------------------------------------------
target_link_libraries(PerceptualCompressor PRIVATE
    ${OpenCV_LIBS}
)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
