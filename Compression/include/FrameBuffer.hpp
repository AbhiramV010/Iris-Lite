#pragma once

#include <opencv2/core.hpp>
#include <vector>
#include <mutex>
#include <cstdint>

struct BufferedFrame
{
    cv::Mat frame;
    uint64_t frameIndex;
    double timestamp;
    float perceptualScore;
};

class FrameBuffer
{
public:
    FrameBuffer(int maxFrames, int frameWidth, int frameHeight);
    ~FrameBuffer();

    void pushFrame(const cv::Mat& frame, uint64_t index);

    std::vector<BufferedFrame> getFrameRange(uint64_t startIndex, uint64_t endIndex);

    BufferedFrame getLatestFrame();

    uint64_t getCurrentIndex() const;

    int getNumBufferedFrames() const;

    void clear();

private:
    std::vector<BufferedFrame> buffer;
    int maxFrames;
    int writePos;
    uint64_t frameCounter;
    std::mutex bufferMutex;
};