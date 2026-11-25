#include "util/http.hpp"
#include "log/log.hpp"

#include <curl/curl.h>
#include <switch.h>

#include <arpa/inet.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstring>
#include <mutex>

#include "util/curl_wrappers.hpp"

static std::atomic<unsigned long long> g_httpEpoch{0};
static std::atomic<bool> g_sonyBlocked{false};

void httpSetSonyBlocked(bool blocked)
{
    g_sonyBlocked.store(blocked, std::memory_order_relaxed);
}

bool httpSonyBlocked()
{
    return g_sonyBlocked.load(std::memory_order_relaxed);
}

static std::string urlHost(const std::string& url)
{
    std::size_t start = url.find("://");
    start = (start == std::string::npos) ? 0 : start + 3;
    std::size_t end = url.find_first_of("/?#", start);
    std::string host = url.substr(start, end == std::string::npos ? std::string::npos : end - start);

    std::size_t at = host.rfind('@');
    if (at != std::string::npos)
        host = host.substr(at + 1);
    std::size_t colon = host.rfind(':');
    if (colon != std::string::npos && host.find(']') == std::string::npos)
        host = host.substr(0, colon);

    std::transform(host.begin(), host.end(), host.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return host;
}

static bool isSonyHost(const std::string& url)
{
    static constexpr const char* SONY_DOMAINS[] = {
        "playstation.com",
        "playstation.net",
        "sony.com",
        "sonyentertainmentnetwork.com",
    };

    const std::string host = urlHost(url);
    for (const char* domain : SONY_DOMAINS)
    {
        const std::size_t len = std::char_traits<char>::length(domain);
        if (host.size() < len)
            continue;
        if (host.compare(host.size() - len, len, domain) != 0)
            continue;
        /* Suffix match only on a label boundary, so "notplaystation.com" does not hit. */
        if (host.size() == len || host[host.size() - len - 1] == '.')
            return true;
    }
    return false;
}

static std::string ipv4String(uint32_t value)
{
    if (value == 0)
        return "0.0.0.0";

    in_addr address{};
    address.s_addr = value;
    char text[INET_ADDRSTRLEN]{};
    return inet_ntop(AF_INET, &address, text, sizeof(text)) ? text : "?";
}

static std::string safeDiagnosticText(const char* value)
{
    if (!value || !value[0])
        return "-";

    std::string out(value);
    if (out.size() > 240)
        out.resize(240);
    for (char& c : out)
    {
        if (c == '\r' || c == '\n' || static_cast<unsigned char>(c) < 0x20)
            c = ' ';
    }

    // Curl's error buffer can include an effective URL. Hide query strings
    // so account tokens never enter production logs.
    size_t scheme = 0;
    while ((scheme = out.find("://", scheme)) != std::string::npos)
    {
        const size_t urlEnd = out.find_first_of(" \t\"'", scheme + 3);
        const size_t query = out.find('?', scheme + 3);
        if (query != std::string::npos && (urlEnd == std::string::npos || query < urlEnd))
        {
            const size_t queryEnd = urlEnd == std::string::npos ? out.size() : urlEnd;
            std::fill(out.begin() + query, out.begin() + queryEnd, '*');
        }
        scheme += 3;
    }
    return out;
}

static long long curlTimeUs(CURL* curl, CURLINFO info)
{
    curl_off_t value = -1;
    if (curl_easy_getinfo(curl, info, &value) != CURLE_OK)
        return -1;
    return static_cast<long long>(value);
}

static void logNetworkStateThrottled(const std::string& host)
{
    using Clock = std::chrono::steady_clock;
    static std::mutex mutex;
    static Clock::time_point lastLog;

    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto now = Clock::now();
        if (lastLog.time_since_epoch().count() != 0 && now - lastLog < std::chrono::seconds(5))
            return;
        lastLog = now;
    }

    const Result initRc = nifmInitialize(NifmServiceType_User);
    if (R_FAILED(initRc))
    {
        akira::log::warning("[NET] state host={} nifm_init=0x{:08x}", host,
            static_cast<unsigned int>(initRc));
        return;
    }

    NifmInternetConnectionType type = NifmInternetConnectionType_WiFi;
    NifmInternetConnectionStatus status = NifmInternetConnectionStatus_ConnectingUnknown1;
    uint32_t strength = 0;
    uint32_t address = 0;
    uint32_t mask = 0;
    uint32_t gateway = 0;
    uint32_t dns1 = 0;
    uint32_t dns2 = 0;

    const Result statusRc = nifmGetInternetConnectionStatus(&type, &strength, &status);
    const Result configRc = nifmGetCurrentIpConfigInfo(&address, &mask, &gateway, &dns1, &dns2);
    nifmExit();

    akira::log::warning(
        "[NET] state host={} status_rc=0x{:08x} config_rc=0x{:08x} type={} status={} strength={} "
        "ip={} mask={} gateway={} dns1={} dns2={}",
        host, static_cast<unsigned int>(statusRc), static_cast<unsigned int>(configRc),
        static_cast<int>(type), static_cast<int>(status), strength,
        ipv4String(address), ipv4String(mask), ipv4String(gateway),
        ipv4String(dns1), ipv4String(dns2));
}

