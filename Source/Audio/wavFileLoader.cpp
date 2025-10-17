#include "wavFileLoader.h"
#include "wavHeader.h"
#include <iostream>
#include <fstream>

bool WAVFileLoader::LoadWAVFile(const std::string& filePath, std::vector<float>& outSamples)
{
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Failed to open WAV file: " << filePath << std::endl;
        return false;
    }

    // Read WAV header
    WAVHeader header;
    inFile.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    // Check if it's a valid WAV file
    // if (header.chunkID != 0x46464952 || header.format != 0x45564157) // "RIFF" and "WAVE"
    // {
    //     std::cerr << "Invalid WAV file format: " << filePath << std::endl;
    //     return false;
    // }

    //outSampleRate = header.sampleRate;

    // Read audio data
    outSamples.resize(header.dataSize / sizeof(float));
    inFile.read(reinterpret_cast<char*>(outSamples.data()), header.dataSize);

    return true;
}