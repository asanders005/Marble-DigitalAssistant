#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class GStreamer
{
public:
    enum class CaptureBackend
    {
        LIBCAMERA,
        V4L2
    };

public:
    bool openCapture(CaptureBackend backend, int w, int h, int fps);
    cv::Mat captureFrame();

private:
    std::string gst_pipeline_libcamera(int w, int h, int fps);
    std::string gst_pipeline_v4l2(int w, int h, int fps);
    
    bool open_capture_with_pipeline(const std::string &pipeline);

private:
    cv::VideoCapture cap;
};