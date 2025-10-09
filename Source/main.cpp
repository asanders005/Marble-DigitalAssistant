#include <iostream>
#include <wiringPi.h>

void shortFlash(int pin)
{
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
}

void longFlash(int pin)
{
    digitalWrite(pin, HIGH);
    delay(500);
    digitalWrite(pin, LOW);
    delay(200);
}

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

    while (true)
    {
        if (digitalRead(inputPin) == LOW && toggleAvailable) // Button pressed
        {
            lightOn = !lightOn;
            digitalWrite(breadboardPin, lightOn ? HIGH : LOW);
            toggleAvailable = false;
            delay(300); // Debounce delay
        }
        
        if (digitalRead(inputPin) == HIGH) // Button released
        {
            toggleAvailable = true;
        }
        
        // S.O.S. pattern
        // shortFlash(breadboardPin);
        // shortFlash(breadboardPin);
        // shortFlash(breadboardPin);
        
        // longFlash(breadboardPin);
        // longFlash(breadboardPin);
        // longFlash(breadboardPin);

        // shortFlash(breadboardPin);
        // shortFlash(breadboardPin);
        // shortFlash(breadboardPin);

        // delay(1000);
    }
}
