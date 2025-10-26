#include "audioRecorder.h"
#include <cstring>
#include <iostream>

AudioRecorder::AudioRecorder(bool recordToWav) : recordToWav(recordToWav)
{
    wavHeader.sampleRate = rate;
    wavHeader.numChannels = channels;

    // open mic in capture mode
    if ((pcm = snd_pcm_open(&pcmHandle, PCM_MICROPHONE, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf(stderr, "ERROR: Can't open \"%s\" PCM device. %s\n", PCM_MICROPHONE,
                snd_strerror(pcm));
        exit(1);
    }

    // allocate parameters object and fill it with default values
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcmHandle, params);

    // set parameters
    snd_pcm_hw_params_set_access(pcmHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcmHandle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcmHandle, params, channels);
    snd_pcm_hw_params_set_rate_near(pcmHandle, params, &rate, 0);
    snd_pcm_hw_params_set_period_size_near(pcmHandle, params, &framesPerPeriod, &dir);
    std::cout << "framesPerPeriod after set: " << framesPerPeriod << std::endl;
    // snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    // std::cout << "frames after get: " << frames << std::endl;

    // write parameters
    if ((pcm = snd_pcm_hw_params(pcmHandle, params)) < 0)
    {
        fprintf(stderr, "ERROR: Can't set hardware parameters. %s\n", snd_strerror(pcm));
        exit(1);
    }
}

bool AudioRecorder::startRecording(std::string filename)
{
    if (recordToWav)
    {
        bytesRecorded = 0;

        filename = "build/Audio/" + filename;
        wavFile.open(filename, std::ios::binary);
        if (!wavFile.is_open())
        {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return false;
        }

        wavHeader.writeHeader(wavFile);
    }

    // calculate buffer size (in samples)
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    const int samplesPerPeriod = frames * channels; // samples (int16_t)
    buffer.resize(samplesPerPeriod);
    bufferSize = samplesPerPeriod * sizeof(int16_t); // bytes

    std::cout << "Recording started..." << std::endl;
    return true;
}

void AudioRecorder::recordChunk()
{
    // read frames into int16_t buffer
    pcm = snd_pcm_readi(pcmHandle, buffer.data(), frames);
    
    if (pcm == -EPIPE)
    {
        fprintf(stderr, "Overrun occurred\n");
        snd_pcm_prepare(pcmHandle);
    }
    else if (pcm < 0)
    {
        fprintf(stderr, "Error from read: %s\n", snd_strerror(pcm));
    }
    else if (pcm > 0)
    {
        int framesRead = (int)pcm;
        int bytesPerSample = sizeof(int16_t);
        int samplesRead = framesRead * channels;
        int bytesRead = samplesRead * bytesPerSample;

        if (recordToWav)
        {
            // write raw bytes to WAV
            wavFile.write(reinterpret_cast<const char *>(buffer.data()), bytesRead);
            bytesRecorded += bytesRead;
        }
    }
}

void AudioRecorder::stopRecording()
{
    snd_pcm_drain(pcmHandle);
    snd_pcm_close(pcmHandle);
    buffer.clear();

    if (recordToWav)
    {
        wavHeader.finalizeHeader(wavFile, bytesRecorded);
        wavFile.close();
    }

    std::cout << "Recording stopped." << std::endl;
}