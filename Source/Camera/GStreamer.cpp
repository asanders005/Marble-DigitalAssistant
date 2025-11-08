#include "GStreamer.h"
#include "ImageRec/onnx_classifier.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
#include <wiringPi.h>

bool GStreamer::openCapture(CaptureBackend backend, int w, int h, int fps)
{
    std::string pipeline;
    if (backend == CaptureBackend::LIBCAMERA)
    {
        pipeline = gst_pipeline_libcamera(w, h, fps);
    }
    else // V4L2
    {
        pipeline = gst_pipeline_v4l2(w, h, fps);
    }

    return open_capture_with_pipeline(pipeline);
}

cv::Mat GStreamer::captureFrame()
{
    cv::Mat frame;
    if (!cap->read(frame) || frame.empty())
    {
        throw std::runtime_error("Failed to read frame from GStreamer capture.");
    }
    return frame;
}

// Build a safer libcamerasrc pipeline: avoid forcing a possibly-unsupported format
// and let videoconvert handle conversions. End with appsink with a name so
// OpenCV can negotiate caps.
std::string GStreamer::gst_pipeline_libcamera(int w, int h, int fps)
{
    std::ostringstream ss;
    ss << "libcamerasrc ! video/x-raw,width=" << w << ",height=" << h << ",framerate=" << fps
       << "/1 "
       << "! videoconvert ! video/x-raw,format=BGR ! appsink name=appsink emit-signals=false "
          "sync=false max-buffers=1 drop=true";
    return ss.str();
}

// v4l2 path: prefer to ask for a common raw format and convert downstream.
std::string GStreamer::gst_pipeline_v4l2(int w, int h, int fps)
{
    std::ostringstream ss;
    ss << "v4l2src device=/dev/video0 ! video/x-raw,width=" << w << ",height=" << h
       << ",framerate=" << fps << "/1 "
       << "! videoconvert ! video/x-raw,format=BGR ! appsink name=appsink emit-signals=false "
          "sync=false max-buffers=1 drop=true";
    return ss.str();
}

// Try to open a pipeline and print helpful diagnostics.
bool GStreamer::open_capture_with_pipeline(const std::string &pipeline)
{
    //std::cerr << "Trying pipeline: " << pipeline << std::endl;
    // Ask GStreamer to be a bit more verbose (helps when launching from the terminal)
    // Only set if not already set in the environment
    if (!std::getenv("GST_DEBUG"))
        setenv("GST_DEBUG", "3", 0);

    cap->open(pipeline, cv::CAP_GSTREAMER);
    if (!cap->isOpened())
    {
        std::cerr << "cv::VideoCapture failed to open pipeline.\n";
        return false;
    }

    // Try to query a few properties; if these fail the pipeline didn't negotiate caps
    double w = cap->get(cv::CAP_PROP_FRAME_WIDTH);
    double h = cap->get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = cap->get(cv::CAP_PROP_FPS);
    //std::cerr << "Opened capture: width=" << w << " height=" << h << " fps=" << fps << std::endl;

    // If width/height/fps are not reported, the pipeline likely didn't negotiate caps.
    // Treat this as a failure so caller can try a different pipeline.
    if (w <= 0 || h <= 0 || fps <= 0)
    {
        std::cerr << "Capture opened but video properties invalid (caps negotiation likely "
                     "failed). Releasing and reporting failure.\n";
        cap->release();
        return false;
    }

    return true;
}