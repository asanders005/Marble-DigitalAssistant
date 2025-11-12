#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

int main(int argc, char **argv)
{
    // if (argc < 2)
    // {
    //     std::cout << "Usage: onnx_diagnostics <build/Assets/onnx/model.onnx>\n";
    //     return -1;
    // }

    std::string modelPath = "build/Assets/onnx/model.onnx";

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_VERBOSE);

    try
    {
        // Load the ONNX model
        std::cout << "Trying readNetFromONNX with model: " << modelPath << std::endl;

        cv::dnn::Net net = cv::dnn::readNetFromONNX(modelPath);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::cout << "Model loaded successfully\n";

        // Print input layer information
        std::vector<std::string> inputNames = net.getLayerNames();
        std::cout << "Input Layers: " << inputNames.size() << " Names:\n";
        for (size_t i = 0; i < inputNames.size(); i++)
        {
            std::cout << i << ": " << inputNames[i] << std::endl;
        }

        // Print output layer information
        std::vector<std::string> outputNames = net.getUnconnectedOutLayersNames();
        std::cout << "Unconnected output names:\n";
        for (auto &name : outputNames)
        {
            std::cout << name << std::endl;
        }

        // try a few input sizes common for YOLO models
        std::vector<cv::Size> testSizes = { {640,640}, {320,320}, {384,384} };
        for (auto s : testSizes) {
            try {
                std::cerr << "Testing forward with dummy image size " << s.width << "x" << s.height << "\n";
                cv::Mat img = cv::Mat::zeros(s, CV_8UC3);
                cv::Mat blob = cv::dnn::blobFromImage(img, 1.0/255.0, s, cv::Scalar(), true, false);
                net.setInput(blob);
                std::vector<cv::Mat> outs;
                net.forward(outs, outputNames);
                std::cerr << "Forward succeeded, got " << outs.size() << " outputs\n";
                for (size_t i=0;i<outs.size();++i) {
                    std::cerr << "  out["<<i<<"] dims=" << outs[i].dims;
                    for (int d=0; d<outs[i].dims; ++d) std::cerr << " " << outs[i].size[d];
                    std::cerr << "\n";
                }
            } catch (const std::exception &e) {
                std::cerr << "Forward failed with size " << s.width << "x" << s.height << ": " << e.what() << "\n";
            }
        }
    }
    catch (const cv::Exception &e)
    {
        std::cerr << "Error loading model: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}