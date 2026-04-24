cmake_minimum_required(VERSION 3.16)

project(PerceptualCompressor LANGUAGES CXX)

# ---------------- C++ STANDARD ----------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------- DEFAULT BUILD TYPE ----------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------- PI 4B OPTIMIZED FLAGS ----------------
set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard"
)

set(CMAKE_CXX_FLAGS_DEBUG
    "-O0 -g -Wall -Wextra"
)

# NOTE:
# - NO -ffast-math (keeps perceptual scoring stable)
# - explicit Cortex-A72 tuning (Pi 4 CPU)
# - NEON enabled explicitly for OpenCV ops

# ---------------- THREAD SUPPORT ----------------
find_package(Threads REQUIRED)

# ---------------- OPENCV ----------------
find_package(OpenCV REQUIRED)

# ---------------- INCLUDE PATHS ----------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/config/external
    ${OpenCV_INCLUDE_DIRS}
)

# ---------------- SOURCE FILES ----------------
set(SOURCES
    src/main.cpp
    src/CompressionEngine.cpp
    src/CompressionPolicy.cpp
    src/Config.cpp
    src/H264Encoder.cpp
    src/ImportanceEngine.cpp
    src/SharedFrameBuffer.cpp
    src/logging.cpp
)

# ---------------- EXECUTABLE ----------------
add_executable(PerceptualCompressor ${SOURCES})

# ---------------- LINK LIBRARIES ----------------
target_link_libraries(PerceptualCompressor
    ${OpenCV_LIBS}
    Threads::Threads
    rt
)

# ---------------- OPTIONAL: STRIP IN RELEASE ----------------
set_target_properties(PerceptualCompressor PROPERTIES
    LINK_FLAGS_RELEASE "-s"
)

# ---------------- STATUS ----------------
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")