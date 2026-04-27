#include "CompressionEngine.hpp"
#include "logging.hpp"

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

bool CompressionEngine::initialize(const Config& cfg)
{
    config = cfg;

    policy = std::make_unique<CompressionPolicy>();

    encoder = std::make_unique<H264Encoder>(
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.fps,
        true
    );

    buffer = std::make_unique<SharedFrameBuffer>();

    if (!buffer->initialize())
        return false;

    faceCascade.load("haarcascade_frontalface_default.xml");

    return true;
}

float CompressionEngine::computeRegionImportance(const cv::Mat& frame)
{
    int w = frame.cols;
    int h = frame.rows;

    cv::Rect center(w * 0.25, h * 0.25, w * 0.5, h * 0.5);
    cv::Mat roi = frame(center);

    cv::Mat gray;
    cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);

    return cv::mean(gray)[0] / 255.0f;
}

bool CompressionEngine::detectFace(const cv::Mat& frame)
{
    std::vector<cv::Rect> faces;
    cv::Mat gray;

    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    faceCascade.detectMultiScale(gray, faces, 1.1, 3);

    return !faces.empty();
}

void CompressionEngine::processEvent(const EventWindow& event)
{
    std::string path =
        "/mnt/clipDrive/clips/" +
        std::to_string(event.startFrame) + ".mp4";

    cv::Mat prev;
    bool opened = false;

    for (uint64_t i = event.startFrame; i < event.endFrame; i++)
    {
        std::vector<uint8_t> jpeg;

        if (!buffer->getFrame(i, jpeg))
            continue;

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        if (frame.empty())
            continue;

        // ---------------- MOTION ----------------
        float motion = 0.0f;

        if (!prev.empty())
        {
            cv::Mat diff;
            cv::absdiff(frame, prev, diff);
            motion = cv::mean(diff)[0] / 255.0f;
        }

        // smooth motion (stability fix)
        motion = 0.85f * lastMotion + 0.15f * motion;
        lastMotion = motion;

        // ---------------- SPATIAL ----------------
        cv::Mat gray, edges;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 50, 150);

        float spatial =
            (float)cv::countNonZero(edges) /
            (frame.rows * frame.cols);

        // ---------------- FACE ----------------
        float face = detectFace(frame) ? 1.0f : 0.0f;

        // ---------------- REGION ----------------
        float region = computeRegionImportance(frame);

        // ---------------- SCORE ----------------
        float score = policy->perceptualScore(
            policy->motion(motion),
            policy->spatial(spatial),
            policy->face(face > 0.5f),
            policy->region(region)
        );

        // temporal smoothing (critical stability layer)
        score = 0.85f * lastScore + 0.15f * score;
        lastScore = score;

        int crf = policy->computeCRF(score, config.crf);
        int fps = policy->computeFPS(score, config.fps);

        if (!opened)
        {
            encoder->open(path, crf);
            opened = true;
        }

        if (encoder->isOpen())
            encoder->writeFrame(frame);

        prev = frame;
    }

    if (encoder->isOpen())
        encoder->close();
}