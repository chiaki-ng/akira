#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>

#include "platform/updates/update_info.hpp"

namespace akira::platform
{

class UpdateProvider
{
public:
    using ProgressCallback = std::function<bool(int64_t received, int64_t total)>;

    virtual ~UpdateProvider() = default;

    virtual void prepare(int argc, char* argv[]) = 0;
    virtual std::span<const char* const> helpFiles() const = 0;

    virtual UpdateInfo checkForUpdate(const std::string& channel) = 0;
    virtual std::string fetchExpectedSha256(const UpdateInfo& info) = 0;
    virtual std::string download(const UpdateInfo& info, const ProgressCallback& progress, std::string& outError) = 0;
    virtual bool verify(const std::string& path, const std::string& expectedSha256Hex, int64_t expectedSize,
                        std::string& outError) = 0;
    virtual bool applyDownloaded(const std::string& tempPath, std::string& outError) = 0;
};

}