static void logCurlFailure(CURL* curl, CURLcode result, const char* detail,
    const HttpRequest& request)
{
    long osErrno = 0;
    long responseCode = 0;
    long primaryPort = 0;
    long localPort = 0;
    long connections = 0;
    char* primaryIp = nullptr;
    char* localIp = nullptr;

    curl_easy_getinfo(curl, CURLINFO_OS_ERRNO, &osErrno);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &primaryIp);
    curl_easy_getinfo(curl, CURLINFO_PRIMARY_PORT, &primaryPort);
    curl_easy_getinfo(curl, CURLINFO_LOCAL_IP, &localIp);
    curl_easy_getinfo(curl, CURLINFO_LOCAL_PORT, &localPort);
    curl_easy_getinfo(curl, CURLINFO_NUM_CONNECTS, &connections);

    const std::string host = urlHost(request.url);
    const std::string osText = osErrno != 0
        ? safeDiagnosticText(std::strerror(static_cast<int>(osErrno))) : "-";
    akira::log::error(
        "[NET] curl_fail host={} code={} reason=\"{}\" detail=\"{}\" os_errno={} os_error=\"{}\" "
        "http={} primary={}:{} local={}:{} reused={} fresh={} new_connections={} "
        "dns_us={} tcp_us={} tls_us={} total_us={}",
        host, static_cast<int>(result), curl_easy_strerror(result), safeDiagnosticText(detail),
        osErrno, osText, responseCode, primaryIp ? primaryIp : "-", primaryPort,
        localIp ? localIp : "-", localPort, request.reuseHandle != nullptr,
        request.freshConnect, connections,
        curlTimeUs(curl, CURLINFO_NAMELOOKUP_TIME_T),
        curlTimeUs(curl, CURLINFO_CONNECT_TIME_T),
        curlTimeUs(curl, CURLINFO_APPCONNECT_TIME_T),
        curlTimeUs(curl, CURLINFO_TOTAL_TIME_T));
    logNetworkStateThrottled(host);
}

void httpMarkConnectionsStale()
{
    g_httpEpoch.fetch_add(1, std::memory_order_relaxed);
}

static std::array<std::mutex, CURL_LOCK_DATA_LAST> g_shareLocks;

static void curlShareLock(CURL*, curl_lock_data data, curl_lock_access, void*)
{
    g_shareLocks[data].lock();
}

static void curlShareUnlock(CURL*, curl_lock_data data, void*)
{
    g_shareLocks[data].unlock();
}

