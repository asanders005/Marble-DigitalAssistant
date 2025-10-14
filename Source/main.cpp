#include "Audio/audioRecorder.h"

#include <iostream>
#include <wiringPi.h>

int main(int, char **)
{
    wiringPiSetupGpio();

    int breadboardPin = 4; // GPIO pin 4 (Physical pin 7)
    int inputPin = 16; // Physical pin 36

    bool lightOn = false;
    bool toggleAvailable = true;
    
    pinMode(breadboardPin, OUTPUT);
    pinMode(inputPin, INPUT);
    pullUpDnControl(inputPin, PUD_UP); // Enable pull-up resistor

    AudioRecorder recorder{ true };
    bool isRecording = false;

    while (true)
    {
        if (digitalRead(inputPin) == LOW && toggleAvailable) // Button pressed
        {
            lightOn = !lightOn;
            digitalWrite(breadboardPin, lightOn ? HIGH : LOW);
            toggleAvailable = false;
            delay(300); // Debounce delay

            isRecording = !isRecording;
            if (isRecording)
            {
                isRecording = recorder.startRecording("recording.wav");
            }
            else
            {
                recorder.stopRecording();
            }
        }
        
        if (digitalRead(inputPin) == HIGH) // Button released
        {
            toggleAvailable = true;
        }

        if (isRecording)
        {
            recorder.recordChunk();
        }
    }
}
