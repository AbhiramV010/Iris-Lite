cmake_minimum_required(VERSION 3.16)

project(PerceptualCompressor LANGUAGES CXX)

# ---------------- C++ STANDARD ----------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------- PERFORMANCE FLAGS (Pi-optimized) ----------------
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math")

# ---------------- FIND OPENV ----------------
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
    src/H264encoder.cpp
    src/ImportanceEngine.cpp
    src/SharedFrameBuffer.cpp
    src/logging.cpp
)

# ---------------- EXECUTABLE ----------------
add_executable(PerceptualCompressor ${SOURCES})

# ---------------- LINK LIBRARIES ----------------
target_link_libraries(PerceptualCompressor
    ${OpenCV_LIBS}
    pthread
    rt
)

# ---------------- STATUS OUTPUT ----------------
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")