#include "platform/updates/github/provider.hpp"

#include <array>

namespace akira::platform::github
{

GithubUpdateProvider::GithubUpdateProvider(const app::AppSettings& app)
    : m_manager(app)
{
}

void GithubUpdateProvider::prepare(int argc, char* argv[])
{
    m_manager.setSelfPath(argc > 0 && argv[0] ? argv[0] : "");
}

std::span<const char* const> GithubUpdateProvider::helpFiles() const
{
    static constexpr auto files = std::to_array<const char*>({"updates"});
    return files;
}

UpdateInfo GithubUpdateProvider::checkForUpdate(const std::string& channel)
{
    return m_manager.checkForUpdate(channel);
}

std::string GithubUpdateProvider::fetchExpectedSha256(const UpdateInfo& info)
{
    return m_manager.fetchExpectedSha256(info);
}

std::string GithubUpdateProvider::download(const UpdateInfo& info, const ProgressCallback& progress,
                                           std::string& outError)
{
    return m_manager.download(info, progress, outError);
}

bool GithubUpdateProvider::verify(const std::string& path, const std::string& expectedSha256Hex,
                                  int64_t expectedSize, std::string& outError)
{
    return m_manager.verify(path, expectedSha256Hex, expectedSize, outError);
}

bool GithubUpdateProvider::applyDownloaded(const std::string& tempPath, std::string& outError)
{
    return m_manager.applyDownloaded(tempPath, outError);
}

}
