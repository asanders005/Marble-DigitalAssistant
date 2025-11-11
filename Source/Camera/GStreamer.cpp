#include "GStreamer.h"
#include "ImageRec/onnx_classifier.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <opencv2/videoio.hpp>
#include <sstream>
#include <vector>
#include <wiringPi.h>
#include <ctime>
#include <chrono>

// For recording
static const int DEFAULT_BITRATE_KBPS = 2000;

bool GStreamer::openCapture(CaptureBackend backend, int w, int h, int fps)
{
    std::string pipeline;

    this->w = w;
    this->h = h;
    this->fps = fps;

    if (backend == CaptureBackend::LIBCAMERA)
    {
        pipeline = gst_pipeline_libcamera();
    }
    else // V4L2
    {
        pipeline = gst_pipeline_v4l2();
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
    writeFrame(frame);

    return frame;
}

void GStreamer::writeFrame(const cv::Mat &frame)
{
    if (!isRecording() || frame.empty())
        return;

    if (frame.size() != cv::Size(w, h))
    {
        cv::Mat resized;
        cv::resize(frame, resized, cv::Size(w, h));
        recorder->pushFrame(resized);
        //writer->write(resized);
    }
    else
        recorder->pushFrame(frame);
        //writer->write(frame);
}

bool GStreamer::startRecording(const std::string &filename, int bitrate_kbps)
{
    if (isRecording())
    {
        std::cerr << "[GStreamer Warning] Recording already in progress.\n";
        return false;
    }
    this->bitrate_kbps = bitrate_kbps;
    recordingFilename = "build/Assets/Video/" + filename;

    // Determine effective fps for the writer. Prefer camera-negotiated fps if available,
    // otherwise fall back to the requested fps. If camera reports 0, sample a few frames
    // to estimate the real capture framerate.
    double measured_fps = measureCaptureFps(8);
    double effective_fps = (measured_fps > 0.5 && measured_fps < 120.0) ? measured_fps : static_cast<double>(fps);
    fps = static_cast<int>(effective_fps);

    if (!recorder->start(recordingFilename, w, h, effective_fps, bitrate_kbps, 30))
    {
        std::cerr << "[GStreamer Error] Failed to start recorder.\n";
        return false;
    }

    return true;

    // Use CAP_GSTREAMER backend
    //std::string pipeline = get_pipeline_encoderMp4();
    
    //writer = std::make_unique<cv::VideoWriter>(pipeline, cv::CAP_GSTREAMER, 0, effective_fps, cv::Size(w, h), true);
    
    // if (!writer->isOpened())
    // {
    //     std::cerr << "GStreamer VideoWriter failed to open pipeline: " << pipeline << std::endl;
    //     writer.reset();
    //     return false;
    // }
}

bool GStreamer::startRecordingDateTime(int bitrate_kbps, const std::string &filenamePrefix)
{
    // Get current date/time for filename
    auto t = std::time(0);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    if (!filenamePrefix.empty()) oss << filenamePrefix << "_";
    oss << std::put_time(&tm, "%Y-%m-%d_%H:%M:%S") << ".mp4";
    return startRecording(oss.str(), bitrate_kbps);
}

void GStreamer::stopRecording()
{
    if (isRecording())
    {
        recorder->stop();
        // writer->release();
        // writer.reset();
    }
}

double GStreamer::measureCaptureFps(int samples)
{
    if (!cap || !cap->isOpened() || samples <= 0)
        return 0.0;
    cv::Mat tmp;
    int framesRetrieved = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < samples; ++i)
    {
        if (!cap->read(tmp) || tmp.empty())
            break;
        ++framesRetrieved;
    }
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
    return (framesRetrieved > 1 && secs > 0.0) ? static_cast<double>(framesRetrieved) / secs : 0.0;
}

// Build a safer libcamerasrc pipeline: avoid forcing a possibly-unsupported format
// and let videoconvert handle conversions. End with appsink with a name so
// OpenCV can negotiate caps.
std::string GStreamer::gst_pipeline_libcamera()
{
    std::ostringstream ss;
    ss << "libcamerasrc ! video/x-raw,width=" << w << ",height=" << h << ",framerate=" << fps
       << "/1 "
       << "! videoconvert ! video/x-raw,format=BGR ! appsink name=appsink emit-signals=false "
          "sync=false max-buffers=1 drop=true";
    return ss.str();
}

// v4l2 path: prefer to ask for a common raw format and convert downstream.
std::string GStreamer::gst_pipeline_v4l2()
{
    std::ostringstream ss;
    ss << "v4l2src device=/dev/video0 ! video/x-raw,width=" << w << ",height=" << h
       << ",framerate=" << fps << "/1 "
       << "! videoconvert ! video/x-raw,format=BGR ! appsink name=appsink emit-signals=false "
          "sync=false max-buffers=1 drop=true";
    return ss.str();
}

std::string GStreamer::get_pipeline_encoderMp4()
{
    std::ostringstream ss;
    ss << "appsrc name=appsrc0 is-live=true do-timestamp=true format=time block=true ! "
       << "video/x-raw,format=BGR,width=" << w << ",height=" << h << " ! "
       << "videoconvert ! videorate ! video/x-raw,framerate=" << fps << "/1 ! "
       << "queue ! x264enc bitrate=" << bitrate_kbps
       << " speed-preset=ultrafast tune=zerolatency ! "
       << "video/x-h264,stream-format=byte-stream ! "
       << "h264parse ! mp4mux ! filesink location=" << recordingFilename << " sync=true";
    return ss.str();
}

// Try to open a pipeline and print helpful diagnostics.
bool GStreamer::open_capture_with_pipeline(const std::string &pipeline)
{
    // std::cerr << "Trying pipeline: " << pipeline << std::endl;
    //  Ask GStreamer to be a bit more verbose (helps when launching from the terminal)
    //  Only set if not already set in the environment
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
    // std::cerr << "Opened capture: width=" << w << " height=" << h << " fps=" << fps << std::endl;

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