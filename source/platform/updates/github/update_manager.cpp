#include "platform/updates/github/update_manager.hpp"
#include "log/log.hpp"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

#include <curl/curl.h>

extern "C" {
#include <switch/runtime/devices/romfs_dev.h>
}

#include <nlohmann/json.hpp>

#include "app/app_settings.hpp"
#include "app/version.hpp"
#include "util/curl_wrappers.hpp"
#include "util/http.hpp"
#include "util/semver.hpp"
#include "util/sha256.hpp"

namespace akira::platform::github
{

namespace {

constexpr const char* kReleasesUrl =
    "https://api.github.com/repos/chiaki-ng/akira/releases?per_page=30";

constexpr const char* kDefaultInstallPath = "sdmc:/switch/akira/akira.nro";

struct ProgressCtx {
    const UpdateManager::ProgressCallback* cb;
    bool canceled;
};

size_t writeToFile(char* ptr, size_t size, size_t nmemb, void* stream) {
    return std::fwrite(ptr, size, nmemb, static_cast<FILE*>(stream));
}

int xferTrampoline(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<ProgressCtx*>(p);
    if (ctx->cb && *ctx->cb) {
        if (!(*ctx->cb)(static_cast<int64_t>(dlnow), static_cast<int64_t>(dltotal))) {
            ctx->canceled = true;
            return 1;
        }
    }
    return 0;
}

std::string toLowerHex(const std::string& in) {
    std::string out = in;
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool endsWithNro(const std::string& p) {
    return p.size() >= 4 && toLowerHex(p.substr(p.size() - 4)) == ".nro";
}

}

UpdateManager::UpdateManager(const app::AppSettings& app)
    : appSettings(app)
{
}

int UpdateManager::compareSemver(const std::string& aStr, const std::string& bStr) {
    return akira::semver::compare(aStr, bStr);
}

UpdateInfo UpdateManager::checkForUpdate(const std::string& channel) {
    UpdateInfo result;
    result.channel = channel;

    HttpRequest req;
    req.url = kReleasesUrl;
    req.headers.push_back("User-Agent: akira-updater");
    req.headers.push_back("Accept: application/vnd.github+json");
    req.headers.push_back("X-GitHub-Api-Version: 2022-11-28");
    req.verifyPeer = true;
    req.timeoutSec = 20;

    HttpResponse res = httpPerform(req);
    if (!res.ok()) {
        result.error = res.error.empty() ? ("HTTP " + std::to_string(res.status)) : res.error;
        log::warning("Update check failed: {}", result.error);
        return result;
    }

    nlohmann::json arr;
    try {
        arr = nlohmann::json::parse(res.body);
    } catch (...) {
        result.error = "Failed to parse release list";
        return result;
    }
    if (!arr.is_array()) {
        result.error = "Unexpected release list";
        return result;
    }

    const std::string self = app::version::semver();
    bool wantRc = (channel == "rc");

    std::string bestVersion;
    const nlohmann::json* best = nullptr;

    for (const auto& rel : arr) {
        if (rel.value("draft", false))
            continue;
        bool pre = rel.value("prerelease", false);
        if (!wantRc && pre)
            continue;

        std::string tag = rel.value("tag_name", std::string());
        if (tag.empty())
            continue;

        std::string ver = tag[0] == 'v' ? tag.substr(1) : tag;
        if (!akira::semver::isValid(ver))
            continue;

        if (bestVersion.empty() || compareSemver(ver, bestVersion) > 0) {
            bestVersion = ver;
            best = &rel;
        }
    }

    if (!best) {
        result.error = "No releases found";
        return result;
    }

    result.version = bestVersion;
    result.tag = best->value("tag_name", std::string());
    result.notesUrl = best->value("html_url", std::string());
    result.notes = best->value("body", std::string());

    nlohmann::json assets = best->value("assets", nlohmann::json::array());
    for (const auto& asset : assets) {
        std::string name = asset.value("name", std::string());
        if (name == "akira.nro") {
            result.nroUrl = asset.value("browser_download_url", std::string());
            result.size = asset.value("size", static_cast<int64_t>(0));
        } else if (name == "SHA256SUMS") {
            result.sha256Url = asset.value("browser_download_url", std::string());
        }
    }

    result.available = compareSemver(bestVersion, self) > 0;
    log::info("Update check ({}): latest {} vs self {} -> {}",
                       channel, bestVersion, self, result.available ? "update available" : "up to date");
    return result;
}

void UpdateManager::setSelfPath(const std::string& path) {
    selfPath = path;
}

std::string UpdateManager::resolveInstallPath() const {
    if (endsWithNro(selfPath)) {
        if (selfPath.rfind("sdmc:", 0) == 0)
            return selfPath;
        if (!selfPath.empty() && selfPath[0] == '/')
            return "sdmc:" + selfPath;
        return selfPath;
    }

    std::string configured = appSettings.updateInstallPath();
    if (!configured.empty())
        return configured;

    return kDefaultInstallPath;
}

std::string UpdateManager::fetchExpectedSha256(const UpdateInfo& info) {
    if (info.sha256Url.empty())
        return "";

    HttpRequest req;
    req.url = info.sha256Url;
    req.headers.push_back("User-Agent: akira-updater");
    req.verifyPeer = true;
    req.timeoutSec = 20;

    HttpResponse res = httpPerform(req);
    if (!res.ok())
        return "";

    std::istringstream iss(res.body);
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream ls(line);
        std::string hash;
        std::string name;
        ls >> hash >> name;
        if (name.find("akira.nro") != std::string::npos)
            return toLowerHex(hash);
    }
    return "";
}

std::expected<std::string, std::string> UpdateManager::download(const UpdateInfo& info, const ProgressCallback& progress) {
    if (info.nroUrl.empty()) {
        return std::unexpected("No download URL");
    }

    std::string tmp = resolveInstallPath() + ".new";
    FILE* file = std::fopen(tmp.c_str(), "wb");
    if (!file) {
        return std::unexpected("Cannot open temp file");
    }

    CurlHandle curl;
    if (!curl) {
        std::fclose(file);
        std::remove(tmp.c_str());
        return std::unexpected("curl init failed");
    }

    ProgressCtx ctx{ &progress, false };
    char errbuf[CURL_ERROR_SIZE];
    errbuf[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, info.nroUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferTrampoline);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "akira-updater");

