#include "wavHeader.h"

void WAVHeader::writeHeader(std::ofstream& outFile)
{
    {
        byteRate = sampleRate * numChannels * bitsPerSample / 8;
        blockAlign = numChannels * bitsPerSample / 8;
        fileSize = 36; // placeholder, will be updated later
        dataSize = 0;  // placeholder, will be updated later
        outFile.write(reinterpret_cast<const char*>(this), sizeof(WAVHeader));
    }
}

void WAVHeader::finalizeHeader(std::ofstream& outFile, uint32_t dataSize)
{
    this->dataSize = dataSize;
    fileSize = 36 + dataSize;

    outFile.seekp(0, std::ios::beg);
    outFile.write(reinterpret_cast<const char*>(this), sizeof(WAVHeader));
}