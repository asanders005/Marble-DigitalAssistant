#include "Audio/audioRecorder.h"
#include "Hardware/Pinboard.h"
#include "Camera/PiCam.h"
#include "ImageRec/onnx_classifier.h"

#include <iostream>
#include <memory>
#include <wiringPi.h>
#include <filesystem>

int main(int, char **)
{
    // Initialize wiringPi library
    Pinboard::initialize();

    int breadboardPin = 4; // GPIO pin 4 (Physical pin 7)
    Pinboard::setOutputPin(breadboardPin);
    
    int inputPin = 16; // Physical pin 36
    Pinboard::setInputPin(inputPin, PUD_UP);
    
    // Initialize ONNX Classifier
    std::string model = "build/Assets/onnx/mobilenetv2-12-int8.onnx";
    std::string image = "build/Assets/Images/dog.jpg";
    std::string labels = "build/Assets/onnx/imagenet-simple-labels.json";
    int threads = 2;

    ONNXClassifier clf;
    clf.Initialize(model, labels, threads);

    PiCam camera;
    camera.ConfigureStill(1080, 1080, 60);
    
    bool lightOn = false;
    bool toggleAvailable = true;

    bool quit = false;

    while (true)
    {
        if (digitalRead(inputPin) == LOW && toggleAvailable) // Button pressed
        {
            lightOn = !lightOn;
            digitalWrite(breadboardPin, lightOn ? HIGH : LOW);
            camera.CaptureStill("image.jpg");
            
            toggleAvailable = false;
            delay(300); // Debounce delay
        }
        
        if (digitalRead(inputPin) == HIGH) // Button released
        {
            toggleAvailable = true;
        }
    }
}
