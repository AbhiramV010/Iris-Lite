cmake_minimum_required(VERSION 3.16)
project(PerceptualCompressor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OpenCV_DIR "C:/dev/opencv/build/x64/vc16/lib")
find_package(OpenCV REQUIRED)

add_executable(PerceptualCompressor
    src/main.cpp
    src/CompressionEngine.cpp

    
    src/Utils.cpp
    src/logging.cpp
    src/Config.cpp
    src/H264encoder.cpp

    
    
    src/ImportanceEngine.cpp
    src/CompressionPolicy.cpp
    
    
  "src/FrameWindowBuffer.cpp" "src/EventAligner.cpp")

target_include_directories(PerceptualCompressor PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(PerceptualCompressor PRIVATE
    ${OpenCV_LIBS}
)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "Build system: ${CMAKE_SYSTEM_NAME}")