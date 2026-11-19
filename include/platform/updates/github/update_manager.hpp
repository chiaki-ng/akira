#ifndef AKIRA_UPDATE_MANAGER_HPP
#define AKIRA_UPDATE_MANAGER_HPP

#include <cstdint>
#include <functional>
#include <string>

namespace akira::app
{
class AppSettings;
}

namespace akira::platform::github
{

struct UpdateInfo {
    bool available = false;
    std::string version;
    std::string tag;
    std::string channel;
    std::string nroUrl;
    std::string sha256Url;
    std::string sha256;
    std::string notesUrl;
    std::string notes;
    int64_t size = 0;
    std::string error;
};

class UpdateManager {
public:
    explicit UpdateManager(const app::AppSettings& app);

    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;

    static int compareSemver(const std::string& a, const std::string& b);

    void setSelfPath(const std::string& path);
    std::string resolveInstallPath() const;

    UpdateInfo checkForUpdate(const std::string& channel);

    using ProgressCallback = std::function<bool(int64_t received, int64_t total)>;

    std::string fetchExpectedSha256(const UpdateInfo& info);
    std::string download(const UpdateInfo& info, const ProgressCallback& progress, std::string& outError);
    bool verify(const std::string& path, const std::string& expectedSha256Hex, int64_t expectedSize, std::string& outError);
    bool applyDownloaded(const std::string& tempPath, std::string& outError);

private:
    const app::AppSettings& appSettings;
    std::string selfPath;
};

}

#endif
