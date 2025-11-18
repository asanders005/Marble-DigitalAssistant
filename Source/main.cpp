#include "ImageRec/YoloModel.h"
#include "Camera/GStreamer.h"

#include <memory>

int main()
{
    int W = 640, H = 480, FPS = 30;

    // Initialize ONNX Classifier
    std::string modelPath = "build/Assets/onnx/yolov5n-CV-sim.onnx";
    std::string labelsPath = "build/Assets/onnx/coco.yaml";
    int threadCount = 2;

    std::unique_ptr<YOLOModel> model = std::make_unique<YOLOModel>();
    if (!model->Initialize(modelPath, labelsPath, threadCount))
    {
        std::cerr << "YOLOModel failed to initialize. Check model path and files. Exiting.\n";
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
    gst->startRecordingDateTime();

    float predictionDelay = 1000.0f; // milliseconds between predictions
    float currentTime = static_cast<float>(cv::getTickCount()) / cv::getTickFrequency() * 1000.0f;
    float lastPredictionTime = 0.0f;

    float videoLengthMs = 3600000.0f; // 1 hour
    float endTime = currentTime + videoLengthMs;
    
    std::vector<YoloDetection> detections;
    while (true)
    {
        cv::Mat frame = gst->captureFrame();

        currentTime = static_cast<float>(cv::getTickCount()) / cv::getTickFrequency() * 1000.0f;
        if (currentTime - lastPredictionTime >= predictionDelay)
        {
            detections.clear();
            detections = model->detect(frame, 0.5f, 0.5f);
            if (detections.empty())
                std::cout << "No detections.\n";
            else
                std::cout << "Detections: " << detections.size() << "\n";
            lastPredictionTime = currentTime;
        }
        if (currentTime >= endTime)
        {
            gst->stopRecording();
            gst->startRecordingDateTime();
            endTime = currentTime + videoLengthMs;
        }

        for (const auto& det : detections)
        {
            cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);
            std::string label = "ID: " + std::to_string(det.trackId) + " Conf: " + std::to_string(det.score);
            cv::putText(frame, label, cv::Point(det.box.x, det.box.y - 10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
        }

        // For debugging show the frame:
        cv::imshow("camera", frame);
        if (cv::waitKey(1) == 27)
            break; // ESC to exit
    }
    gst->stopRecording();
    return 0;
}