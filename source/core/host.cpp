#include "core/host.hpp"
#include "stream/session.hpp"
#include "core/exception.hpp"
#include "core/settings_manager.hpp"
#include "core/wireguard_manager.hpp"
#include "core/discovery_manager.hpp"
#include "input/pad_path.hpp"
#include "input/rumble_profile.hpp"
#include "input/sample_cadence.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <algorithm>
#include <cstring>
#include <format>
#include <thread>
#include <chrono>

#include <chiaki/base64.h>


static akira::input::RumbleProfile ActivePadProfile()
{
    auto* settings = SettingsManager::getInstance();
    const std::vector<akira::input::PadDescription> pads = akira::input::DescribePads();

    if (pads.empty())
        return settings->getRumbleProfile(akira::input::kRumbleKeyDefault);

    const akira::input::PadDescription& pad = pads.front();
    const bool switchNative = pad.kind == akira::input::PadPathKind::JoyCon
                           || pad.kind == akira::input::PadPathKind::SwitchPro;

    return settings->resolveRumbleProfile(pad.vendor_id, pad.product_id,
                                          pad.has_address ? pad.bt_addr : nullptr,
                                          switchNative,
                                          pad.kind == akira::input::PadPathKind::JoyCon);
}

static void InitAudioCallback(unsigned int channels, unsigned int rate, void* user)
{
    Session* session = static_cast<Session*>(user);
    session->InitAudioCB(channels, rate);
}

static bool VideoCallback(uint8_t* buf, size_t buf_size, int32_t frames_lost, bool frame_recovered, void* user)
{
    Session* session = static_cast<Session*>(user);
    return session->VideoCB(buf, buf_size, frames_lost, frame_recovered, user);
}

static void AudioCallback(int16_t* buf, size_t samples_count, void* user)
{
    Session* session = static_cast<Session*>(user);
    session->AudioCB(buf, samples_count);
}

static void HapticsFrameCallback(uint8_t* buf, size_t buf_size, void* user)
{
    Session* session = static_cast<Session*>(user);
    session->HapticCB(buf, buf_size);
}

static void EventCallback(ChiakiEvent* event, void* user)
{
    Host* host = static_cast<Host*>(user);
    host->connectionEventCallback(event);
}

static void RegistEventCallback(ChiakiRegistEvent* event, void* user)
{
    Host* host = static_cast<Host*>(user);
    host->registCallback(event);
}

Host::Host(const std::string& name)
    : hostName(name), consoleModel(ConsoleModel::PS5Fat)
{
    settings = SettingsManager::getInstance();
    log = settings->getLogger();
    memset(&session, 0, sizeof(session));
    memset(&opusDecoder, 0, sizeof(opusDecoder));
    memset(&regist, 0, sizeof(regist));
    memset(&registInfo, 0, sizeof(registInfo));
    memset(&controllerState, 0, sizeof(controllerState));
    memset(&videoProfile, 0, sizeof(videoProfile));
}

Host::~Host()
{
    stopFeedbackLoop();
    if (sessionInit)
    {
        finiSession();
    }
}

bool Host::isPS5() const
{
    return target >= CHIAKI_TARGET_PS5_UNKNOWN;
}

void Host::setLoginPIN(const std::string& pin)
{
    brls::Logger::info("Host::setLoginPIN called with pin length={}", pin.length());

    if (!sessionInit)
    {
        brls::Logger::error("setLoginPIN: session not initialized!");
        return;
    }

    brls::Logger::info("setLoginPIN: calling chiaki_session_set_login_pin...");

    chiaki_session_set_login_pin(&session,
        reinterpret_cast<const uint8_t*>(pin.c_str()),
        pin.length());

    brls::Logger::info("setLoginPIN: chiaki_session_set_login_pin completed");
}

std::string Host::getStateString() const
{
    switch (state)
    {
        case CHIAKI_DISCOVERY_HOST_STATE_READY:
            return brls::getStr("akira/hosts/state_ready");
        case CHIAKI_DISCOVERY_HOST_STATE_STANDBY:
            return brls::getStr("akira/hosts/state_standby");
        default:
            return brls::getStr("akira/hosts/state_unknown");
    }
}

std::string Host::getTargetString() const
{
    if (target >= CHIAKI_TARGET_PS5_UNKNOWN)
    {
        return "PS5";
    }
    else if (target >= CHIAKI_TARGET_PS4_UNKNOWN)
    {
        return "PS4";
    }
    return "Unknown";
}

bool Host::getVideoResolution(int* width, int* height) const
{
    switch (videoResolution)
    {
        case CHIAKI_VIDEO_RESOLUTION_PRESET_360p:
            *width = 640;
            *height = 360;
            break;
        case CHIAKI_VIDEO_RESOLUTION_PRESET_540p:
            *width = 950;
            *height = 540;
            break;
        case CHIAKI_VIDEO_RESOLUTION_PRESET_720p:
            *width = 1280;
            *height = 720;
            break;
        case CHIAKI_VIDEO_RESOLUTION_PRESET_1080p:
            *width = 1920;
            *height = 1080;
            break;
        default:
            return false;
    }
    return true;
}

const Registration* Host::activeRegistration() const
{
    int64_t pid = settings ? settings->getActiveProfileId() : 0;
    for (const auto& reg : registrations)
        if (reg.profileId == pid)
            return &reg;
    return nullptr;
}

void Host::upsertRegistration(const Registration& reg)
{
    for (auto& existing : registrations)
    {
        if (existing.profileId == reg.profileId)
        {
            existing = reg;
            return;
        }
    }
    registrations.push_back(reg);
}

void Host::removeRegistrationsForProfile(int64_t profileId)
{
    std::erase_if(registrations,
        [profileId](const Registration& reg) { return reg.profileId == profileId; });
}

int Host::wakeup()
{
    const Registration* reg = activeRegistration();
    if (!reg)
    {
        brls::Logger::error("Host::wakeup: no registration for the active profile");
        return 2;
    }

    if (strlen(reg->rpRegistKey) > 8)
    {
        brls::Logger::error("Given registkey is too long");
        return 1;
    }
    else if (strlen(reg->rpRegistKey) <= 0)
    {
        brls::Logger::error("Given registkey is not defined");
        return 2;
    }

    uint64_t credential = strtoull(reg->rpRegistKey, NULL, 16);

    auto& wg = WireGuardManager::instance();
    if (wg.isConnected())
    {
        brls::Logger::info("Host::wakeup: sending via WireGuard tunnel");
        const char* version = isPS5() ? "00030010" : "00020020";
        uint16_t port = isPS5() ? 9302 : 987;

        auto buf = std::format(
            "WAKEUP * HTTP/1.1\n"
            "client-type:vr\n"
            "auth-type:R\n"
            "model:w\n"
            "app-type:r\n"
            "user-credential:{}\n"
            "device-discovery-protocol-version:{}\n",
            credential, version);

        int result = wg.sendUdpPacket(hostAddr, port, buf.data(), buf.size() + 1);
        brls::Logger::info("Host::wakeup: sendUdpPacket returned {}", result);
        return result >= 0 ? 0 : 1;
    }

    ChiakiDiscovery* live = DiscoveryManager::getInstance()->getLiveDiscovery(
        hostAddr.find(':') != std::string::npos);
    ChiakiErrorCode ret = chiaki_discovery_wakeup(log, live, hostAddr.c_str(), credential, isPS5());
    return ret;
}

