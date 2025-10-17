#pragma once
#include <iostream>
#include <fstream>

struct WAVHeader
{
    char riff[4] = {'R', 'I', 'F', 'F'};    // "RIFF"
    uint32_t fileSize;                      // Size of the entire file minus 8 bytes
    char wave[4] = {'W', 'A', 'V', 'E'};    // "WAVE"
    char fmt[4] = {'f', 'm', 't', ' '};     // "fmt "
    uint32_t fmtSize = 16;                  // Size of the fmt chunk (16 for PCM)
    uint16_t audioFormat = 1;               // Audio format (1 for PCM)
    uint16_t numChannels = 1;               // Number of channels
    uint32_t sampleRate = 44100;            // Sample rate
    uint32_t byteRate;                      // Byte rate (Sample Rate * NumChannels * BitsPerSample/8)
    uint16_t blockAlign;                    // Block align (NumChannels * BitsPerSample/8)
    uint16_t bitsPerSample = 16;            // Bits per sample
    char data[4] = {'d', 'a', 't', 'a'};    // "data"
    uint32_t dataSize;                      // Size of the data section

    void writeHeader(std::ofstream& outFile);

    void finalizeHeader(std::ofstream& outFile, uint32_t dataSize);
};