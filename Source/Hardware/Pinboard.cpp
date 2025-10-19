#include "Pinboard.h"

void Pinboard::initialize()
{
    wiringPiSetupGpio();
}

void Pinboard::setInputPin(int pin, int pullUpDown)
{
    pinMode(pin, INPUT);
    pullUpDnControl(pin, pullUpDown);
}

void Pinboard::setOutputPin(int pin)
{
    pinMode(pin, OUTPUT);
}