void Host::applyRegistrationData(ChiakiRegisteredHost* regHost)
{
    if (!regHost) return;

    apSsid = regHost->ap_ssid;
    apKey = regHost->ap_key;
    apName = regHost->ap_name;
    serverNickname = regHost->server_nickname;
    target = regHost->target;

    Registration reg;
    reg.consoleId = consoleId;
    reg.profileId = settings ? settings->getActiveProfileId() : 0;
    memcpy(reg.serverMac, regHost->server_mac, sizeof(reg.serverMac));
    memcpy(reg.rpRegistKey, regHost->rp_regist_key, sizeof(reg.rpRegistKey));
    reg.rpKeyType = regHost->rp_key_type;
    memcpy(reg.rpKey, regHost->rp_key, sizeof(reg.rpKey));
    upsertRegistration(reg);

    brls::Logger::info("Applied registration data for {}", hostName);
}

void Host::copyRegistrationFrom(const Host* other)
{
    if (!other) return;

    const Registration* src = other->activeRegistration();
    if (!src) return;

    Registration reg = *src;
    reg.consoleId = consoleId;
    upsertRegistration(reg);

    consolePIN = other->consolePIN;

    brls::Logger::info("Copied registration data from '{}' to '{}'", other->hostName, hostName);
}

int Host::registerHost(int pin)
{
    std::string accountId = settings->getPsnAccountId(this);
    std::string onlineId = settings->getPsnOnlineId(this);
    size_t accountIdSize = sizeof(uint8_t[CHIAKI_PSN_ACCOUNT_ID_SIZE]);

    registInfo.target = target;
    registInfo.holepunch_info = NULL;
    registInfo.rudp = NULL;

    if (target >= CHIAKI_TARGET_PS4_9)
    {
        if (accountId.length() > 0)
        {
            chiaki_base64_decode(accountId.c_str(), accountId.length(),
                registInfo.psn_account_id, &accountIdSize);
            registInfo.psn_online_id = nullptr;
        }
        else
        {
            brls::Logger::error("Undefined PSN Account ID (Please configure a valid psn_account_id)");
            return HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID;
        }
    }
    else if (target > CHIAKI_TARGET_PS4_UNKNOWN)
    {
        if (onlineId.length() > 0)
        {
            psnOnlineId = onlineId;
            registInfo.psn_online_id = psnOnlineId.c_str();
        }
        else
        {
            brls::Logger::error("Undefined PSN Online ID (Please configure a valid psn_online_id)");
            return HOST_REGISTER_ERROR_SETTING_PSNONLINEID;
        }
    }
    else
    {
        brls::Logger::error("Undefined PS4 system version (please run discover first)");
        return HOST_REGISTER_ERROR_UNDEFINED_TARGET;
    }

    registInfo.pin = pin;
    registInfo.host = hostAddr.c_str();
    registInfo.broadcast = false;

    if (target >= CHIAKI_TARGET_PS4_9)
    {
        brls::Logger::info("Registering to host `{}` `{}` with PSN AccountID `{}` pin `{}`",
            hostName, hostAddr, settings->maskAccountName(accountId), pin);
    }
    else
    {
        brls::Logger::info("Registering to host `{}` `{}` with PSN OnlineID `{}` pin `{}`",
            hostName, hostAddr, settings->maskAccountName(onlineId), pin);
    }

    registInit = true;
    chiaki_regist_start(&regist, log, &registInfo, RegistEventCallback, this);
    return HOST_REGISTER_OK;
}

bool Host::canAutoRegister() const
{
    /*
     * Stated at the source rather than resting on "no token happens to be stored":
     * auto-registration is OAuth, then a PSN device lookup, then holepunch, all before a
     * packet reaches the console. Legacy registers by PIN.
     */
    if (settings->isLegacyProfileActive())
        return false;
    if (settings->getPsnAccessToken().empty())
        return false;
    if (settings->getPsnAccountId(const_cast<Host*>(this)).empty())
        return false;
    if (!isPS5() && target < CHIAKI_TARGET_PS4_9)
        return false;
    return true;
}

bool Host::resolveRemoteDuid()
{
    if (settings->isLegacyProfileActive())
    {
        brls::Logger::info("Legacy profile: skipping PSN device lookup");
        return false;
    }

    std::string accessToken = settings->getPsnAccessToken();
    if (accessToken.empty())
        return false;

    ChiakiHolepunchConsoleType type = isPS5() ?
        CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS5 : CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS4;

    ChiakiHolepunchDeviceInfo* devices = nullptr;
    size_t count = 0;
    ChiakiErrorCode err = chiaki_holepunch_list_devices(
        accessToken.c_str(), type, &devices, &count, log);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        brls::Logger::error("Auto-register: failed to list PSN devices: {}", chiaki_error_string(err));
        return false;
    }

    bool found = false;
    for (size_t i = 0; i < count; i++)
    {
        if (hostName == devices[i].device_name)
        {
            std::string duid;
            duid.reserve(64);
            for (size_t j = 0; j < 32; j++)
                std::format_to(std::back_inserter(duid), "{:02x}", devices[i].device_uid[j]);
            remoteDuid = duid;
            found = true;
            brls::Logger::info("Auto-register: resolved DUID for {} from PSN device list", hostName);
            break;
        }
    }

    chiaki_holepunch_free_device_list(&devices);
    return found;
}

