#pragma once

#include "platform/updates/github/update_manager.hpp"
#include "platform/updates/provider.hpp"

namespace akira::platform::github
{

class GithubUpdateProvider final : public UpdateProvider
{
public:
    explicit GithubUpdateProvider(const app::AppSettings& app);

    void prepare(int argc, char* argv[]) override;
    std::span<const char* const> helpFiles() const override;

    UpdateInfo checkForUpdate(const std::string& channel) override;
    std::string fetchExpectedSha256(const UpdateInfo& info) override;
    std::expected<std::string, std::string> download(const UpdateInfo& info, const ProgressCallback& progress) override;
    std::expected<void, std::string> verify(const std::string& path, const std::string& expectedSha256Hex,
                                            int64_t expectedSize) override;
    std::expected<void, std::string> applyDownloaded(const std::string& tempPath) override;

private:
    UpdateManager m_manager;
};

}
