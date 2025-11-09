#pragma once

#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <atomic
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class GstRecorder
{
public:
    GstRecorder();
    ~GstRecorder();

    bool start(const std::string& filename, int width, int height, double fps, int bitrate_kbps = 2000, size_t maxQueueSize = 30);
    bool pushFrame(const cv::Mat& frame);
    bool isRunning() const {
        return running.load();
    }
    void stop();

private:
    void recorderThreadFunc();

    std::string buildPipelineString();

    GstBuffer* matToGstBuffer(const cv::Mat& frame, GstClockTime pts);

private:
    // GStreamer objects
    GstElement* pipeline{ nullptr };
    GstAppSrc* appsrc{ nullptr };

    std::deque<cv::Mat> frameQueue;
    size_t maxQueueSize = 30;
    std::mutex queueMutex;
    std::condition_variable queueCondVar;

    std::thread recorderThread;
    std::atomic<bool> running{ false };
    std::atomic<bool> stopRequested{ false };

    std::string filename;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int bitrate_kbps = 2000;
    uint64_t frameIndex = 0;
}