#pragma once
#include <string>
#include <vector>

class WAVFileLoader
{
public:
    static bool LoadWAVFile(const std::string& filePath, std::vector<float>& outSamples);
};