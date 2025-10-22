#include "Audio/InferencePool.h"
#include "Audio/wavFileLoader.h"
#include <iostream>
#include <vector>

int main() {
    std::vector<float> samples;
    if (!WAVFileLoader::LoadWAVFile("test_sine.wav", samples)) {
        return 1;
    }

    // Convert float samples to int16_t expected by PCMQueue::Chunk
    std::vector<int16_t> int16_samples;
    int16_samples.reserve(samples.size());
    for (float f : samples) {
        int v = static_cast<int>(f * 32767.0f);
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        int16_samples.push_back(static_cast<int16_t>(v));
    }

    // create pool
    InferencePool pool("ThirdParty/whisper.cpp/models/ggml-tiny.en.bin", 1, 1, 1, 4);
    if (!pool.start()) {
        std::cerr << "Failed to start pool" << std::endl;
        return 2;
    }

    // push all samples as one chunk
    PCMQueue::Chunk chunk;
    chunk.assign(int16_samples.begin(), int16_samples.end());
    pool.pushTask(chunk);

    TranscriptionResult res;
    if (pool.popResult(res)) {
        std::cout << "Test result: " << res.text << std::endl;
    } else {
        std::cout << "No result" << std::endl;
    }

    pool.stop();
    return 0;
}
