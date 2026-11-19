#pragma once

#include "platform/updates/provider.hpp"

namespace akira::platform::none
{

class NoUpdateProvider final : public UpdateProvider
{
public:
    void prepare(int, char*[]) override {}
    std::span<const char* const> helpFiles() const override { return {}; }

    UpdateInfo checkForUpdate(const std::string&) override { return { .error = "updates are not supplied" }; }
    std::string fetchExpectedSha256(const UpdateInfo&) override { return {}; }
    std::string download(const UpdateInfo&, const ProgressCallback&, std::string& outError) override
    {
        outError = "updates are not supplied";
        return {};
    }
    bool verify(const std::string&, const std::string&, int64_t, std::string& outError) override
    {
        outError = "updates are not supplied";
        return false;
    }
    bool applyDownloaded(const std::string&, std::string& outError) override
    {
        outError = "updates are not supplied";
        return false;
    }
};

}
