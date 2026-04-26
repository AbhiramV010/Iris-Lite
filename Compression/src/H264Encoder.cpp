#include "H264Encoder.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

#include <sstream>
#include <sys/stat.h>
#include <cstdlib>

H264Encoder::H264Encoder(int w, int h, int fps, bool hw)
    : width(w), height(h), fps(fps), useHardware(hw)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

// ---------------- OPEN ----------------
bool H264Encoder::open(const std::string& path, int crf)
{
    currentCRF = crf;

    system("mkdir -p /mnt/clipDrive/clips");
    system("rm -f /tmp/iris_audio.pcm && mkfifo /tmp/iris_audio.pcm");

    audioPipe = popen("cat > /tmp/iris_audio.pcm", "w");

    ffmpegCommand = buildCommand(path, crf);
    ffmpegPipe = popen(ffmpegCommand.c_str(), "w");

    if (!ffmpegPipe)
    {
        logError("FFmpeg pipe failed to open");
        return false;
    }

    return true;
}

// ---------------- WRITE FRAME ----------------
bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    cv::Mat bgr;
    cv::resize(frame, bgr, cv::Size(width, height));

    if (bgr.channels() != 3)
        cv::cvtColor(bgr, bgr, cv::COLOR_GRAY2BGR);

    size_t bytes = bgr.total() * bgr.elemSize();

    size_t written = fwrite(bgr.data, 1, bytes, ffmpegPipe);

    if (written != bytes)
    {
        logError("FFmpeg write failed — closing encoder");
        close();
        return false;
    }

    return true;
}

// ---------------- WRITE AUDIO ----------------
bool H264Encoder::writeAudio(const std::vector<uint8_t>& pcm)
{
    if (!audioPipe || pcm.empty())
        return false;

    size_t written = fwrite(pcm.data(), 1, pcm.size(), audioPipe);
    return written == pcm.size();
}

// ---------------- CLOSE ----------------
void H264Encoder::close()
{
    if (audioPipe)
    {
        pclose(audioPipe);
        audioPipe = nullptr;
    }

    if (ffmpegPipe)
    {
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;
    }
}

// ---------------- COMMAND BUILDER ----------------
std::string H264Encoder::buildCommand(const std::string& outputPath, int crf)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y "
        << "-f s16le -ar " << AUDIO_RATE << " -ac 1 -i /tmp/iris_audio.pcm "
        << "-f rawvideo -pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " "
        << "-i - ";

#if defined(_WIN32)
    cmd << "-c:v libx264 -preset ultrafast -crf " << crf;
#else
    if (useHardware)
        cmd << "-c:v h264_v4l2m2m -b:v 2M ";
    else
        cmd << "-c:v libx264 -preset ultrafast -crf " << crf;
#endif

    cmd << " -c:a aac -b:a 128k "
        << "-vsync vfr -pix_fmt yuv420p \"" << outputPath << "\"";

    return cmd.str();
}
