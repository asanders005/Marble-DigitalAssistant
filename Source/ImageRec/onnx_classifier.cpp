#include "onnx_classifier.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <iostream>

ONNXClassifier::ONNXClassifier() {}

bool ONNXClassifier::loadModel(const std::string &modelPath) {
    try {
        net_ = cv::dnn::readNetFromONNX(modelPath);
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    } catch (const std::exception &e) {
        std::cerr << "Failed to load model: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool ONNXClassifier::loadLabels(const std::string &labelsPath) {
    labels_.clear();
    std::ifstream ifs(labelsPath);
    if (!ifs) return false;
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    // detect simple JSON array of strings
    if (!content.empty() && content.front() == '[') {
        size_t pos = 0;
        while (true) {
            pos = content.find('"', pos);
            if (pos == std::string::npos) break;
            size_t end = content.find('"', pos+1);
            if (end == std::string::npos) break;
            labels_.push_back(content.substr(pos+1, end-pos-1));
            pos = end + 1;
        }
    } else {
        ifs.clear();
        ifs.seekg(0);
        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty()) labels_.push_back(line);
        }
    }
    return !labels_.empty();
}

void ONNXClassifier::setNumThreads(int n) {
    cv::setNumThreads(n);
}

cv::Mat ONNXClassifier::preprocess(const cv::Mat &img) const {
    cv::Mat resized;
    cv::resize(img, resized, inputSize_);
    cv::Mat blob;
    // blobFromImage: scalefactor will scale to [0,1]
    cv::dnn::blobFromImage(resized, blob, 1.0/255.0, inputSize_, cv::Scalar(0,0,0), true, false);
    // convert blob (NCHW) to image, normalize, convert back
    std::vector<cv::Mat> imgs;
    cv::dnn::imagesFromBlob(blob, imgs);
    if (imgs.empty()) return cv::Mat();
    cv::Mat nm = imgs[0]; // HxWxC BGR
    cv::cvtColor(nm, nm, cv::COLOR_BGR2RGB);
    std::vector<cv::Mat> planes(3);
    cv::split(nm, planes);
    for (int c = 0; c < 3; ++c) {
        planes[c] = (planes[c] - mean_[c]) / std_[c];
    }
    cv::merge(planes, nm);
    cv::Mat inBlob = cv::dnn::blobFromImages(nm);
    return inBlob;
}

std::vector<std::pair<std::string,float>> ONNXClassifier::classify(const cv::Mat &img, int topK) {
    std::vector<std::pair<std::string,float>> results;
    if (img.empty() || net_.empty()) return results;
    cv::Mat input = preprocess(img);
    if (input.empty()) return results;
    net_.setInput(input);
    cv::Mat prob = net_.forward();
    cv::Mat probMat = prob.reshape(1, 1);
    std::vector<std::pair<int,float>> idx;
    for (int i = 0; i < probMat.cols; ++i) {
        idx.emplace_back(i, probMat.at<float>(0, i));
    }
    if ((int)idx.size() < topK) topK = idx.size();
    std::partial_sort(idx.begin(), idx.begin()+topK, idx.end(), [](auto &a, auto &b){ return a.second > b.second; });
    for (int i = 0; i < topK; ++i) {
        int id = idx[i].first;
        float score = idx[i].second;
        std::string label = (id < (int)labels_.size() ? labels_[id] : std::to_string(id));
        results.emplace_back(label, score);
    }
    return results;
}
