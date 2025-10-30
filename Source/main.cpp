#include "Audio/audioRecorder.h"
#include "Camera/PiCam.h"
#include "Hardware/Pinboard.h"
#include "ImageRec/onnx_classifier.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <opencv2/imgcodecs.hpp>
#include <wiringPi.h>

int main(int, char **)
{
    // Initialize wiringPi library
    Pinboard::initialize();

    int breadboardPin = 4; // GPIO pin 4 (Physical pin 7)
    Pinboard::setOutputPin(breadboardPin);

    int inputPin = 16; // Physical pin 36
    Pinboard::setInputPin(inputPin, PUD_UP);

    // Initialize ONNX Classifier
    PiCam camera;
    // Configure the camera early so large model allocations (ONNX/OpenCV) do not exhaust CMA memory
    camera.ConfigureStill(1080, 1080, 60);

    // Initialize ONNX Classifier
    std::string model = "build/Assets/onnx/mobilenetv2-12-int8.onnx";
    std::string image = "build/Assets/Images/dog.jpg";
    std::string labels = "build/Assets/onnx/imagenet-simple-labels.json";
    int threads = 2;

    ONNXClassifier clf;
    clf.Initialize(model, labels, threads);

    // If ONE_SHOT_CAPTURE is set in environment run one capture and exit (useful for testing)
    if (std::getenv("ONE_SHOT_CAPTURE"))
    {
        try
        {
            auto mat = camera.CaptureToMat();
            if (!mat.empty())
            {
                auto res = clf.classify(mat, 1);
                std::cout << "Top prediction:\n";
                if (!res.empty())
                {
                    std::cout << "1: " << res[0].first << " (" << res[0].second << ")\n";
                }
            }
            else
            {
                std::cerr << "CaptureToMat returned empty image" << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "ONE_SHOT_CAPTURE failed: " << e.what() << std::endl;
        }

        // Ensure rpicam-apps threads and camera are torn down cleanly before exiting.
        try
        {
            camera.Shutdown();
        }
        catch (...)
        {
        }

        return 0;
    }

    bool lightOn = false;
    bool toggleAvailable = true;

    bool quit = false;
    bool classifyImage = true;

    while (true)
    {
        if (digitalRead(inputPin) == LOW && toggleAvailable) // Button pressed
        {
            lightOn = !lightOn;
            digitalWrite(breadboardPin, lightOn ? HIGH : LOW);
            
            if (classifyImage)
            {
                auto img = camera.CaptureToMat(true);
                if (!img.empty())
                {
                    auto res = clf.classify(img, 1);
                    std::cout << "Top prediction:\n";
                    if (!res.empty())
                    {
                        std::cout << "1: " << res[0].first << " (" << res[0].second << ")\n";
                    }
                }
                else
                {
                    std::cerr << "Failed to open captured image." << std::endl;
                }
            }

            toggleAvailable = false;
            delay(300); // Debounce delay
        }

        if (digitalRead(inputPin) == HIGH) // Button released
        {
            toggleAvailable = true;
        }
    }
    camera.Shutdown();
}
