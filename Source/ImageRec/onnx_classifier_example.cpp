#include "onnx_classifier.h"
#include <opencv2/imgcodecs.hpp>
#include <iostream>

int main(int argc, char** argv) {
    std::string model = "build/Assets/onnx/mobilenetv2-12-int8.onnx";
    std::string image = "build/Assets/images/dog.jpg";
    std::string labels = "build/Assets/onnx/imagenet-simple-labels.json";
    int threads = 2;

    ONNXClassifier clf;
    clf.setNumThreads(threads);
    if (!clf.loadModel(model)) {
        std::cerr << "Failed to load model: " << model << std::endl;
        return 2;
    }
    if (!labels.empty()) {
        if (!clf.loadLabels(labels)) {
            std::cerr << "Warning: failed to load labels file: " << labels << std::endl;
        }
    }

    cv::Mat img = cv::imread(image);
    if (img.empty()) {
        std::cerr << "Failed to open image: " << image << std::endl;
        return 3;
    }

    auto res = clf.classify(img, 5);
    std::cout << "Top predictions:\n";
    for (size_t i = 0; i < res.size(); ++i) {
        std::cout << i+1 << ": " << res[i].first << " (" << res[i].second << ")\n";
    }

    return 0;
}