int Host::registerHostAuto()
{
    std::string accountId = settings->getPsnAccountId(this);
    if (accountId.empty())
        return HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID;

    brls::Logger::info("Auto-register step 1: enter for {}", hostName);
    brls::Logger::flushAsyncLogs();

    if (onRegistStage)
        onRegistStage(2, 4);

    if (remoteDuid.empty())
    {
        brls::Logger::info("Auto-register step 2: resolving DUID via list_devices for {}", hostName);
        brls::Logger::flushAsyncLogs();
        if (!resolveRemoteDuid())
        {
            brls::Logger::error("Auto-register: {} not found in PSN device list", hostName);
            return HOST_REGISTER_ERROR_HOLEPUNCH;
        }
        brls::Logger::info("Auto-register step 2 done: DUID resolved for {}", hostName);
        brls::Logger::flushAsyncLogs();
    }

    if (target <= CHIAKI_TARGET_PS4_UNKNOWN)
        target = isPS5() ? CHIAKI_TARGET_PS5_1 : CHIAKI_TARGET_PS4_10;

    brls::Logger::info("Auto-register step 3: starting holepunch for {}", hostName);
    brls::Logger::flushAsyncLogs();

    if (onRegistStage)
        onRegistStage(3, 4);

    ChiakiErrorCode err = connectHolepunch();
    brls::Logger::info("Auto-register step 4: holepunch returned {}", chiaki_error_string(err));
    brls::Logger::flushAsyncLogs();
    if (err != CHIAKI_ERR_SUCCESS)
    {
        brls::Logger::error("Auto-register: holepunch failed: {}", chiaki_error_string(err));
        cleanupHolepunch();
        return HOST_REGISTER_ERROR_HOLEPUNCH;
    }

    chiaki_socket_t* ctrlSock = chiaki_get_holepunch_sock(holepunchSession, CHIAKI_HOLEPUNCH_PORT_TYPE_CTRL);
    if (!ctrlSock)
    {
        brls::Logger::error("Auto-register: no CTRL socket after holepunch");
        cleanupHolepunch();
        return HOST_REGISTER_ERROR_HOLEPUNCH;
    }

    autoRegistRudp = chiaki_rudp_init(ctrlSock, log);
    if (!autoRegistRudp)
    {
        brls::Logger::error("Auto-register: rudp init failed");
        cleanupHolepunch();
        return HOST_REGISTER_ERROR_HOLEPUNCH;
    }

    autoRegistHinfo = chiaki_get_regist_info(holepunchSession);

    memset(&registInfo, 0, sizeof(registInfo));
    registInfo.target = target;
    registInfo.holepunch_info = &autoRegistHinfo;
    registInfo.rudp = autoRegistRudp;
    registInfo.host = nullptr;
    registInfo.broadcast = false;
    registInfo.psn_online_id = nullptr;
    registInfo.pin = 0;
    registInfo.console_pin = 0;

    size_t accountIdSize = sizeof(registInfo.psn_account_id);
    chiaki_base64_decode(accountId.c_str(), accountId.length(), registInfo.psn_account_id, &accountIdSize);

    autoRegistActive = true;
    brls::Logger::info("Auto-register step 5: rudp + regist info ready, starting regist for {}", hostName);
    brls::Logger::flushAsyncLogs();

    if (onRegistStage)
        onRegistStage(4, 4);
    registInit = true;
    chiaki_regist_start(&regist, log, &registInfo, RegistEventCallback, this);
    return HOST_REGISTER_OK;
}

int Host::initSession(Session* streamSession)
{
    return initSessionWithHolepunch(streamSession, nullptr);
}

