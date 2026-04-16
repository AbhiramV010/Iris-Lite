#include "FrameBuffer.hpp"
#include "logging.hpp"

FrameBuffer::FrameBuffer(int maxFrames_, int frameWidth, int frameHeight)
    : maxFrames(maxFrames_), writePos(0), frameCounter(0)
{
    buffer.resize(maxFrames);
    for (int i = 0; i < maxFrames; i++)
    {
        buffer[i].frame.create(frameHeight, frameWidth, CV_8UC3);
        buffer[i].perceptualScore = 0.0f;
    }
    logInfo("FrameBuffer initialized: " + std::to_string(maxFrames) + " frames capacity");
}

FrameBuffer::~FrameBuffer()
{
    clear();
}

void FrameBuffer::pushFrame(const cv::Mat& frame, uint64_t index)
{
    std::lock_guard<std::mutex> lock(bufferMutex);

    if (frame.empty() || frame.size() != buffer[0].frame.size())
        return;

    frame.copyTo(buffer[writePos].frame);
    buffer[writePos].frameIndex = index;
    buffer[writePos].timestamp = static_cast<double>(index) / 30.0;
    buffer[writePos].perceptualScore = 0.0f;

    writePos = (writePos + 1) % maxFrames;
    frameCounter++;
}

std::vector<BufferedFrame> FrameBuffer::getFrameRange(uint64_t startIndex, uint64_t endIndex)
{
    std::lock_guard<std::mutex> lock(bufferMutex);
    std::vector<BufferedFrame> result;

    if (startIndex > endIndex || frameCounter == 0)
        return result;

    uint64_t oldestIndex = (frameCounter > maxFrames) ? (frameCounter - maxFrames) : 0;
    uint64_t newestIndex = frameCounter - 1;

    if (endIndex < oldestIndex || startIndex > newestIndex)
        return result;

    uint64_t actualStart = (startIndex < oldestIndex) ? oldestIndex : startIndex;
    uint64_t actualEnd = (endIndex > newestIndex) ? newestIndex : endIndex;

    for (uint64_t idx = actualStart; idx <= actualEnd; idx++)
    {
        int bufPos = idx % maxFrames;
        if (buffer[bufPos].frameIndex == idx)
            result.push_back(buffer[bufPos]);
    }

    return result;
}

BufferedFrame FrameBuffer::getLatestFrame()
{
    std::lock_guard<std::mutex> lock(bufferMutex);

    if (frameCounter == 0)
        return BufferedFrame();

    int pos = (writePos - 1 + maxFrames) % maxFrames;
    return buffer[pos];
}

uint64_t FrameBuffer::getCurrentIndex() const
{
    return frameCounter;
}

int FrameBuffer::getNumBufferedFrames() const
{
    return (frameCounter < maxFrames) ? frameCounter : maxFrames;
}

void FrameBuffer::clear()
{
    std::lock_guard<std::mutex> lock(bufferMutex);
    buffer.clear();
    writePos = 0;
    frameCounter = 0;
}