#pragma once
#include "MetricStruct.h"

#include <memory>
#include <map>
#include <string>
#include <vector>

class MetricTracker
{
public:
    MetricTracker() = default;

    void NewMetric();
    void EndMetric();
    void PersonEntered(int trackId);
    void PersonPassed(int trackId);

    bool WriteToFile(const std::string& filename) const;
    bool WriteDateTime() const;
    
    void ResetMetrics();

private:
    bool CanAddPerson(int trackId);
    
private:
    std::unique_ptr<MetricData> currentMetric{ nullptr };
    std::vector<std::unique_ptr<MetricData>> metrics;

    std::map<int, int> activeTracks; // <trackId, captureCount>
    std::vector<int> savedTracks;   // trackIds already counted
};