int Host::initSessionWithHolepunch(Session* streamSession, ChiakiHolepunchSession holepunch)
{
    bool cloud = isCloud();
    bool pscloud = cloud && cloudSession->serviceType == CHIAKI_SERVICE_TYPE_PSCLOUD;

    transientPs4Nicknames.clear();
    transientPs4NicknameMismatch.store(false);
    if (holepunch && usesTransientPsnRegistration())
    {
        auto* hostsMap = settings->getHostsMap();
        if (hostsMap)
        {
            for (const auto& entry : *hostsMap)
            {
                const auto& candidate = entry.second;
                if (!candidate || candidate.get() == this || candidate->isRemote() ||
                    candidate->isCloud() || candidate->isPS5() || !candidate->hasRpKey())
                    continue;

                // New registrations persist the server nickname. Host name is
                // the compatibility fallback for older configs, where local
                // discovery supplied that same nickname.
                transientPs4Nicknames.push_back(candidate->serverNickname.empty()
                    ? candidate->hostName
                    : candidate->serverNickname);
            }
        }

        if (transientPs4Nicknames.empty())
            throw Exception("Register your main PS4 locally before using PSN Remote Play");
    }

    ChiakiVideoResolutionPreset resolution;
    ChiakiVideoFPSPreset fps;
    if (cloud) {
        resolution = SettingsManager::intToResolution(settings->getCloudVideoResolution(pscloud));
        fps = CHIAKI_VIDEO_FPS_PRESET_60;
    } else {
        resolution = settings->getVideoResolution(this);
        fps = settings->getVideoFPS(this);
    }
    chiaki_connect_video_profile_preset(&videoProfile, resolution, fps);
    if (pscloud) {
        int cloudRes = settings->getCloudVideoResolution(pscloud);
        if (cloudRes >= 2160) {
            videoProfile.width = 3840;
            videoProfile.height = 2160;
        } else if (cloudRes >= 1440) {
            videoProfile.width = 2560;
            videoProfile.height = 1440;
        }
    }
    videoProfile.bitrate = cloud ? settings->getCloudVideoBitrate(pscloud) : settings->getVideoBitrate(this);

    chiaki_opus_decoder_init(&opusDecoder, log);

    ChiakiAudioSink audioSink = {};
    ChiakiAudioSink hapticsSink = {};
    hapticsSink.user = streamSession;
    hapticsSink.frame_cb = HapticsFrameCallback;

    ChiakiConnectInfo connectInfo = {};
    std::string effectiveHost = cloud && cloudSession ? cloudSession->host : hostAddr;

    auto& wg = WireGuardManager::instance();
    if (!cloud && wg.isConnected()) {
        resolution = settings->getVpnVideoResolution();
        fps = settings->getVpnVideoFPS();
        chiaki_connect_video_profile_preset(&videoProfile, resolution, fps);
        videoProfile.bitrate = settings->getVpnVideoBitrate();
        brls::Logger::info("Host::initSession: WireGuard connected, starting relays to {} (PS5:{})", hostAddr, isPS5());
        uint16_t tcpPort = wg.startTcpRelay(hostAddr, 9295, 9295);
        uint16_t udp9295Port = wg.startUdpRelay(hostAddr, 9295, 9295);
        uint16_t udp9296Port = wg.startUdpRelay(hostAddr, 9296, 9296);
        uint16_t udp9297Port = wg.startUdpRelay(hostAddr, 9297, 9297);
        bool success = tcpPort && udp9295Port && udp9296Port && udp9297Port;
        if (isPS5()) {
            uint16_t udp9302Port = wg.startUdpRelay(hostAddr, 9302, 9302);
            success = success && udp9302Port;
            brls::Logger::info("Host::initSession: PS5 relays TCP:{} UDP9295:{} UDP9296:{} UDP9297:{} UDP9302:{}", tcpPort, udp9295Port, udp9296Port, udp9297Port, udp9302Port);
        } else {
            brls::Logger::info("Host::initSession: PS4 relays TCP:{} UDP9295:{} UDP9296:{} UDP9297:{}", tcpPort, udp9295Port, udp9296Port, udp9297Port);
        }
        if (success) {
            effectiveHost = "127.0.0.1";
            brls::Logger::info("Host::initSession: All relays started, using {}", effectiveHost);
        } else {
            brls::Logger::error("Host::initSession: Failed to start some relays");
        }
    }

    brls::Logger::info("Host::initSession: videoResolution preset={}, profile={}x{}, bitrate={}",
        static_cast<int>(resolution), videoProfile.width, videoProfile.height, videoProfile.bitrate);

    connectInfo.host = effectiveHost.c_str();
    connectInfo.service_type = CHIAKI_SERVICE_TYPE_REMOTE_PLAY;
    connectInfo.video_profile = videoProfile;
    connectInfo.video_profile_auto_downgrade = true;
    connectInfo.enable_idr_on_fec_failure = settings->getRequestIdrOnFecFailure();
    connectInfo.takion_version_override = static_cast<unsigned int>(settings->getTakionVersion());
    connectInfo.packet_loss_max = settings->getPacketLossMax();
    connectInfo.holepunch_session = holepunch;

    memset(connectInfo.couch_account_id, 0, sizeof(connectInfo.couch_account_id));
    couchAccountLabels.fill(std::string{});
    couchProfileIds.fill(0);
    {
        const Profile* active = settings->getActiveProfile();
        if (active && !active->accountId.empty())
        {
            snprintf(connectInfo.couch_account_id[0], CHIAKI_COUCH_ACCOUNT_ID_SIZE, "%s", active->accountId.c_str());
            couchAccountLabels[0] = active->label();
            couchProfileIds[0] = active->id;
        }
        brls::Logger::info("Host::initSession: PS5 couch profiles will be selected explicitly per player");
    }

    if (holepunch)
    {
        std::string accountId = settings->getPsnAccountId(this);
        if (!accountId.empty())
        {
            size_t accountIdSize = CHIAKI_PSN_ACCOUNT_ID_SIZE;
            ChiakiErrorCode err = chiaki_base64_decode(
                accountId.c_str(), accountId.length(),
                connectInfo.psn_account_id, &accountIdSize);
            if (err == CHIAKI_ERR_SUCCESS && accountIdSize == CHIAKI_PSN_ACCOUNT_ID_SIZE)
            {
                brls::Logger::info("Host::initSession: PSN account ID set for holepunch");
            }
            else
            {
                brls::Logger::error("Host::initSession: Failed to decode PSN account ID");
            }
        }
        else
        {
            brls::Logger::warning("Host::initSession: No PSN account ID for holepunch session");
        }
    }

    if (isPS5())
    {
        connectInfo.video_profile.codec = CHIAKI_CODEC_H265;
    }

    streamSession->setRequestedProfile(
        connectInfo.video_profile.width,
        connectInfo.video_profile.height,
        connectInfo.video_profile.max_fps,
        connectInfo.video_profile.bitrate,
        connectInfo.video_profile.codec == CHIAKI_CODEC_H265
    );

    const akira::input::RumbleProfile padProfile = ActivePadProfile();

    connectInfo.enable_dualsense = true;
    streamSession->setRumbleStrength(padProfile.rumble_source == akira::input::RumbleSource::Off
                                         ? 0.0f
                                         : padProfile.strength);
    streamSession->setRumbleSource(padProfile.rumble_source);
    streamSession->setHapticBase(
        akira::input::HapticBaseForIntensity(padProfile.haptic_intensity));

    streamSession->setRumbleFreqs(padProfile.freq_low, padProfile.freq_high);
    streamSession->setEnvelopeAttack(padProfile.envelope_attack);
    streamSession->setEnvelopeDecay(padProfile.envelope_decay);
    streamSession->setRumbleCeiling(padProfile.ceiling);

    brls::Logger::info("Host::initSession: rumble source={} intensity={} strength={:.2f}"
                       " ceiling={:.2f} freq={:.0f}/{:.0f} envelope={:.2f}/{:.2f}",
                       (int)padProfile.rumble_source,
                       (int)padProfile.haptic_intensity,
                       padProfile.strength, padProfile.ceiling,
                       padProfile.freq_low, padProfile.freq_high,
                       padProfile.envelope_attack, padProfile.envelope_decay);

    connectInfo.ps5 = isPS5();

    if (!streamSession->InitAVCodec(isPS5(), connectInfo.video_profile.width, connectInfo.video_profile.height))
    {
        throw Exception("Failed to initiate libav codec");
    }

    if (!streamSession->InitVideo(connectInfo.video_profile.width, connectInfo.video_profile.height))
    {
        throw Exception("Failed to initiate video");
    }

    const char* platformLabel = isPS5() ? "PS5" : "PS4";
    if (cloud && cloudSession)
    {
        std::string server = cloudSession->host;
        if (cloudSession->port != 0)
            server += ":" + std::to_string(cloudSession->port);
        streamSession->setConnectionContext(std::format("Cloud \xc2\xb7 {}", server));
    }
    else if (holepunch || isRemote())
    {
        streamSession->setConnectionContext(std::format("{} \xc2\xb7 Remote", platformLabel));
    }
    else if (wg.isConnected())
    {
        streamSession->setConnectionContext(std::format("{} \xc2\xb7 VPN", platformLabel));
    }
    else
    {
        streamSession->setConnectionContext(std::format("{} \xc2\xb7 Direct", platformLabel));
    }

    if (cloud && cloudSession)
    {
        connectInfo.service_type = cloudSession->serviceType;
        connectInfo.cloud_launch_spec = cloudSession->launchSpec.c_str();
        connectInfo.cloud_handshake_key = cloudSession->handshakeKey.c_str();
        connectInfo.cloud_session_id = cloudSession->sessionId.c_str();
        connectInfo.cloud_port = cloudSession->port;
        connectInfo.cloud_psn_wrapper_type = cloudSession->psnWrapperType;
        connectInfo.cloud_mtu_in = cloudSession->mtuIn;
        connectInfo.cloud_mtu_out = cloudSession->mtuOut;
        connectInfo.cloud_rtt_us = cloudSession->rttUs;
    }
    else if (!holepunch)
    {
        const Registration* reg = activeRegistration();
        if (!reg)
        {
            throw Exception("No registration for the active profile");
        }
        memcpy(connectInfo.regist_key, reg->rpRegistKey, sizeof(connectInfo.regist_key));
        memcpy(connectInfo.morning, reg->rpKey, sizeof(connectInfo.morning));
    }

    ChiakiErrorCode err = chiaki_session_init(&session, &connectInfo, log);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        throw Exception(chiaki_error_string(err));
    }

    // check chiaki session.c:
    // chiaki_holepunch_session_fini(session->holepunch_session);
    // dont hold to it because we might double free it.
    if (holepunch)
    {
        holepunchSession = nullptr;
    }

    sessionInit = true;

    streamSession->setSession(&session);
    streamSession->startStreamTimer();

    brls::Logger::info("Host::initSession: Setting up callbacks...");
    chiaki_opus_decoder_set_cb(&opusDecoder, InitAudioCallback, AudioCallback, streamSession);
    chiaki_opus_decoder_get_sink(&opusDecoder, &audioSink);
    chiaki_session_set_audio_sink(&session, &audioSink);
    chiaki_session_set_haptics_sink(&session, &hapticsSink);
    brls::Logger::info("Host::initSession: Setting video callback, session={}", (void*)streamSession);
    chiaki_session_set_video_sample_cb(&session, VideoCallback, streamSession);
    chiaki_session_set_event_cb(&session, EventCallback, this);
    brls::Logger::info("Host::initSession: All callbacks set");

    chiaki_controller_state_set_idle(&controllerState);

    return 0;
}

