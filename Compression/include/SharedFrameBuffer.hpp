#pragma once

#include <cstdint>
#include <string>

constexpr int SLOT_SIZE = 200000;
constexpr int BUFFER_SIZE = 7200;

constexpr const char* FRAME_DATA_NAME = "iris_frame_buffer_data";
constexpr const char* FRAME_META_NAME = "iris_frame_metadata";

struct FrameMetadata
{
    uint64_t frameId;
    uint32_t size;
    uint32_t checksum;

    uint8_t committed;
    uint8_t reserved[7];
};

class SharedFrameBuffer
{
public:

    bool initialize();
    bool isValid() const;

    bool readFrame(
        uint64_t absoluteFrameId,
        std::vector<uint8_t>& outData
    );

    uint64_t latestFrameId() const;

private:

    bool validateJPEG(
        const std::vector<uint8_t>& data
    ) const;

    uint32_t crc32(
        const uint8_t* data,
        size_t len
    ) const;

private:

    int dataFd = -1;
    int metaFd = -1;

    uint8_t* frameData = nullptr;
    FrameMetadata* metadata = nullptr;
};