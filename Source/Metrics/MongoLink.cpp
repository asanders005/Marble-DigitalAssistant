#include "MongoLink.h"
#include <bsoncxx/json.hpp>
#include <fstream>
#include <iostream>
#include <mongocxx/gridfs/bucket.hpp>
#include <sstream>

bool MongoLink::UploadMetric(const std::string &jsonPath) const
{
    std::cout << "[MongoLink] Uploading metric: " << jsonPath << std::endl;
    try
    {
        auto collection = db["MetricsData"];
        std::ifstream ifs(jsonPath);
        if (!ifs.is_open())
        {
            std::cerr << "Error opening JSON file for upload: " << jsonPath << std::endl;
            return false;
        }
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        bsoncxx::document::value doc = bsoncxx::from_json(buffer.str());
        collection.insert_one(doc.view());
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error uploading metric to MongoDB: " << e.what() << std::endl;
        return false;
    }
}

bool MongoLink::UploadVideo(const std::string &videoPath, const std::string &videoName) const
{
    std::cout << "[MongoLink] Uploading video: " << videoPath << " as " << videoName << std::endl;
    try
    {
        mongocxx::gridfs::bucket bucket = db.gridfs_bucket();
        std::ifstream ifs(videoPath, std::ios::binary);
        if (!ifs.is_open())
        {
            std::cerr << "Error opening video file for upload: " << videoPath << std::endl;
            return false;
        }

        auto uploader = bucket.open_upload_stream(videoName);
        const size_t bufferSize = 4096;
        std::vector<char> buffer(bufferSize);

        while (ifs)
        {
            ifs.read(reinterpret_cast<char *>(buffer.data()), bufferSize);
            std::streamsize bytesRead = ifs.gcount();
            if (bytesRead > 0)
            {
                uploader.write(reinterpret_cast<const uint8_t *>(buffer.data()),
                               static_cast<size_t>(bytesRead));
            }
        }

        uploader.close();
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error uploading video to MongoDB GridFS: " << e.what() << std::endl;
        return false;
    }
}

MongoLink::MongoLink()
{
    try
    {
        instance = std::make_unique<mongocxx::instance>();
        // Read MongoDB connection string from secrets.json
        std::ifstream secretsFile("build/Assets/Secrets/secrets.json");
        if (!secretsFile.is_open())
        {
            throw std::runtime_error("Could not open secrets.json for MongoDB connection string.");
        }
        std::stringstream secretsBuffer;
        secretsBuffer << secretsFile.rdbuf();
        auto secretsDoc = bsoncxx::from_json(secretsBuffer.str());
        auto connStrElem = secretsDoc.view()["MongoDBConnectionString"];
        if (!connStrElem || connStrElem.type() != bsoncxx::type::k_utf8)
        {
            throw std::runtime_error("MongoDBConnectionString not found or invalid in secrets.json.");
        }
        const auto uri = mongocxx::uri{connStrElem.get_utf8().value.to_string()};

        mongocxx::options::client clientOptions;
        const auto api =
            mongocxx::options::server_api(mongocxx::options::server_api::version::k_version_1);
        clientOptions.server_api_opts(api);

        client = std::make_unique<mongocxx::client>(uri, clientOptions);

        db = (*client)["MarbleMetrics"];

        // Ping the database.
        const auto ping_cmd =
            bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("ping", 1));
        db.run_command(ping_cmd.view());
        std::cout << "Pinged your deployment. You successfully connected to MongoDB!" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error initializing MongoDB client: " << e.what() << std::endl;
    }
}