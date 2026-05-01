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

# ---------------- SAFE OPTIMIZATION FLAGS ----------------
# Let compiler auto-detect Pi CPU instead of forcing broken flags
set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -mcpu=cortex-a72 -mtune=cortex-a72"
)

set(CMAKE_CXX_FLAGS_DEBUG
    "-O0 -g -Wall -Wextra"
)

# NOTES:
# - Removed: -mfpu=neon-fp-armv8 (causes your error)
# - Removed: -mfloat-abi=hard (handled by OS/toolchain)
# - Kept: strong optimization without breaking compatibility
# - No -ffast-math (good call — keeps scoring stable)

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
    src/SystemGovernor.cpp
    src/StorageManager.cpp
    src/EventCluster.cpp
    src/ImportanceMemory.cpp
    src/Dashboard.cpp
    src/CompressionOrchestrator.cpp
)

# ---------------- EXECUTABLE ----------------
add_executable(PerceptualCompressor ${SOURCES})

# ---------------- LINK LIBRARIES ----------------
target_link_libraries(PerceptualCompressor
    ${OpenCV_LIBS}
    Threads::Threads
    rt
)

# ---------------- STRIP BINARIES (RELEASE ONLY) ----------------
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(PerceptualCompressor PRIVATE -s)
endif()

# ---------------- STATUS ----------------
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
