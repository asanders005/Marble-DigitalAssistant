#include <ctime>
#include <string>
#include <iomanip>
#include <sstream>

inline std::string formatTime(const std::tm& time)
{
    std::ostringstream oss;
    oss << std::put_time(&time, "%Y-%m-%d %H:%M:%S");
    return oss.str();
};

struct MetricData
{
    MetricData(const std::tm& start) : startTime(start), endTime(start) 
    { 
        name = formatTime(start);
    }

    MetricData(const std::tm& start, const std::tm& end, int total, int pass, int enter)
        : startTime(start), endTime(end), totalPeople(total), passCount(pass), enterCount(enter) 
    { 
        name = formatTime(start);
    }
    
    std::string name;
    std::tm startTime;
    std::tm endTime;

    int totalPeople = 0;
    int passCount = 0;
    int enterCount = 0;
};
