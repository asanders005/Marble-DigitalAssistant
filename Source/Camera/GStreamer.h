#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>

class GStreamer
{
public:
    enum class CaptureBackend
    {
        LIBCAMERA,
        V4L2
    };

public:
    GStreamer() { cap = std::make_unique<cv::VideoCapture>(); };

    bool openCapture(CaptureBackend backend, int w, int h, int fps);
    cv::Mat captureFrame();
    void writeFrame(const cv::Mat& frame);

    bool startRecording(const std::string& filename);
    bool isRecording() const {
        return (writer && writer->isOpened());
    }
    void stopRecording();

private:
    std::string gst_pipeline_libcamera();
    std::string gst_pipeline_v4l2();
    std::string get_pipeline_encoderMp4();

    bool open_capture_with_pipeline(const std::string &pipeline);

private:
    std::unique_ptr<cv::VideoCapture> cap;
    std::unique_ptr<cv::VideoWriter> writer;

    int w, h, fps;
    std::string recordingFilename;
};