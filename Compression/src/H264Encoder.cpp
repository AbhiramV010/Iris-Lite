#include "H264Encoder.hpp"
#include "logging.hpp"

#include <sstream>
#include <cstdio>
#include <opencv2/opencv.hpp>
#include <algorithm>

// ----------------------------------------------------
// CONSTRUCTOR / DESTRUCTOR
// ----------------------------------------------------

H264Encoder::H264Encoder(
    int w,
    int h,
    int fps_,
    bool hw)
    : width(w),
      height(h),
      fps(fps_),
      useHardware(hw)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

// ----------------------------------------------------
// BUILD FFMPEG COMMAND
// ----------------------------------------------------

std::string H264Encoder::buildCommand(
    const std::string& path,
    int crf)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y "
        << "-loglevel error "
        << "-f rawvideo "
        << "-pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " "
        << "-i - ";

    if (useHardware)
    {
        int bitrateKbps =
            2000 - (crf - 18) * 80;

        bitrateKbps =
            std::clamp(
                bitrateKbps,
                500,
                4000
            );

        cmd << "-c:v h264_v4l2m2m "
            << "-b:v "
            << bitrateKbps
            << "k ";
    }
    else
    {
        cmd << "-c:v libx264 "
            << "-preset veryfast "
            << "-crf "
            << crf
            << " ";
    }

    cmd << "-pix_fmt yuv420p "
        << path;

    return cmd.str();
}

// ----------------------------------------------------
// OPEN
// ----------------------------------------------------

bool H264Encoder::open(
    const std::string& path,
    int crf)
{
    close();

    std::string cmd =
        buildCommand(path, crf);

    ffmpegPipe =
        popen(cmd.c_str(), "w");

    if (!ffmpegPipe)
    {
        logError("FFmpeg failed to start");
        return false;
    }

    currentCRF = crf;
    pendingCRF = crf;

    logInfo("Encoder started: " + path);
    logInfo("FFmpeg CMD: " + cmd);

    return true;
}

// ----------------------------------------------------
// RUNTIME QUALITY CONTROL
// ----------------------------------------------------

void H264Encoder::setQuality(int crf)
{
    pendingCRF =
        std::clamp(crf, 18, 40);

    currentCRF = pendingCRF;
}

void H264Encoder::setRegionImportance(float value)
{
    regionImportance =
        std::clamp(value, 0.0f, 1.0f);
}

void H264Encoder::setFaceImportance(float value)
{
    faceImportance =
        std::clamp(value, 0.0f, 1.0f);
}

// ----------------------------------------------------
// WRITE FRAME
// ----------------------------------------------------

bool H264Encoder::writeFrame(
    const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    cv::Mat resized;

    // ensure encoder size
    if (frame.cols != width ||
        frame.rows != height)
    {
        cv::resize(
            frame,
            resized,
            cv::Size(width, height)
        );
    }
    else
    {
        resized = frame;
    }

    // ensure contiguous memory
    if (!resized.isContinuous())
        resized = resized.clone();

    // ----------------------------------------------------
    // PERCEPTUAL ENHANCEMENT
    // ----------------------------------------------------

    if (regionImportance > 0.55f ||
        faceImportance > 0.45f)
    {
        cv::Mat blurred;

        cv::GaussianBlur(
            resized,
            blurred,
            cv::Size(0, 0),
            2.0
        );

        cv::addWeighted(
            resized,
            1.5,
            blurred,
            -0.5,
            0,
            resized
        );
    }

    // ----------------------------------------------------
    // WRITE RAW FRAME
    // ----------------------------------------------------

    const size_t expectedBytes =
        width * height * 3;

    size_t written =
        fwrite(
            resized.data,
            1,
            expectedBytes,
            ffmpegPipe
        );

    if (written != expectedBytes)
    {
        logError(
            "FFmpeg write mismatch: " +
            std::to_string(written) +
            "/" +
            std::to_string(expectedBytes)
        );

        return false;
    }

    fflush(ffmpegPipe);

    return true;
}

// ----------------------------------------------------
// CLOSE
// ----------------------------------------------------

void H264Encoder::close()
{
    if (ffmpegPipe)
    {
        fflush(ffmpegPipe);

        pclose(ffmpegPipe);

        ffmpegPipe = nullptr;

        logInfo(
            "Encoder closed (CRF=" +
            std::to_string(currentCRF) +
            ")"
        );
    }
}