int Host::finiSession()
{
    stopFeedbackLoop();
    if (sessionInit)
    {
        sessionInit = false;
        chiaki_session_join(&session);
        chiaki_session_fini(&session);
        chiaki_opus_decoder_fini(&opusDecoder);
    }
    return 0;
}

int Host::finiRegist()
{
    if (registInit)
    {
        registInit = false;
        chiaki_regist_stop(&regist);
        chiaki_regist_fini(&regist);
    }

    if (autoRegistActive)
    {
        autoRegistActive = false;
        if (autoRegistRudp)
        {
            chiaki_rudp_fini(autoRegistRudp);
            autoRegistRudp = nullptr;
        }
        cleanupHolepunch();
    }
    return 0;
}

void Host::stopSession()
{
    stopFeedbackLoop();
    if (sessionInit)
        chiaki_session_stop(&session);
}

bool Host::isSessionSocketHealthy() const
{
    if (!sessionInit)
        return false;
    chiaki_socket_t sock = session.stream_connection.takion.sock;
    if (CHIAKI_SOCKET_IS_INVALID(sock))
        return false;
    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        return false;
    return err == 0;
}

void Host::gotoBed()
{
    if (sessionInit) {
        brls::Logger::info("Sending go to bed command to console");
        chiaki_session_goto_bed(&session);
    }
}

void Host::startSession()
{
    brls::Logger::info("Starting chiaki session...");
    ChiakiErrorCode err = chiaki_session_start(&session);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        brls::Logger::error("chiaki_session_start failed with error: {}", chiaki_error_string(err));
        sessionInit = false;
        chiaki_session_fini(&session);
        throw Exception("Chiaki Session Start failed");
    }
    brls::Logger::info("chiaki_session_start returned SUCCESS");
    if (!startFeedbackLoop()) {
        chiaki_session_stop(&session);
        throw Exception("Failed to start input feedback loop");
    }
}

