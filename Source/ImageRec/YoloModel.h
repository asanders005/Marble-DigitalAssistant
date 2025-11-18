#pragma once

#include <string>
#include <memory>
#include <vector>
#include <opencv2/dnn.hpp>
#include <opencv2/core.hpp>
#include <unordered_map>

struct YoloDetection {
    cv::Rect box;     // xmin,ymin,width,height in pixel coords (relative to inputSize_)
    float score;      // confidence score
    int classId;      // class id (for person-only models usually 0)
    int trackId;      // persistent ID assigned by a simple tracker
};

class YOLOModel
{
public:
    YOLOModel();
    bool Initialize(const std::string& modelPath, const std::string& labelsPath, int threadCount = 1);
    // load model; returns true on success
    bool loadModel(const std::string& modelPath);
    // load labels (supports simple newline list or JSON array of strings)
    bool loadLabels(const std::string& labelsPath);
    
    // Run detection on an image. Returns detections (with track IDs assigned).
    std::vector<YoloDetection> detect(const cv::Mat& img, float confThresh = 0.25f, float iouThresh = 0.45f);

    // Set the input size explicitly (width,height). If not set, default is 640x640.
    void setInputSize(const cv::Size &s) { inputSize = s; }

    // Reset simple tracker (clears previous tracks)
    void resetTracks() { prevDetections.clear(); nextTrackId = 1; }

    // Tweak detection/post-processing behavior
    void setKeepAliveFrames(int k) { keepAliveFrames = k; }
    void setMinBoxAreaRatio(float r) { minBoxAreaRatio = r; }
    void setMinBoxHeightRatio(float r) { minBoxHeightRatio = r; }

private:
    // Utility: compute IoU between two boxes
    float iou(const cv::Rect &a, const cv::Rect &b);

private:
    cv::Size inputSize = cv::Size(640, 640);
    std::unique_ptr<cv::dnn::Net> net;
    std::vector<std::string> labels;

    // Simple IoU-based tracker state
    int nextTrackId = 1;
    std::vector<YoloDetection> prevDetections;
    int frameIndex = 0;
    int keepAliveFrames = 5; // how many frames to keep a lost track visible
    float minBoxAreaRatio = 0.002f; // relative to image area
    float minBoxHeightRatio = 0.12f; // relative to image height
    std::unordered_map<int,int> trackLastSeen; // trackId -> last seen frameIndex
    std::unordered_map<int,cv::Rect> trackLastBox; // trackId -> last box
    std::unordered_map<int,float> trackLastScore; // trackId -> last score
};