    CURLcode res = curl_easy_perform(curl);
    std::fclose(file);

    if (res != CURLE_OK) {
        std::remove(tmp.c_str());
        return std::unexpected(ctx.canceled ? "Canceled"
                                            : (errbuf[0] != '\0' ? std::string(errbuf) : std::string(curl_easy_strerror(res))));
    }

    return tmp;
}

std::expected<void, std::string> UpdateManager::verify(const std::string& path, const std::string& expectedSha256Hex, int64_t expectedSize) {
    if (expectedSha256Hex.empty()) {
        return std::unexpected("No checksum to verify against");
    }

    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return std::unexpected("Cannot open downloaded file");
    }

    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (expectedSize > 0 && size != expectedSize) {
        std::fclose(file);
        return std::unexpected("Size mismatch");
    }

    akira::sha256::Ctx state;
    akira::sha256::init(state);
    unsigned char buffer[65536];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        akira::sha256::update(state, buffer, n);
    std::fclose(file);

    unsigned char digest[32];
    akira::sha256::final(state, digest);
    std::string got = akira::sha256::toHex(digest);

    if (got != toLowerHex(expectedSha256Hex)) {
        return std::unexpected("Checksum mismatch");
    }
    return {};
}

std::expected<void, std::string> UpdateManager::applyDownloaded(const std::string& tempPath) {
    std::string target = resolveInstallPath();
    std::string bak = target + ".bak";
    log::info("Update apply: self='{}' target='{}' temp='{}'", selfPath, target, tempPath);

    bool hadTarget = false;
    if (FILE* t = std::fopen(target.c_str(), "rb")) {
        hadTarget = true;
        std::fclose(t);
    }

    romfsExit();

    bool haveBak = false;
    if (hadTarget) {
        std::remove(bak.c_str());
        if (std::rename(target.c_str(), bak.c_str()) == 0) {
            haveBak = true;
        } else {
            log::warning("Update: backup rename failed ({}), removing target", std::strerror(errno));
            std::remove(target.c_str());
        }
    }

    if (std::rename(tempPath.c_str(), target.c_str()) != 0) {
        std::string failure = std::string("rename failed: ") + std::strerror(errno);
        if (haveBak)
            std::rename(bak.c_str(), target.c_str());
        std::remove(tempPath.c_str());
        romfsInit();
        return std::unexpected(std::move(failure));
    }

    log::info("Update applied to {}{}", target, haveBak ? " (backup at " + bak + ")" : "");
    return {};
}

}
