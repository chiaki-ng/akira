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
    std::expected<std::string, std::string> download(const UpdateInfo&, const ProgressCallback&) override
    {
        return std::unexpected("updates are not supplied");
    }
    std::expected<void, std::string> verify(const std::string&, const std::string&, int64_t) override
    {
        return std::unexpected("updates are not supplied");
    }
    std::expected<void, std::string> applyDownloaded(const std::string&) override
    {
        return std::unexpected("updates are not supplied");
    }
};

}
