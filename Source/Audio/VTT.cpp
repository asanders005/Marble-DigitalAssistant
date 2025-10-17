#include "VTT.h"
#include "wavFileLoader.h"
#include <iostream>
#include <vector>

bool VTT::Initialize(const std::string& modelPath)
{
    // Initialize the Whisper model from the specified path
    struct whisper_context_params cparams = whisper_context_default_params();
    ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!ctx)
    {
        std::cerr << "Failed to initialize Whisper model from " << modelPath << std::endl;
        return false;
    }
    
    return true;
}

std::string VTT::Transcribe(const std::string& audioFilePath)
{
    std::vector<float> pcmf32;
    if (!WAVFileLoader::LoadWAVFile(audioFilePath, pcmf32))
    {
        return "Failed to load audio file: " + audioFilePath;
        return "";
    }
    
    // Transcribe the audio file using Whisper
    whisper_full_params* wparams = whisper_full_default_params_by_ref(WHISPER_SAMPLING_GREEDY);
    wparams->print_progress = true;

    if (whisper_full(ctx, *wparams, pcmf32.data(), pcmf32.size()) != 0) {
        return "Transcription failed.";
    }

    int n_segments = whisper_full_n_segments(ctx);
    std::string transcription;
    for (int i = 0; i < n_segments; ++i) 
    {
        const char* text = whisper_full_get_segment_text(ctx, i);
        std::cout << text;
        transcription += text;
    }
    return transcription;
}