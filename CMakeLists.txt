cmake_minimum_required(VERSION 3.16)
project(video_compressor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Basic warnings so we catch mistakes early
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# OpenCV is the only real dependency here
find_package(OpenCV REQUIRED)

include_directories(include)
include_directories(${OpenCV_INCLUDE_DIRS})

# Grab everything in src/
file(GLOB SOURCES src/*.cpp)

add_executable(video_compressor ${SOURCES})

target_link_libraries(video_compressor ${OpenCV_LIBS})
