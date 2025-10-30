#pragma once
#include <string>

#include <core/rpicam_app.hpp>
#include <core/rpicam_encoder.hpp>
#include <opencv2/core.hpp>

class PiCam
{
public:
    // Ensure camera closed on destruction if still open
    ~PiCam();
    // Gracefully stop camera and teardown internal threads
    void Shutdown();

    void ConfigureStill(unsigned int width, unsigned int height, int quality = 93);
    void ConfigureVideo(unsigned int width, unsigned int height, float framerate = 30.0f, int bitrate = 8000000);

    void CaptureStill(const std::string& outFile);
    void CaptureVideo(const std::string& outFile, int durationSeconds);    

    // Capture a still and return it as a BGR cv::Mat (cloned). Throws on failure.
    cv::Mat CaptureToMat(bool saveDebugJPEG = false);
    
// helpers
public:
    static void listCameras();
    
private:
    RPiCamApp camera;
    bool stillConfigured = false;
    
    RPiCamEncoder encoder;
    bool videoConfigured = false;

    bool cameraRunning = false;
};