#pragma once
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <string>

class MongoLink
{
public:
    MongoLink(const MongoLink&) = delete;
    MongoLink& operator=(const MongoLink&) = delete;
    static MongoLink& GetInstance()
    {
        static MongoLink instance;
        return instance;
    }

    bool UploadMetric(const std::string& jsonPath) const;
    bool UploadVideo(const std::string& videoPath, const std::string& videoName) const;
    
private:
    MongoLink();

private:
    std::unique_ptr<mongocxx::instance> instance; // This should be done only once.
    std::unique_ptr<mongocxx::client> client;
    mongocxx::database db;
};