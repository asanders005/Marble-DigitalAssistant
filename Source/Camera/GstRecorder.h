#pragma once

#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

class GstRecorder
{
public:
    GstRecorder();
    ~GstRecorder();

    bool configure(int width, int height, double fps, int bitrate_kbps = 2000, size_t maxQueueSize = 30) 
    {
        if (running.load()) return false;
        
        this->width = width;
        this->height = height;
        this->fps = fps;
        this->bitrate_kbps = bitrate_kbps;
        this->maxQueueSize = maxQueueSize;
        return true;
    }
    
    bool start(const std::string& filename, int width, int height, double fps, int bitrate_kbps = 2000, size_t maxQueueSize = 30);
    bool start(const std::string& filename);
    bool pushFrame(const cv::Mat& frame);
    bool isRunning() const {
        return running.load();
    }
    void stop(bool upload = false);

private:
    void recorderThreadFunc();

    std::string buildPipelineString();

    GstBuffer* matToGstBuffer(const cv::Mat& frame, GstClockTime pts);

private:
    // GStreamer objects
    GstElement* pipeline{ nullptr };
    GstAppSrc* appsrc{ nullptr };

    // store frame with its capture timestamp (steady clock)
    using TimePoint = std::chrono::steady_clock::time_point;
    std::deque<std::pair<cv::Mat, TimePoint>> frameQueue;
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
    GstClockTime frameDuration = 0;
    // small runtime debug counter to print first few PTS values
    int debugPrintCount = 0;
    // base time recorded at start() used to compute PTS from capture timestamps
    TimePoint baseTime;
};