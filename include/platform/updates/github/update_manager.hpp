#ifndef AKIRA_UPDATE_MANAGER_HPP
#define AKIRA_UPDATE_MANAGER_HPP

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>

#include "platform/updates/provider.hpp"

namespace akira::app
{
class AppSettings;
}

namespace akira::platform::github
{

class UpdateManager {
public:
    UpdateManager(const app::AppSettings& app, std::string_view repository);

    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;

    static int compareSemver(const std::string& a, const std::string& b);

    void setSelfPath(const std::string& path);
    std::string resolveInstallPath() const;

    UpdateInfo checkForUpdate(const std::string& channel);

    using ProgressCallback = UpdateProvider::ProgressCallback;

    std::string fetchExpectedSha256(const UpdateInfo& info);
    [[nodiscard]] std::expected<std::string, std::string> download(const UpdateInfo& info,
                                                                   const ProgressCallback& progress);
    [[nodiscard]] std::expected<void, std::string> verify(const std::string& path, const std::string& expectedSha256Hex,
                                                          int64_t expectedSize);
    [[nodiscard]] std::expected<void, std::string> applyDownloaded(const std::string& tempPath);

private:
    const app::AppSettings& appSettings;
    std::string releasesUrl;
    std::string selfPath;
};

}

#endif
