#include <ctime>
#include <string>

struct MetricData
{
    MetricData(const std::tm& start) : startTime(start), endTime(start) 
        { name = std::put_time(&startTime, "%Y-%m-%d_%H:%M:%S"); }

    MetricData(const std::tm& start, const std::tm& end, int total, int pass, int enter)
        : startTime(start), endTime(end), totalPeople(total), passCount(pass), enterCount(enter) 
        { name = std::put_time(&startTime, "%Y-%m-%d_%H:%M:%S"); }
    
    std::string name;
    std::tm startTime;
    std::tm endTime;

    int totalPeople = 0;
    int passCount = 0;
    int enterCount = 0;
};