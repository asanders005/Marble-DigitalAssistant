#include "MetricTracker.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <json.hpp>

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

bool MetricTracker::WriteToFile(const std::string& filename) const
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
    return true;
}

bool MetricTracker::WriteDateTime() const
{
    std::time_t t = std::time(0);
    std::tm tm = *std::localtime(&t);
    std::string filename = "build/Data/Metrics/" + std::to_string(tm.tm_year + 1900) + "-" +
                        std::to_string(tm.tm_mon + 1) + "-" +
                        std::to_string(tm.tm_mday) + ".json";
    WriteToFile(filename);
    return true;
}

bool MetricTracker::CanAddPerson(int trackId)
{
    auto it = activeTracks.find(trackId);
    if (it != activeTracks.end())
    {
        // Already tracked
        return false;
    }

    if (activeTracks[trackId] < 5)
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

void MetricTracker::ResetMetrics()
{
    metrics.clear();
    currentMetric = nullptr;
    activeTracks.clear();
    savedTracks.clear();
}