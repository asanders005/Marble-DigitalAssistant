#pragma once
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

class ONNXClassifier {
public:
    ONNXClassifier();
    // load model; returns true on success
    bool loadModel(const std::string &modelPath);
    // load labels (supports simple newline list or JSON array of strings)
    bool loadLabels(const std::string &labelsPath);
    // set internal thread usage (calls cv::setNumThreads)
    void setNumThreads(int n);
    // classify an image given as cv::Mat; returns vector of (label,score)
    std::vector<std::pair<std::string,float>> classify(const cv::Mat &img, int topK = 5);

private:
    cv::dnn::Net net_;
    std::vector<std::string> labels_;
    cv::Size inputSize_ = cv::Size(224,224);
    std::vector<float> mean_ = {0.485f, 0.456f, 0.406f};
    std::vector<float> std_  = {0.229f, 0.224f, 0.225f};

    cv::Mat preprocess(const cv::Mat &img) const;
};