static CURLSH* sharedDnsCache()
{
    static CURLSH* share = []() -> CURLSH* {
        CURLSH* created = curl_share_init();
        if (!created)
            return nullptr;

        curl_share_setopt(created, CURLSHOPT_LOCKFUNC, curlShareLock);
        curl_share_setopt(created, CURLSHOPT_UNLOCKFUNC, curlShareUnlock);
        curl_share_setopt(created, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
        return created;
    }();

    return share;
}

static size_t curlWriteToString(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

static std::string trimHeaderValue(const std::string& value)
{
    size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

static size_t curlCollectHeader(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t total = size * nmemb;
    std::string line(static_cast<char*>(contents), total);

    size_t colon = line.find(':');
    if (colon != std::string::npos)
    {
        std::string name = trimHeaderValue(line.substr(0, colon));
        std::string value = trimHeaderValue(line.substr(colon + 1));
        if (!name.empty())
        {
            auto* headers = static_cast<std::vector<std::pair<std::string, std::string>>*>(userp);
            headers->emplace_back(std::move(name), std::move(value));
        }
    }

    return total;
}

std::string HttpResponse::header(const std::string& name) const
{
    auto equalsIgnoreCase = [](const std::string& a, const std::string& b) {
        return a.size() == b.size() &&
            std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
                return std::tolower(x) == std::tolower(y);
            });
    };

    for (const auto& entry : headers)
    {
        if (equalsIgnoreCase(entry.first, name))
            return entry.second;
    }

    return std::string();
}

HttpResponse httpPerform(const HttpRequest& request)
{
    HttpResponse response;

    if (httpSonyBlocked() && isSonyHost(request.url))
    {
        response.error = "blocked: legacy profile makes no PSN requests";
        return response;
    }

    CurlHandle ownHandle;
    CURL* curl = static_cast<CURL*>(request.reuseHandle);
    if (curl)
        curl_easy_reset(curl);
    else
        curl = ownHandle.handle;

    if (!curl)
    {
        response.error = "Failed to initialize CURL";
        return response;
    }

    char errorBuffer[CURL_ERROR_SIZE];
    errorBuffer[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

    CurlSlist headers;
    if (!request.bearer.empty())
    {
        std::string bearerHeader = "Authorization: Bearer " + request.bearer;
        headers.append(bearerHeader.c_str());
    }
    for (const std::string& header : request.headers)
    {
        headers.append(header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    if (static_cast<curl_slist*>(headers))
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, static_cast<curl_slist*>(headers));
    }
    if (!request.basicUser.empty())
    {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(curl, CURLOPT_USERNAME, request.basicUser.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, request.basicPassword.c_str());
    }
    if (request.post)
    {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.postFields.size()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.postFields.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlCollectHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    if (request.certInfo)
        curl_easy_setopt(curl, CURLOPT_CERTINFO, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, request.verifyPeer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, request.verifyPeer ? 2L : 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeoutSec);
    if (request.connectTimeoutSec > 0)
    {
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request.connectTimeoutSec);
    }

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, request.followLocation ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L);
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, request.freshConnect ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 1L);
    if (CURLSH* share = sharedDnsCache())
    {
        curl_easy_setopt(curl, CURLOPT_SHARE, share);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        logCurlFailure(curl, res, errorBuffer, request);
        response.error = errorBuffer[0] != '\0'
            ? std::string(errorBuffer)
            : std::string(curl_easy_strerror(res));
        response.body.clear();
        response.headers.clear();
        return response;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    return response;
}

HttpResponse httpGet(const std::string& url, const std::string& bearer, long timeoutSec)
{
    HttpRequest request;
    request.url = url;
    request.bearer = bearer;
    request.timeoutSec = timeoutSec;
    return httpPerform(request);
}

HttpSession::HttpSession()
    : handle(curl_easy_init())
{
}

HttpSession::~HttpSession()
{
    if (handle)
        curl_easy_cleanup(static_cast<CURL*>(handle));
}

HttpResponse HttpSession::perform(HttpRequest request)
{
    unsigned long long current = g_httpEpoch.load(std::memory_order_relaxed);
    if (epoch != current)
    {
        request.freshConnect = true;
        epoch = current;
    }
    request.reuseHandle = handle;
    return httpPerform(request);
}

HttpResponse HttpSession::get(const std::string& url, const std::string& bearer, long timeoutSec)
{
    HttpRequest request;
    request.url = url;
    request.bearer = bearer;
    request.timeoutSec = timeoutSec;
    return perform(request);
}
