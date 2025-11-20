#include "Camera/GStreamer.h"
#include "ImageRec/YoloModel.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

int main()
{
#pragma region Setup
    int W = 640, H = 480, FPS = 30;

    // Initialize ONNX Classifier
    std::string modelPath = "build/Assets/onnx/yolov5n-sim.onnx";
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
#pragma endregion

#pragma region DetectionThread
    cv::Mat latestFrame;
    std::vector<YoloDetection> latestDetections;
    std::mutex frameMutex, detectionMutex;
    std::atomic<bool> running(true);

    auto detectionThread = [&]()
    {
        while (running.load())
        {
            cv::Mat frameCopy;
            {
                std::lock_guard<std::mutex> lock(frameMutex);
                if (latestFrame.empty())
                    continue;
                frameCopy = latestFrame.clone();
            }

            if (!frameCopy.empty())
            {
                cv::Mat resultFrame;
                cv::cvtColor(frameCopy, resultFrame, cv::COLOR_BGR2RGB);
                std::vector<YoloDetection> detections = model->detect(resultFrame, 0.3f, 0.5f);
                {
                    std::lock_guard<std::mutex> lock(detectionMutex);
                    latestDetections = detections;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(predictionDelay)));
        }
    };

    std::thread detectThread(detectionThread);
#pragma endregion

#pragma region MainLoop
    while (true)
    {
        cv::Mat frame = gst->captureFrame();
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            latestFrame = frame.clone();
        }
        
        std::vector<YoloDetection> detectionsCopy;
        {
            std::lock_guard<std::mutex> lock(detectionMutex);
            detectionsCopy = latestDetections;
        }

        currentTime = static_cast<float>(cv::getTickCount()) / cv::getTickFrequency() * 1000.0f;
        if (currentTime >= endTime)
        {
            gst->stopRecording();
            gst->startRecordingDateTime();
            endTime = currentTime + videoLengthMs;
        }

        for (const auto &det : detectionsCopy)
        {
            if (det.score < 0.3f)
                continue;

            cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);
            std::string label =
                "ID: " + std::to_string(det.trackId) + " Conf: " + std::to_string(det.score);
            cv::putText(frame, label, cv::Point(det.box.x, det.box.y - 10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
        }

        // For debugging show the frame:
        cv::imshow("camera", frame);
        if (cv::waitKey(1) == 27)
            break; // ESC to exit
    }
#pragma endregion

    gst->stopRecording();
    running.store(false);
    detectThread.join();

    return 0;
}