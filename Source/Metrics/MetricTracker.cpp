#include "MetricTracker.h"
#include "MongoLink.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <json.hpp>
#include <filesystem>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

void MetricTracker::NewMetric()
{
    if (currentMetric)
    {
        EndMetric();
    }
    
    std::time_t t = std::time(0);
    std::tm tm = *std::localtime(&t);
    currentMetric = std::make_unique<MetricData>(tm);
}

void MetricTracker::EndMetric()
{
    if (currentMetric)
    {
        std::time_t t = std::time(0);
        std::tm tm = *std::localtime(&t);
        currentMetric->endTime = tm;
        metrics.push_back(std::move(currentMetric));
        currentMetric = nullptr;
    }
}

void MetricTracker::PersonEntered(int trackId)
{
    if (currentMetric && CanAddPerson(trackId))
    {
        currentMetric->enterCount++;
        currentMetric->totalPeople++;

        savedTracks.push_back(trackId);
    }
}

void MetricTracker::PersonPassed(int trackId)
{
    if (currentMetric && CanAddPerson(trackId))
    {
        currentMetric->passCount++;
        currentMetric->totalPeople++;

        savedTracks.push_back(trackId);
    }
}

int MetricTracker::GetCurrentCount()
{
    if (currentMetric)
    {
        return currentMetric->totalPeople;
    }
    return 0;
}

bool MetricTracker::WriteToFile(const std::string& filename, bool upload) const
{
    std::cout << "[MetricTracker] Writing metrics to file: " << filename << std::endl;
    
    if (metrics.empty())
    {
        std::cerr << "[MetricTracker] WriteToFile: No metrics to write.\n";
        return false;
    }
    
    std::ofstream ofs(filename);
    if (!ofs.is_open())
    {
        std::cerr << "[MetricTracker] WriteToFile: Failed to open file for writing: " << filename << std::endl;
        return false;
    }

    nlohmann::json metricsJson;
    int totalPeople = 0;
    int totalPass = 0;
    int totalEnter = 0;
    for (const auto& metric : metrics)
    {
        metricsJson[metric->name] = {
            {"startTime", formatTime(metric->startTime)},
            {"endTime", formatTime(metric->endTime)},
            {"totalPeople", metric->totalPeople},
            {"passCount", metric->passCount},
            {"enterCount", metric->enterCount}
        };
        totalPeople += metric->totalPeople;
        totalPass += metric->passCount;
        totalEnter += metric->enterCount;
    }

    metricsJson["totals"] = {
        {"totalPeople", totalPeople},
        {"totalPass", totalPass},
        {"totalEnter", totalEnter}
    };

    ofs << std::setw(4) << metricsJson << std::endl;
    ofs.close();
    if (upload)
    {
        MongoLink::GetInstance().UploadMetric(filename);
    }
    
    return true;
}

bool MetricTracker::WriteDateTime(bool upload) const
{
    std::time_t t = std::time(0);
    std::tm tm = *std::localtime(&t);
    std::string filename = "build/Data/Metrics/" + std::to_string(tm.tm_year + 1900) + "-" +
                        std::to_string(tm.tm_mon + 1) + "-" +
                        std::to_string(tm.tm_mday) + ".json";
    WriteToFile(filename, upload);
    return true;
}

bool MetricTracker::CanAddPerson(int trackId)
{
    if (std::find(savedTracks.begin(), savedTracks.end(), trackId) != savedTracks.end())
    {
        // Already tracked
        return false;
    }

    if (activeTracks[trackId] < 3)
    {
        // Increment capture count
        activeTracks[trackId]++;
        return false;
    }
    else
    {
        // Can add person now
        activeTracks.erase(trackId);
        return true;
    }
}

bool MetricTracker::UploadAllMetrics() const
{
    namespace fs = std::filesystem;

    fs::path metricsDir = "build/Data/Metrics";
    if (!fs::exists(metricsDir) || !fs::is_directory(metricsDir)) {
        std::cerr << "[MetricTracker] Metrics directory does not exist: " << metricsDir << std::endl;
        return false;
    }

    for (const auto& entry : fs::directory_iterator(metricsDir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().string();
            MongoLink::GetInstance().UploadMetric(filename);
        }
    }

    fs::path videosDir = "build/Data/Videos";
    if (!fs::exists(videosDir) || !fs::is_directory(videosDir)) {
        std::cerr << "[MetricTracker] Videos directory does not exist: " << videosDir << std::endl;
        return false;
    }

    for (const auto& entry : fs::directory_iterator(videosDir)) {
        if (entry.is_regular_file()) {
            std::string filepath = entry.path().string();
            std::string filename = entry.path().filename().string();
            MongoLink::GetInstance().UploadVideo(filepath, filename);
        }
    }
    
    return true;
}

void MetricTracker::ResetMetrics()
{
    metrics.clear();
    currentMetric = nullptr;
    activeTracks.clear();
    savedTracks.clear();
}