bool Host::startFeedbackLoop()
{
    bool expected = false;
    if (!feedbackLoopRunning.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
        return true;

    inputSuppressed.store(false, std::memory_order_release);

    try {
        feedbackThread = std::thread(&Host::feedbackLoop, this);
    } catch (const std::exception& e) {
        feedbackLoopRunning.store(false, std::memory_order_release);
        brls::Logger::error("Input feedback loop failed to start: {}", e.what());
        return false;
    }

    brls::Logger::info("Input feedback loop started independently of rendering at 120 Hz");
    return true;
}

void Host::stopFeedbackLoop()
{
    const bool wasRunning = feedbackLoopRunning.exchange(false, std::memory_order_acq_rel);
    feedbackLoopCv.notify_all();

    if (feedbackThread.joinable())
        feedbackThread.join();

    if (wasRunning)
        brls::Logger::info("Input feedback loop stopped");
}

void Host::feedbackLoop()
{
    using Clock = std::chrono::steady_clock;
    constexpr auto period = akira::input::InputPollPeriod;
    constexpr auto reportPeriod = std::chrono::seconds(10);

    auto next = Clock::now();
    auto reportStart = next;
    uint64_t iterations = 0;
    uint64_t lateIterations = 0;

    while (feedbackLoopRunning.load(std::memory_order_acquire)) {
        sendFeedbackState();
        iterations++;

        next += period;
        std::unique_lock<std::mutex> lock(feedbackLoopMutex);
        feedbackLoopCv.wait_until(lock, next, [this]() {
            return !feedbackLoopRunning.load(std::memory_order_acquire);
        });
        lock.unlock();

        if (!feedbackLoopRunning.load(std::memory_order_acquire))
            break;

        const auto now = Clock::now();
        if (now > next + period) {
            lateIterations++;
            next = now;
        }

        if (now - reportStart >= reportPeriod) {
            const double seconds = std::chrono::duration<double>(now - reportStart).count();
            brls::Logger::info(
                "Input feedback loop: actual_hz={:.2f} late_iterations={}",
                static_cast<double>(iterations) / seconds, lateIterations);
            reportStart = now;
            iterations = 0;
            lateIterations = 0;
        }
    }
}

void Host::sendFeedbackStateCouch()
{
    if (inputSuppressed.load(std::memory_order_acquire))
    {
        fingerIdTouchId.clear();
        for (uint8_t i = 0; i < couchPadCount; i++)
            chiaki_controller_state_set_idle(&controllerStates[i]);
    }
    else
    {
        if (onReadController)
            onReadController(&controllerStates[0], &fingerIdTouchId);

        uint8_t count = 1;
        if (onReadCouchSecondary)
            onReadCouchSecondary(controllerStates, &count);
        couchPadCount = count;
    }

    if (couchPadCount > 1)
        chiaki_session_set_pad_count(&session, couchPadCount);
    for (uint8_t i = 0; i < couchPadCount; i++)
    {
        chiaki_session_set_controller_state_for_pad(&session, i, &controllerStates[i]);
    }
}

void Host::couchAddPlayer(uint8_t slot, uint8_t deviceType)
{
    couchMode.store(true, std::memory_order_release);

    if (!sessionInit)
        return;
    if (slot == 0 || slot >= CHIAKI_COUCH_MAX_PADS)
        return;

    couchPadStates[slot] = CouchPadState::Joining;
    couchPadFailStatus[slot] = 0;
    couchPadJoinAt[slot] = std::chrono::steady_clock::now();

    ChiakiErrorCode err;
    if (isPS5())
    {
        brls::Logger::info("Host: requesting PS5 couch player {}", slot + 1);
        err = chiaki_session_couch_send_user_join(&session, slot);
    }
    else
    {
        brls::Logger::info("Host: opening couch pad for player {} (controller type {})", slot + 1, deviceType);
        err = chiaki_session_couch_join_ps4(&session, slot, deviceType);
    }

    if (err != CHIAKI_ERR_SUCCESS)
    {
        brls::Logger::error("Host: couch join for player {} could not be sent", slot + 1);
        couchPadStates[slot] = CouchPadState::Failed;
        couchPadFailStatus[slot] = 1;
        if (onPadJoinFailed)
            onPadJoinFailed(slot, 1);
    }
}

void Host::couchRemovePlayer(uint8_t slot)
{
    if (!couchMode.load(std::memory_order_acquire) || !sessionInit)
        return;
    if (slot == 0 || slot >= CHIAKI_COUCH_MAX_PADS)
        return;

    couchPadStates[slot] = CouchPadState::Idle;
    couchPadFailStatus[slot] = 0;
    couchAccountLabels[slot].clear();
    couchProfileIds[slot] = 0;

    brls::Logger::info("Host: closing couch pad for player {}", slot + 1);
    if (isPS5())
    {
        chiaki_session_couch_send_leave(&session, slot);
        chiaki_session_couch_close_pad(&session, slot);
    }
    else
    {
        chiaki_session_couch_leave_ps4(&session, slot);
    }
}

void Host::couchHandlePadDropped(uint8_t slot)
{
    if (slot == 0 || slot >= CHIAKI_COUCH_MAX_PADS)
        return;

    brls::Logger::info("Host: console dropped player {}, withdrawing the pad", slot + 1);
    couchPadStates[slot] = CouchPadState::Failed;
    couchPadFailStatus[slot] = kCouchPadDropped;

    if (sessionInit)
    {
        if (isPS5())
        {
            chiaki_session_couch_send_leave(&session, slot);
            chiaki_session_couch_close_pad(&session, slot);
        }
        else
        {
            chiaki_session_couch_leave_ps4(&session, slot);
        }
    }

    if (onPadJoinFailed)
        onPadJoinFailed(slot, kCouchPadDropped);
}

void Host::couchRetryPlayer(uint8_t slot)
{
    if (!sessionInit)
        return;
    if (slot == 0 || slot >= CHIAKI_COUCH_MAX_PADS)
        return;

    brls::Logger::info("Host: retrying couch join for player {}", slot + 1);
    chiaki_session_couch_send_leave(&session, slot);
    couchAddPlayer(slot);
}

std::vector<Host::CouchProfileChoice> Host::couchProfileChoices(uint8_t slot) const
{
    std::vector<CouchProfileChoice> choices;
    if (!isPS5() || !settings || slot == 0 || slot >= CHIAKI_COUCH_MAX_PADS)
        return choices;

    const int64_t activeProfileId = settings->getActiveProfileId();
    for (const Profile& profile : settings->getProfiles())
    {
        if (profile.id == activeProfileId || profile.accountId.empty())
            continue;

        if (couchPadStates[slot] == CouchPadState::NeedsProfile && couchProfileIds[slot] == profile.id)
            continue;

        bool assignedElsewhere = false;
        for (uint8_t other = 1; other < CHIAKI_COUCH_MAX_PADS; other++)
        {
            if (other != slot && couchProfileIds[other] == profile.id)
            {
                assignedElsewhere = true;
                break;
            }
        }
        if (!assignedElsewhere)
            choices.push_back({ profile.id, profile.label(), profile.accountId, profile.avatarUrl });
    }
    return choices;
}

bool Host::couchSelectProfile(uint8_t slot, int64_t profileId)
{
    if (!isPS5() || !sessionInit || !settings || slot == 0 || slot >= CHIAKI_COUCH_MAX_PADS)
        return false;

    const auto choices = couchProfileChoices(slot);
    const auto choice = std::find_if(choices.begin(), choices.end(),
        [profileId](const CouchProfileChoice& candidate) { return candidate.profileId == profileId; });
    const Profile* profile = settings->findProfile(profileId);
    if (choice == choices.end() || !profile)
        return false;

    if (chiaki_session_couch_set_account_id(&session, slot, profile->accountId.c_str()) != CHIAKI_ERR_SUCCESS)
    {
        brls::Logger::error("Host: selected couch profile has an invalid account id");
        return false;
    }

    couchProfileIds[slot] = profileId;
    couchAccountLabels[slot] = choice->label;
    brls::Logger::info("Host: selected Akira profile {} for couch player {}", profileId, slot + 1);
    return true;
}

void Host::couchTickJoinTimeouts()
{
    const auto now = std::chrono::steady_clock::now();
    for (uint8_t slot = 1; slot < CHIAKI_COUCH_MAX_PADS; slot++)
    {
        if (couchPadStates[slot] != CouchPadState::Joining)
            continue;
        const auto waited = std::chrono::duration_cast<std::chrono::seconds>(now - couchPadJoinAt[slot]).count();
        if (waited < kCouchJoinTimeoutSeconds)
            continue;

        brls::Logger::error("Host: couch join for player {} timed out after {}s", slot + 1, waited);
        couchPadStates[slot] = CouchPadState::Failed;
        couchPadFailStatus[slot] = kCouchJoinTimedOut;
        if (onPadJoinFailed)
            onPadJoinFailed(slot, kCouchJoinTimedOut);
    }
}

void Host::couchSendPasscode(uint8_t slot, const std::string& passcode)
{
    if (!sessionInit)
        return;
    if (slot == 0 || slot >= CHIAKI_COUCH_MAX_PADS)
        return;

    couchPadStates[slot] = CouchPadState::Joining;
    couchPadJoinAt[slot] = std::chrono::steady_clock::now();

    brls::Logger::info("Host: sending couch passcode for player {}", slot);
    chiaki_session_couch_send_passcode(&session, slot, passcode.c_str());
}

void Host::sendFeedbackState()
{
    if (couchMode.load(std::memory_order_acquire))
    {
        sendFeedbackStateCouch();
        return;
    }

    if (inputSuppressed.load(std::memory_order_acquire))
    {
        chiaki_controller_state_set_idle(&controllerState);
        fingerIdTouchId.clear();
    }
    else if (onReadController)
    {
        onReadController(&controllerState, &fingerIdTouchId);
    }
    static uint32_t prevButtons = 0;
    if (controllerState.buttons != prevButtons)
    {
        brls::Logger::info("Controller buttons changed: 0x{:x} -> 0x{:x}", prevButtons, controllerState.buttons);
        prevButtons = controllerState.buttons;
    }
    static int8_t prevTouchId0 = -1, prevTouchId1 = -1;
    static uint32_t moveCount = 0;
    bool idChanged = controllerState.touches[0].id != prevTouchId0 || controllerState.touches[1].id != prevTouchId1;
    bool anyActive = controllerState.touches[0].id >= 0 || controllerState.touches[1].id >= 0;

    if (idChanged)
    {
        brls::Logger::info("Controller touches: [0] id={} pos=({},{}), [1] id={} pos=({},{})",
            controllerState.touches[0].id, controllerState.touches[0].x, controllerState.touches[0].y,
            controllerState.touches[1].id, controllerState.touches[1].x, controllerState.touches[1].y);
        prevTouchId0 = controllerState.touches[0].id;
        prevTouchId1 = controllerState.touches[1].id;
    }
    chiaki_session_set_controller_state(&session, &controllerState);
}

void Host::setInputSuppressed(bool suppressed)
{
    inputSuppressed.store(suppressed, std::memory_order_release);
    feedbackLoopCv.notify_all();
}

void Host::connectionEventCallback(ChiakiEvent* event)
{
    if (SettingsManager::getInstance()->getDebugChiakiLog())
        brls::Logger::info("connectionEventCallback: event type={}", static_cast<int>(event->type));

    switch (event->type)
    {
        case CHIAKI_EVENT_CONNECTED:
            brls::Logger::info("EventCB CHIAKI_EVENT_CONNECTED");
            if (onConnected)
            {
                onConnected();
            }
            break;

        case CHIAKI_EVENT_LOGIN_PIN_REQUEST:
            brls::Logger::info("EventCB CHIAKI_EVENT_LOGIN_PIN_REQUEST");
            if (onLoginPinRequest)
            {
                onLoginPinRequest(event->login_pin_request.pin_incorrect);
            }
            break;

        case CHIAKI_EVENT_NICKNAME_RECEIVED:
        {
            if (!usesTransientPsnRegistration())
                break;

            const std::string nickname(event->server_nickname,
                strnlen(event->server_nickname, sizeof(event->server_nickname)));
            const bool registered = std::ranges::find(transientPs4Nicknames, nickname)
                != transientPs4Nicknames.end();
            if (!registered)
            {
                brls::Logger::error(
                    "PSN selected PS4 '{}' but it is not registered locally for the active profile",
                    nickname);
                transientPs4NicknameMismatch.store(true);
                stopSession();
            }
            else
            {
                brls::Logger::info("Transient PS4 registration matched locally registered console '{}'", nickname);
            }
            break;
        }

        case CHIAKI_EVENT_RUMBLE:
            if (SettingsManager::getInstance()->getDebugChiakiLog())
                brls::Logger::info("RUMBLE EVENT: left={}, right={}", event->rumble.left, event->rumble.right);
            if (onRumble)
            {
                onRumble(event->rumble.player_index, event->rumble.left, event->rumble.right);
            }
            break;

        case CHIAKI_EVENT_QUIT:
            brls::Logger::info("EventCB CHIAKI_EVENT_QUIT");
            if (onQuit)
            {
                onQuit(&event->quit);
            }
            break;

        case CHIAKI_EVENT_TRIGGER_EFFECTS:
            if (SettingsManager::getInstance()->getDebugChiakiLog())
                brls::Logger::info("TRIGGER EFFECTS EVENT: left type=0x{:02x}, right type=0x{:02x}",
                                   event->trigger_effects.type_left,
                                   event->trigger_effects.type_right);
            if (onTriggerEffects)
            {
                onTriggerEffects(&event->trigger_effects);
            }
            break;

        case CHIAKI_EVENT_HAPTIC_INTENSITY:
        case CHIAKI_EVENT_TRIGGER_INTENSITY:
            if (event->type == CHIAKI_EVENT_HAPTIC_INTENSITY)
                hapticIntensity = (uint8_t)event->intensity;
            else
                triggerIntensity = (uint8_t)event->intensity;

            brls::Logger::info("EventCB intensity: haptics={} triggers={}",
                               hapticIntensity, triggerIntensity);

            if (onEffectIntensity)
            {
                onEffectIntensity(hapticIntensity, triggerIntensity);
            }
            break;

        case CHIAKI_EVENT_LED_COLOR:
            brls::Logger::info("EventCB CHIAKI_EVENT_LED_COLOR: {:02x}{:02x}{:02x}",
                               event->led_state[0], event->led_state[1], event->led_state[2]);
            if (onLedColor)
            {
                onLedColor(event->led_state[0], event->led_state[1], event->led_state[2]);
            }
            break;

        case CHIAKI_EVENT_PAD_CONFIRMED:
            brls::Logger::info("EventCB CHIAKI_EVENT_PAD_CONFIRMED: player_index={} led={:02x}{:02x}{:02x}",
                               event->pad_confirmed.index, event->pad_confirmed.led[0],
                               event->pad_confirmed.led[1], event->pad_confirmed.led[2]);
            if (event->pad_confirmed.index < CHIAKI_COUCH_MAX_PADS)
            {
                couchPadStates[event->pad_confirmed.index] = CouchPadState::Confirmed;
                couchPadFailStatus[event->pad_confirmed.index] = 0;
            }
            if (onPadConfirmed)
            {
                onPadConfirmed(event->pad_confirmed.index, event->pad_confirmed.led[0],
                               event->pad_confirmed.led[1], event->pad_confirmed.led[2]);
            }
            break;

        case CHIAKI_EVENT_PAD_ANNOUNCED:
            brls::Logger::info("EventCB CHIAKI_EVENT_PAD_ANNOUNCED: player_index={}",
                               event->pad_confirmed.index);
            if (event->pad_confirmed.index < CHIAKI_COUCH_MAX_PADS)
            {
                couchPadStates[event->pad_confirmed.index] = CouchPadState::Announced;
            }
            if (onPadConfirmed)
            {
                onPadConfirmed(event->pad_confirmed.index, event->pad_confirmed.led[0],
                               event->pad_confirmed.led[1], event->pad_confirmed.led[2]);
            }
            break;

        case CHIAKI_EVENT_PAD_DROPPED:
            couchHandlePadDropped(event->pad_join_failed.index);
            break;

        case CHIAKI_EVENT_PAD_PASSCODE_REQUEST:
            brls::Logger::info("EventCB CHIAKI_EVENT_PAD_PASSCODE_REQUEST: player_index={} retry={}",
                               event->pad_passcode_request.index, event->pad_passcode_request.retry);
            if (event->pad_passcode_request.index < CHIAKI_COUCH_MAX_PADS)
            {
                couchPadStates[event->pad_passcode_request.index] = CouchPadState::Passcode;
            }
            if (onPadPasscodeRequest)
            {
                onPadPasscodeRequest(event->pad_passcode_request.index, event->pad_passcode_request.retry);
            }
            break;

        case CHIAKI_EVENT_PAD_AUTHORIZATION_REQUIRED:
            brls::Logger::info("EventCB CHIAKI_EVENT_PAD_AUTHORIZATION_REQUIRED: player_index={}",
                               event->pad_authorization_required.index);
            if (event->pad_authorization_required.index < CHIAKI_COUCH_MAX_PADS)
            {
                const uint8_t slot = event->pad_authorization_required.index;
                couchPadStates[slot] = CouchPadState::NeedsProfile;
                couchPadFailStatus[slot] = 0;
            }
            break;

        case CHIAKI_EVENT_PAD_JOIN_FAILED:
            brls::Logger::error("EventCB CHIAKI_EVENT_PAD_JOIN_FAILED: player_index={} status={}",
                                event->pad_join_failed.index, event->pad_join_failed.status);
            if (event->pad_join_failed.index < CHIAKI_COUCH_MAX_PADS)
            {
                couchPadStates[event->pad_join_failed.index] = CouchPadState::Failed;
                couchPadFailStatus[event->pad_join_failed.index] = event->pad_join_failed.status;
            }
            if (onPadJoinFailed)
            {
                onPadJoinFailed(event->pad_join_failed.index, event->pad_join_failed.status);
            }
            break;

        case CHIAKI_EVENT_MOTION_RESET:
            brls::Logger::info("EventCB CHIAKI_EVENT_MOTION_RESET");
            if (onMotionReset)
            {
                onMotionReset();
            }
            break;
    }
}

void Host::registCallback(ChiakiRegistEvent* event)
{
    switch (event->type)
    {
        case CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED:
            brls::Logger::info("Register event CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED");
            if (onRegistCanceled)
            {
                onRegistCanceled();
            }
            break;

        case CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED:
            brls::Logger::info("Register event CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED");
            if (onRegistFailed)
            {
                onRegistFailed();
            }
            break;

        case CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS:
        {
            ChiakiRegisteredHost* rHost = event->registered_host;
            brls::Logger::info("Register event CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS");

            apSsid = rHost->ap_ssid;
            apKey = rHost->ap_key;
            apName = rHost->ap_name;
            serverNickname = rHost->server_nickname;

            Registration reg;
            reg.consoleId = consoleId;
            reg.profileId = settings ? settings->getActiveProfileId() : 0;
            memcpy(reg.serverMac, rHost->server_mac, sizeof(reg.serverMac));
            memcpy(reg.rpRegistKey, rHost->rp_regist_key, sizeof(reg.rpRegistKey));
            reg.rpKeyType = rHost->rp_key_type;
            memcpy(reg.rpKey, rHost->rp_key, sizeof(reg.rpKey));
            upsertRegistration(reg);

            brls::Logger::info("Register Success {}", hostName);

            if (onRegistSuccess)
            {
                onRegistSuccess();
            }
            break;
        }
    }

}

ChiakiErrorCode Host::initHolepunchSession()
{
    if (holepunchSession)
    {
        brls::Logger::warning("Holepunch session already initialized");
        return CHIAKI_ERR_SUCCESS;
    }

    /*
     * chiaki's holepunch has its own curl and never passes through httpPerform, so the
     * Sony gate there does not cover it. Guard the call sites instead.
     */
    if (settings->isLegacyProfileActive())
    {
        brls::Logger::error("Legacy profile: holepunch is not available");
        return CHIAKI_ERR_INVALID_DATA;
    }

    std::string accessToken = settings->getPsnAccessToken();
    if (accessToken.empty())
    {
        brls::Logger::error("No PSN access token available for holepunch");
        return CHIAKI_ERR_INVALID_DATA;
    }

    holepunchSession = chiaki_holepunch_session_init(accessToken.c_str(), log);
    if (!holepunchSession)
    {
        brls::Logger::error("Failed to initialize holepunch session");
        return CHIAKI_ERR_MEMORY;
    }

    if (settings->getPortGuessing())
    {
        chiaki_holepunch_session_force_port_guessing(holepunchSession, true);
        chiaki_holepunch_session_set_port_guessing_ports(holepunchSession, settings->getPortGuessingCount());
        chiaki_holepunch_session_set_port_guessing_socks(holepunchSession, settings->getPortGuessingSocks());
        brls::Logger::info("Port guessing enabled: {} guesses, {} sockets", settings->getPortGuessingCount(), settings->getPortGuessingSocks());
    }

    const std::string forcedWsFqdn = settings->getDevForceWsFqdn();
    if (!forcedWsFqdn.empty())
    {
        chiaki_holepunch_session_set_ws_fqdn_override(holepunchSession, forcedWsFqdn.c_str());
        brls::Logger::warning("Forcing push notification node to {}", forcedWsFqdn);
    }

    brls::Logger::info("Holepunch session initialized for {}", hostName);
    return CHIAKI_ERR_SUCCESS;
}

ChiakiErrorCode Host::connectHolepunch()
{
    if (holepunchSession)
    {
        brls::Logger::warning("Discarding stale holepunch session before new attempt");
        cleanupHolepunch();
    }

    bool retryEnabled = SettingsManager::getInstance()->getHolepunchRetry();
    const int maxRetries = retryEnabled ? 4 : 1;
    ChiakiErrorCode err = CHIAKI_ERR_UNKNOWN;

    if (remoteDuid.empty())
    {
        brls::Logger::error("No remote DUID available for holepunch");
        return CHIAKI_ERR_INVALID_DATA;
    }

    size_t duidLen = remoteDuid.size();
    size_t duidBytesLen = duidLen / 2;
    uint8_t duidBytes[32] = {0};

    for (size_t i = 0; i < duidBytesLen && i < 32; i++)
    {
        unsigned int byte;
        if (sscanf(remoteDuid.c_str() + (i * 2), "%02x", &byte) == 1)
        {
            duidBytes[i] = static_cast<uint8_t>(byte);
        }
    }

    ChiakiHolepunchConsoleType consoleType = isPS5() ?
        CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS5 : CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS4;

    for (int attempt = 1; attempt <= maxRetries; attempt++)
    {
        if (!holepunchSession)
        {
            err = initHolepunchSession();
            if (err != CHIAKI_ERR_SUCCESS)
                return err;
        }

        brls::Logger::info("Starting holepunch connection sequence for {} ({}) - attempt {}/{}",
            hostName, isPS5() ? "PS5" : "PS4", attempt, maxRetries);

        if (onHolepunchPhase)
            onHolepunchPhase(HolepunchPhase::Negotiating);

        err = chiaki_holepunch_upnp_discover(holepunchSession);
        if (err != CHIAKI_ERR_SUCCESS)
        {
            brls::Logger::warning("UPNP discovery failed (non-fatal): {}", chiaki_error_string(err));
        }
        else
        {
            brls::Logger::info("UPNP discovery completed");
        }

        brls::Logger::info("Creating holepunch session on PSN...");
        err = chiaki_holepunch_session_create(holepunchSession);
        if (err != CHIAKI_ERR_SUCCESS)
        {
            brls::Logger::error("Failed to create holepunch session: {}", chiaki_error_string(err));
            return err;
        }
        brls::Logger::info("Holepunch session created on PSN");

        brls::Logger::info("Creating CTRL offer...");
        err = holepunch_session_create_offer(holepunchSession);
        if (err != CHIAKI_ERR_SUCCESS)
        {
            brls::Logger::error("Failed to create CTRL offer: {}", chiaki_error_string(err));
            return err;
        }
        brls::Logger::info("CTRL offer created");

        brls::Logger::info("Starting holepunch session for device...");
        err = chiaki_holepunch_session_start(holepunchSession, duidBytes, consoleType);
        if (err != CHIAKI_ERR_SUCCESS)
        {
            brls::Logger::error("Failed to start holepunch session: {}", chiaki_error_string(err));
            return err;
        }
        brls::Logger::info("Holepunch session started for device");

        brls::Logger::info("Punching CTRL hole...");
        if (onHolepunchPhase)
            onHolepunchPhase(HolepunchPhase::Punching);
        err = chiaki_holepunch_session_punch_hole(holepunchSession, CHIAKI_HOLEPUNCH_PORT_TYPE_CTRL);
        if (err == CHIAKI_ERR_SUCCESS)
        {
            brls::Logger::info("CTRL hole punched successfully!");
            return CHIAKI_ERR_SUCCESS;
        }

        brls::Logger::error("Failed to punch CTRL hole: {}", chiaki_error_string(err));

        if (err == CHIAKI_ERR_HOST_UNREACH && attempt < maxRetries)
        {
            int delaySeconds = attempt * 3;
            brls::Logger::warning("Holepunch attempt {}/{} failed, retrying in {} seconds...", attempt, maxRetries, delaySeconds);
            cleanupHolepunch();
            std::this_thread::sleep_for(std::chrono::seconds(delaySeconds));
            continue;
        }

        break;
    }

    return err;
}

void Host::cancelHolepunch()
{
    if (holepunchSession)
    {
        brls::Logger::info("Canceling holepunch session");
        chiaki_holepunch_main_thread_cancel(holepunchSession, true);
    }
}

void Host::cleanupHolepunch()
{
    if (holepunchSession)
    {
        brls::Logger::info("Cleaning up holepunch session");
        chiaki_holepunch_session_fini(holepunchSession);
        holepunchSession = nullptr;
    }
}
