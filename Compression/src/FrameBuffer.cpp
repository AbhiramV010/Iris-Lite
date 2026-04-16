#include "FrameBuffer.hpp"
#include "logging.hpp"
#include <algorithm>

FrameBuffer::~FrameBuffer()
{
    clear();
}
FrameBuffer::FrameBuffer(int maxFrames_, int frameWidth, int frameHeight)
    : maxFrames(maxFrames_), writePos(0), frameCounter(0)
{
    buffer.resize(maxFrames);

    for (auto& f : buffer)
    {
        f.frame.create(frameHeight, frameWidth, CV_8UC3);
        f.frame.setTo(cv::Scalar(0, 0, 0));
        f.perceptualScore = 0.0f;
        f.frameIndex = 0;
    }

    logInfo("FrameBuffer initialized (SAFE MODE): " + std::to_string(maxFrames));
}

void FrameBuffer::pushFrame(const cv::Mat& frame, uint64_t index)
{
    std::lock_guard<std::mutex> lock(bufferMutex);

    if (frame.empty())
        return;

    BufferedFrame& slot = buffer[writePos];

    frame.copyTo(slot.frame);
    slot.frameIndex = frameCounter;   // ALWAYS internal monotonic index
    slot.timestamp = frameCounter / 30.0;
    slot.perceptualScore = 0.0f;

    writePos = (writePos + 1) % maxFrames;
    frameCounter++;
}

std::vector<BufferedFrame> FrameBuffer::getFrameRange(uint64_t startIndex, uint64_t endIndex)
{
    std::lock_guard<std::mutex> lock(bufferMutex);

    std::vector<BufferedFrame> out;
    if (frameCounter == 0 || startIndex > endIndex)
        return out;

    uint64_t oldest = (frameCounter > maxFrames) ? frameCounter - maxFrames : 0;
    uint64_t newest = frameCounter - 1;

    startIndex = std::max(startIndex, oldest);
    endIndex = std::min(endIndex, newest);

    for (uint64_t i = startIndex; i <= endIndex; i++)
    {
        int pos = i % maxFrames;

        if (buffer[pos].frame.empty())
            continue;

        // IMPORTANT: trust position, not stored ID
        out.push_back(buffer[pos]);
    }

    return out;
}

BufferedFrame FrameBuffer::getLatestFrame()
{
    std::lock_guard<std::mutex> lock(bufferMutex);

    if (frameCounter == 0)
        return BufferedFrame();

    int pos = (writePos + maxFrames - 1) % maxFrames;
    return buffer[pos];
}

uint64_t FrameBuffer::getCurrentIndex() const
{
    return frameCounter;
}

void FrameBuffer::clear()
{
    std::lock_guard<std::mutex> lock(bufferMutex);

    for (auto& f : buffer)
        f.frame.release();

    writePos = 0;
    frameCounter = 0;
}