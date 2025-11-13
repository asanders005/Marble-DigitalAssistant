#include <memory>
#include <vector>

struct MetricData;

class MetricTracker
{
public:
    MetricTracker() = default;

    void NewMetric();
    void EndMetric();
    void PersonEntered();
    void PersonPassed();

    bool WriteToFile(const std::string& filename) const;

private:
    std::unique_ptr<MetricData> currentMetric{ nullptr };
    std::vector<std::unique_ptr<MetricData>> metrics;
};