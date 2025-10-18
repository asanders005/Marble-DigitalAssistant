#pragma once
#include "whisper.h"
#include <string>

class VTT
{
public:
    bool Initialize(const std::string& modelPath = "ThirdParty/whisper.cpp/models/ggml-tiny.en.bin");

    std::string Transcribe(const std::string& audioFilePath);

private:
    struct whisper_context* ctx = nullptr;
    
};