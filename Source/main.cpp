#include "ImageRec/onnx_classifier.h"
#include "Camera/GStreamer.h"

#include <memory>

int main()
{
    int W = 640, H = 480, FPS = 30;

    // Initialize ONNX Classifier
    std::string model = "build/Assets/onnx/mobilenetv2-12-int8.onnx";
    std::string labels = "build/Assets/onnx/imagenet-simple-labels.json";
    int threads = 2;

    std::unique_ptr<ONNXClassifier> clf = std::make_unique<ONNXClassifier>();
    if (!clf->Initialize(model, labels, threads))
    {
        std::cerr << "ONNXClassifier failed to initialize. Check model path and files. Exiting.\n";
        return 1;
    }

    std::unique_ptr<GStreamer> gst = std::make_unique<GStreamer>();
    if (!gst->openCapture(GStreamer::CaptureBackend::LIBCAMERA, W, H, FPS))
    {
        std::cerr << "Failed to open GStreamer capture with LIBCAMERA backend. Trying V4L2...\n";
        if (!gst->openCapture(GStreamer::CaptureBackend::V4L2, W, H, FPS))
        {
            std::cerr << "Failed to open GStreamer capture with V4L2 backend. Exiting.\n";
            return 1;
        }
    }

    float predictionDelay = 1000.0f; // milliseconds between predictions
    float currentTime = static_cast<float>(cv::getTickCount()) / cv::getTickFrequency() * 1000.0f;
    float lastPredictionTime = 0.0f;

    while (true)
    {
        cv::Mat frame = gst->captureFrame();

        currentTime = static_cast<float>(cv::getTickCount()) / cv::getTickFrequency() * 1000.0f;
        if (!(currentTime - lastPredictionTime < predictionDelay))
        {
            auto res = clf->classify(frame, 1);
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