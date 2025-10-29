#pragma once
#include <string>

#include <core/rpicam_app.hpp>
#include <core/rpicam_encoder.hpp>

class PiCam
{
public:
    void ConfigureStill(unsigned int width, unsigned int height, int quality = 93);
    void ConfigureVideo(unsigned int width, unsigned int height, float framerate = 30.0f, int bitrate = 8000000);

    void CaptureStill(const std::string& outFile);
    void CaptureVideo(const std::string& outFile, int durationSeconds);    

private:
    RPiCamApp camera;
    bool stillConfigured = false;
    
    RPiCamEncoder encoder;
    bool videoConfigured = false;
};