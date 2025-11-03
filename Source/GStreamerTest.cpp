// Improved GStreamer test: safer pipelines, more logging and helpful diagnostics
#include "ImageRec/onnx_classifier.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <vector>
#include <wiringPi.h>

// Build a safer libcamerasrc pipeline: avoid forcing a possibly-unsupported format
// and let videoconvert handle conversions. End with appsink with a name so
// OpenCV can negotiate caps.
std::string gst_pipeline_libcamera(int w, int h, int fps)
{
    std::ostringstream ss;
    ss << "libcamerasrc ! video/x-raw,width=" << w << ",height=" << h << ",framerate=" << fps
       << "/1 "
       << "! videoconvert ! video/x-raw,format=BGR ! appsink name=appsink emit-signals=false "
          "sync=false max-buffers=1 drop=true";
    return ss.str();
}

// v4l2 path: prefer to ask for a common raw format and convert downstream.
std::string gst_pipeline_v4l2(int w, int h, int fps)
{
    std::ostringstream ss;
    ss << "v4l2src device=/dev/video0 ! video/x-raw,width=" << w << ",height=" << h
       << ",framerate=" << fps << "/1 "
       << "! videoconvert ! video/x-raw,format=BGR ! appsink name=appsink emit-signals=false "
          "sync=false max-buffers=1 drop=true";
    return ss.str();
}

// Try to open a pipeline and print helpful diagnostics.
bool open_capture_with_pipeline(cv::VideoCapture &cap, const std::string &pipeline)
{
    std::cerr << "Trying pipeline: " << pipeline << std::endl;
    // Ask GStreamer to be a bit more verbose (helps when launching from the terminal)
    // Only set if not already set in the environment
    if (!std::getenv("GST_DEBUG"))
        setenv("GST_DEBUG", "3", 0);

    cap.open(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened())
    {
        std::cerr << "cv::VideoCapture failed to open pipeline.\n";
        return false;
    }

    // Try to query a few properties; if these fail the pipeline didn't negotiate caps
    double w = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double h = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = cap.get(cv::CAP_PROP_FPS);
    std::cerr << "Opened capture: width=" << w << " height=" << h << " fps=" << fps << std::endl;

    // If width/height/fps are not reported, the pipeline likely didn't negotiate caps.
    // Treat this as a failure so caller can try a different pipeline.
    if (w <= 0 || h <= 0 || fps <= 0)
    {
        std::cerr << "Capture opened but video properties invalid (caps negotiation likely "
                     "failed). Releasing and reporting failure.\n";
        cap.release();
        return false;
    }

    return true;
}

int main()
{
    int W = 640, H = 480, FPS = 30;
    cv::VideoCapture cap;

    // Initialize ONNX Classifier
    std::string model = "Assets/onnx/mobilenetv2-12-int8.onnx";
    std::string labels = "Assets/onnx/imagenet-simple-labels.json";
    int threads = 2;

    ONNXClassifier clf;
    if (!clf.Initialize(model, labels, threads))
    {
        std::cerr << "ONNXClassifier failed to initialize. Check model path and files. Exiting.\n";
        return 1;
    }

    // Try libcamera first (if available), then fall back to v4l2
    std::string pipeline = gst_pipeline_libcamera(W, H, FPS);
    if (!open_capture_with_pipeline(cap, pipeline))
    {
        std::cerr << "libcamerasrc pipeline failed, trying v4l2src...\n";
        pipeline = gst_pipeline_v4l2(W, H, FPS);
        if (!open_capture_with_pipeline(cap, pipeline))
        {
            std::cerr << "Failed to open capture pipeline. Make sure OpenCV is built with "
                         "GStreamer and the necessary gst plugins are installed.\n";
            std::cerr << "Try running the pipeline with gst-launch-1.0 to get more GStreamer-level "
                         "errors.\n";
            return 1;
        }
    }

    float predictionDelay = 1000.0f; // milliseconds between predictions
    float currentTime = static_cast<float>(cv::getTickCount()) / cv::getTickFrequency() * 1000.0f;
    float lastPredictionTime = 0.0f;

    cv::Mat frame;
    while (true)
    {
        if (!cap.read(frame) || frame.empty())
        {
            std::cerr << "Failed to read frame (cap.read returned false or empty frame).\n";
            break;
        }

        // // frame is BGR (OpenCV). Preprocess for ONNX model:
        // cv::Mat input;
        // cv::cvtColor(frame, input, cv::COLOR_BGR2RGB);
        // cv::resize(input, input, cv::Size(224,224)); // adapt to model input
        // input.convertTo(input, CV_32F, 1.0f/255.0f);

        currentTime = static_cast<float>(cv::getTickCount()) / cv::getTickFrequency() * 1000.0f;
        if (!(currentTime - lastPredictionTime < predictionDelay))
        {
            auto res = clf.classify(frame, 1);
            std::cout << "Top prediction:\n";
            if (!res.empty())
            {
                std::cout << "1: " << res[0].first << " (" << res[0].second << ")\n";
            }
            lastPredictionTime = currentTime;
        }

        // For debugging show the frame:
        cv::imshow("camera", frame);
        if (cv::waitKey(1) == 27)
            break; // ESC to exit
    }
    return 0;
}