#include "MetricTracker.h"
#include "MetricStruct.h"

#include <fstream>
#include <iomanip>
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

void MetricTracker::PersonEntered()
{
    if (currentMetric)
    {
        currentMetric->enterCount++;
        currentMetric->totalPeople++;
    }
}

void MetricTracker::PersonPassed()
{
    if (currentMetric)
    {
        currentMetric->passCount++;
        currentMetric->totalPeople++;
    }
}

bool MetricTracker::WriteToFile(const std::string& filename) const
{
    std::ofstream ofs(filename);
    if (!ofs.is_open())
    {
        std::cerr << "[MetricTracker] WriteToFile: Failed to open file for writing: " << filename << std::endl;
        return false;
    }

    json metricsJson;
    int totalPeople = 0;
    int totalPass = 0;
    int totalEnter = 0;
    for (const auto& metric : metrics)
    {
        metricsJson[metric->name] = {
            {"startTime", std::put_time(&metric->startTime, "%Y-%m-%d %H:%M:%S")},
            {"endTime", std::put_time(&metric->endTime, "%Y-%m-%d %H:%M:%S")},
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