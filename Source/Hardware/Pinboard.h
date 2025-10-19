#pragma once
#include <wiringPi.h>

class Pinboard
{
public:
    static void initialize();
    static void setInputPin(int pin, int pullUpDown);
    static void setOutputPin(int pin);
};