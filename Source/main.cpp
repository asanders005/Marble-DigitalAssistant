#include "Audio/audioRecorder.h"
#include "Audio/RealtimeTranscriber.h"
#include "Audio/PCMQueue.h"
#include "Hardware/Pinboard.h"

#include <iostream>
#include <memory>
#include <wiringPi.h>
#include <filesystem>

int main(int, char **)
{
    Pinboard::initialize();

    int breadboardPin = 4; // GPIO pin 4 (Physical pin 7)
    int inputPin = 16; // Physical pin 36

    Pinboard::setOutputPin(breadboardPin);
    Pinboard::setInputPin(inputPin, PUD_UP);
    
    bool lightOn = false;
    bool toggleAvailable = true;

    std::shared_ptr<PCMQueue> pcmQueue = std::make_shared<PCMQueue>(10); // 64KB max size
    AudioRecorder recorder{ false };
    recorder.setPCMQueue(pcmQueue);

    RealtimeTranscriber::Config config;
    config.modelPath = "ThirdParty/whisper.cpp/models/ggml-tiny.en.bin";
    // Raspberry Pi 4B typically has 4 hardware threads; don't exceed that.
    unsigned int hw = std::thread::hardware_concurrency();
    config.threads = static_cast<int>(std::min<unsigned int>(hw ? hw : 1, 4));
    config.chunkSeconds = 2; // 2s chunks balance latency and throughput on Pi

    RealtimeTranscriber transcriber(*pcmQueue, config);
    if (!transcriber.start()) {
        std::cerr << "Failed to start RealtimeTranscriber" << std::endl;
        return 1;
    }
    
    recorder.startRecording();
    
    bool quit = false;
    while (!quit)
    {
        if (digitalRead(inputPin) == LOW && toggleAvailable) // Button pressed
        {
            quit = true;
        }
        recorder.recordChunk();
    }

    recorder.stopRecording();
    transcriber.stop();
    
    // bool isRecording = false;

    // while (true)
    // {
    //     if (digitalRead(inputPin) == LOW && toggleAvailable) // Button pressed
    //     {
    //         lightOn = !lightOn;
    //         digitalWrite(breadboardPin, lightOn ? HIGH : LOW);
    //         toggleAvailable = false;
    //         delay(300); // Debounce delay

    //         isRecording = !isRecording;
    //         if (isRecording)
    //         {
    //             isRecording = recorder.startRecording("recording.wav");
    //         }
    //         else
    //         {
    //             recorder.stopRecording();
    //         }
    //     }
        
    //     if (digitalRead(inputPin) == HIGH) // Button released
    //     {
    //         toggleAvailable = true;
    //     }

    //     if (isRecording)
    //     {
    //         recorder.recordChunk();
    //     }
    // }
}
