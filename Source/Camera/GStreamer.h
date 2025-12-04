#pragma once
#include "GstRecorder.h"

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
    GStreamer() 
    { 
        cap = std::make_unique<cv::VideoCapture>(); 
        recorder = std::make_unique<GstRecorder>();
    };

    bool openCapture(CaptureBackend backend, int w, int h, int fps);
    cv::Mat captureFrame();
    void writeFrame(const cv::Mat& frame);

    bool startRecording(const std::string& filename, int bitrate_kbps = 2000);
    bool startRecordingDateTime(int bitrate_kbps = 2000, const std::string& filenamePrefix = "");
    bool isRecording() const {
        return recorder->isRunning();
        //return (writer && writer->isOpened());
    }
    void stopRecording(bool upload = false);
    
private:
    double measureCaptureFps(int samples = 3);

    std::string gst_pipeline_libcamera();
    std::string gst_pipeline_v4l2();
    std::string get_pipeline_encoderMp4();

    bool open_capture_with_pipeline(const std::string &pipeline);

private:
    std::unique_ptr<cv::VideoCapture> cap;
    std::unique_ptr<GstRecorder> recorder;
    std::unique_ptr<cv::VideoWriter> writer;

    int w, h, fps;
    int bitrate_kbps;
    std::string recordingFilename;
};