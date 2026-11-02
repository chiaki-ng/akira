/*
 * Akira - PlayStation Remote Play for Nintendo Switch
 * Built with Borealis UI Framework
 */

#include <switch.h>
#include <borealis.hpp>
#include <borealis/core/thread_pool.hpp>
#include <borealis/views/hint.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/views/widgets/battery.hpp>
#include <borealis/views/widgets/wireless.hpp>
#include <SDL2/SDL.h>
#include <arpa/inet.h>
#include <atomic>
#include <array>
#include <fstream>
#include <string_view>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <memory>

#include <curl/curl.h>
#include <optional>
#include "ui/theme.hpp"
#include "util/app_log.hpp"
#include "util/http.hpp"
#include "util/http_pool.hpp"
#include "util/sony_dns_checker.hpp"

#include "views/host_list_tab.hpp"
#include "views/vendored/switchfin/recycling_grid.hpp"
#include "views/psn_gated_box.hpp"
#include "views/trophy_list_tab.hpp"
#include "views/add_host_tab.hpp"
#include "views/build_info_tab.hpp"
#include "views/config_view_tab.hpp"
#include "views/network_utilities_tab.hpp"
#include "views/stream_view.hpp"
#include "views/enter_pin_view.hpp"
#include "views/settings_frame_view.hpp"
#include "views/profile_setup_view.hpp"
#include "stream/session.hpp"
#include "core/settings_manager.hpp"
#include "core/chiaki_core.hpp"
#include "core/wireguard_manager.hpp"
#include "core/update_manager.hpp"
#include "core/discovery_manager.hpp"
#include "ui/akira_header.hpp"
#include "views/update_flow.hpp"

#include <borealis/core/i18n.hpp>
using namespace brls::literals;

extern "C" {
#include <libavutil/log.h>
}

static void ffmpeg_log_callback(void*, int level, const char* fmt, va_list vl) {
    if (SettingsManager::getInstance()->getSilenceAllLogs() || !SettingsManager::getInstance()->getDebugFfmpegLog())
        return;
    if (level > av_log_get_level())
        return;
    std::array<char, 512> buf{};
    vsnprintf(buf.data(), buf.size(), fmt, vl);
    std::string_view msg(buf.data());
    if (msg.ends_with('\n'))
        msg.remove_suffix(1);
    if (msg.empty())
        return;
    if (level <= AV_LOG_ERROR)
        brls::Logger::error("ffmpeg: {}", msg);
    else if (level <= AV_LOG_WARNING)
        brls::Logger::warning("ffmpeg: {}", msg);
    else
        brls::Logger::info("ffmpeg: {}", msg);
}

static std::string getLocalIpAddress() {
    u32 ip = 0;
    Result rc = nifmGetCurrentIpAddress(&ip);
    if (R_SUCCEEDED(rc) && ip != 0) {
        struct in_addr addr;
        addr.s_addr = ip;
        return std::string(inet_ntoa(addr));
    }
    return "Not connected";
}

static std::string getAppVersion() {
    std::ifstream file("romfs:/build_info.txt");
    if (!file.is_open()) {
        return "";
    }
    std::string line;
    if (std::getline(file, line)) {
        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            return "v" + line.substr(spacePos + 1);
        }
    }
    return "";
}

// Borealis dismisses a Dialog before invoking its button callback. Disable
// the actual A/touch action until the deadline, not merely the button style.
class DelayedDnsWarningDialog final : public brls::Dialog {
    struct LiveState {
        DelayedDnsWarningDialog* dialog = nullptr;
    };

    std::shared_ptr<LiveState> liveState = std::make_shared<LiveState>();
    std::chrono::steady_clock::time_point openedAt;
    int lastRemaining = -1;

    int remainingSeconds() const {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - openedAt).count();
        return akira::net::secondsUntilDnsWarningOk(elapsed);
    }

    void updateCountdown() {
        const int remaining = remainingSeconds();
        if (remaining == lastRemaining)
            return;
        lastRemaining = remaining;
        button1->setText(remaining > 0
            ? brls::getStr("akira/network/dns_ok_in", remaining)
            : brls::getStr("akira/common/ok"));
        button1->setState(remaining > 0
            ? brls::ButtonState::DISABLED : brls::ButtonState::ENABLED);
        button1->setActionAvailable(brls::BUTTON_A, remaining == 0);
    }

public:
    explicit DelayedDnsWarningDialog(std::string message)
        : brls::Dialog(std::move(message)) {
        liveState->dialog = this;
        setCancelable(false);
        addButton(brls::getStr("akira/common/ok"), [] {});
        button1->setActionAvailable(brls::BUTTON_A, false);
        button1->setState(brls::ButtonState::DISABLED);
    }

    ~DelayedDnsWarningDialog() override { liveState->dialog = nullptr; }

    void show() {
        openedAt = std::chrono::steady_clock::now();
        updateCountdown();
        const std::weak_ptr<LiveState> weakState = liveState;
        brls::Application::getRunLoopEvent()->subscribe([weakState] {
            if (const auto state = weakState.lock(); state && state->dialog)
                state->dialog->updateCountdown();
        });
        open();
    }
};

static void startSonyDnsStartupCheck() {
    if (SettingsManager::getInstance()->isLegacyProfileActive())
        return;

    brls::async([] {
        const auto report = akira::net::checkSonyDnsAtStartup();
        if (!report.failed())
            return;

        std::string failedDomain;
        std::string failedServer;
        // Prefer a concrete poisoned DNS answer over an ambiguous timeout.
        for (const auto verdict : {akira::net::DnsVerdict::Blocked,
                                   akira::net::DnsVerdict::Inconclusive}) {
            for (const auto& server : report.servers) {
                for (size_t i = 0; i < server.domains.size(); ++i) {
                    if (server.domains[i] != verdict)
                        continue;
                    failedDomain = akira::net::SonyDnsDomains[i];
                    failedServer = server.address;
                    break;
                }
                if (!failedDomain.empty())
                    break;
            }
            if (!failedDomain.empty())
                break;
        }
        brls::Logger::warning("[NET] Sony DNS check failed domain={} server={}",
            failedDomain, failedServer);
        brls::sync([failedDomain = std::move(failedDomain),
                    failedServer = std::move(failedServer)] {
            (new DelayedDnsWarningDialog(
                brls::getStr("akira/network/dns_failed", failedDomain, failedServer)))->show();
        });
    }, true);
}

void initCustomTheme()
{
    akira::ui::setActiveTheme(SettingsManager::getInstance()->getUiTheme());
    akira::ui::applyToBorealis();
}

static const char* appletTypeToString(AppletType type)
{
    switch (type)
    {
        case AppletType_None:              return "None";
        case AppletType_Default:           return "Default";
        case AppletType_Application:       return "Application";
        case AppletType_SystemApplet:      return "SystemApplet";
        case AppletType_LibraryApplet:     return "LibraryApplet";
        case AppletType_OverlayApplet:     return "OverlayApplet";
        case AppletType_SystemApplication: return "SystemApplication";
        default:                           return "Unknown";
    }
}

class HeaderStatusView : public brls::Box
{
public:
    HeaderStatusView()
    {
        this->setAxis(brls::Axis::ROW);
        this->setAlignItems(brls::AlignItems::CENTER);

        brls::Platform* platform = brls::Application::getPlatform();

        auto* battery = new brls::BatteryWidget();
        battery->setVisibility(platform->canShowBatteryLevel() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        battery->setMarginRight(21);
        this->addView(battery);

        auto* wireless = new brls::WirelessWidget();
        wireless->setVisibility(platform->canShowWirelessLevel() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        wireless->setMarginRight(21);
        this->addView(wireless);

        timeLabel = new brls::Label();
        timeLabel->setVerticalAlign(brls::VerticalAlign::CENTER);
        timeLabel->setFontSize(21.5f);
        this->addView(timeLabel);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override
    {
        auto in_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto tm        = *std::localtime(&in_time_t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%H:%M:%S");
        if (ss.str() != lastTime)
        {
            lastTime = ss.str();
            timeLabel->setText(lastTime);
        }
        brls::Box::draw(vg, x, y, width, height, style, ctx);
    }

private:
    brls::Label* timeLabel = nullptr;
    std::string lastTime;
};

void decorateAkiraHeader(brls::AppletFrame* appletFrame)
{
    if (!appletFrame)
        return;

    std::string version = getAppVersion();
    appletFrame->setTitle("Akira");
    if (!version.empty()) {
        if (auto* titleLabel = dynamic_cast<brls::Label*>(appletFrame->getView("brls/applet_frame/title_label"))) {
            titleLabel->setMarginTop(0);
            titleLabel->setFontSize(34);
            if (auto* titleBox = dynamic_cast<brls::Box*>(titleLabel->getParent())) {
                titleBox->setAlignItems(brls::AlignItems::BASELINE);

                auto* versionLabel = new brls::Label();
                versionLabel->setText(version);
                versionLabel->setFontSize(19);
                versionLabel->setTextColor(akira::ui::active().textDim);
                versionLabel->setMarginLeft(8);

                const auto& kids = titleBox->getChildren();
                size_t insertAt = kids.size();
                for (size_t i = 0; i < kids.size(); i++) {
                    if (kids[i] == titleLabel) {
                        insertAt = i + 1;
                        break;
                    }
                }
                titleBox->addView(versionLabel, insertAt);
            }
        }
    }

    auto* header = appletFrame->getHeader();
    if (header) {
        header->setAlignItems(brls::AlignItems::CENTER);

        auto* headerRight = new brls::Box();
        headerRight->setAxis(brls::Axis::COLUMN);
        headerRight->setJustifyContent(brls::JustifyContent::CENTER);
        headerRight->setAlignItems(brls::AlignItems::FLEX_END);
        headerRight->setGrow(1.0f);
        headerRight->setMarginRight(20);

        headerRight->addView(new HeaderStatusView());

        auto* attribution = new brls::Label();
        attribution->setText("akira/app/about"_i18n);
        attribution->setFontSize(14);
        attribution->setTextColor(akira::ui::active().textDim);
        attribution->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
        headerRight->addView(attribution);

        header->addView(headerRight);
    }

    auto* footer = appletFrame->getFooter();
    if (footer && !footer->getChildren().empty()) {
        if (auto* containerBox = dynamic_cast<brls::Box*>(footer->getChildren()[0])) {
            if (!containerBox->getChildren().empty()) {
                if (auto* rowBox = dynamic_cast<brls::Box*>(containerBox->getChildren()[0])) {
                    for (auto* child : rowBox->getChildren()) {
                        if (dynamic_cast<brls::Hints*>(child))
                            continue;
                        if (auto* box = dynamic_cast<brls::Box*>(child)) {
                            box->setVisibility(brls::Visibility::GONE);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void akiraOpenTrophies()
{
    /* Which screen asked, and how deep the stack was. Trophies opening from
     * under the controller picker could not be explained by borealis's
     * dispatch, and the focus at the moment of the call is what named the
     * cause: the home tab taking focus back from four activities down. */
    {
        auto* focus = brls::Application::getCurrentFocus();
        brls::Logger::info("akiraOpenTrophies: focus={} stack={}",
                           focus ? focus->describe() : std::string("none"),
                           brls::Application::getActivitiesStack().size());
    }

    /*
     * Also guarded here, not only at registration: the action is registered once per view
     * but the active profile can change under it.
     */
    if (SettingsManager::getInstance()->isLegacyProfileActive()) {
        brls::Application::notify("akira/trophies/no_psn_token"_i18n);
        return;
    }

    if (!SettingsManager::getInstance()->getActiveProfileTrophiesEnabled()) {
        brls::Application::notify("akira/trophies/disabled_for_profile"_i18n);
        return;
    }
    auto* view = TrophyListTab::create();
    view->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    }, false);
    auto* frame = new brls::AppletFrame(view);
    decorateAkiraHeader(frame);
    brls::Application::pushActivity(new brls::Activity(frame));
}

void akiraOpenSettings()
{
    if (SettingsManager::getInstance()->getProfiles().empty()) {
        auto* setupFrame = new brls::AppletFrame(new ProfileSetupView(false, true));
        decorateAkiraHeader(setupFrame);
        brls::Application::pushActivity(new brls::Activity(setupFrame));
        return;
    }
    auto* frame = new brls::AppletFrame(new SettingsFrameView());
    decorateAkiraHeader(frame);
    brls::Application::pushActivity(new brls::Activity(frame));
}

void registerAkiraTabActions(brls::View* view)
{
    /*
     * Trophies need a PSN token, so in legacy the action is not registered at all rather
     * than opening a screen that can only tell the user no.
     */
    if (!SettingsManager::getInstance()->isLegacyProfileActive())
    {
        view->registerAction("akira/tabs/trophies"_i18n, brls::ControllerButton::BUTTON_LB, [](brls::View*) {
            akiraOpenTrophies();
            return true;
        }, false);
    }
    view->registerAction("akira/tabs/settings"_i18n, brls::ControllerButton::BUTTON_RB, [](brls::View*) {
        akiraOpenSettings();
        return true;
    }, false);
}

class MainActivity : public brls::Activity
{
public:
    CONTENT_FROM_XML_RES("activity/main.xml");

    brls::Label* discoveryStatusLabel = nullptr;
    brls::RepeatingTimer discoveryStatusTimer;

    void onResume() override
    {
        brls::Activity::onResume();
        HostListTab::refreshRailsIfActive();
    }

    void onContentAvailable() override
    {
        brls::Logger::info("Main activity content available");

        auto* appletFrame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
        if (appletFrame) {
            decorateAkiraHeader(appletFrame);

            auto* footer = appletFrame->getFooter();
            if (footer) {
                auto& outerChildren = footer->getChildren();
                if (!outerChildren.empty()) {
                    auto* containerBox = dynamic_cast<brls::Box*>(outerChildren[0]);
                    if (containerBox && !containerBox->getChildren().empty()) {
                        auto* rowBox = dynamic_cast<brls::Box*>(containerBox->getChildren()[0]);
                        if (rowBox) {
                            for (auto* child : rowBox->getChildren()) {
                                if (dynamic_cast<brls::Hints*>(child))
                                    continue;
                                if (auto* box = dynamic_cast<brls::Box*>(child)) {
                                    box->setVisibility(brls::Visibility::GONE);
                                    break;
                                }
                            }

                            auto* footerCol = new brls::Box();
                            footerCol->setAxis(brls::Axis::COLUMN);
                            footerCol->setDirection(brls::Direction::LEFT_TO_RIGHT);
                            footerCol->setJustifyContent(brls::JustifyContent::CENTER);
                            footerCol->setAlignItems(brls::AlignItems::FLEX_START);

                            auto* ipLabel = new brls::Label();
                            ipLabel->setText("akira/app/ip_prefix"_i18n + getLocalIpAddress());
                            ipLabel->setFontSize(18);
                            ipLabel->setTextColor(akira::ui::active().textDim);
                            ipLabel->setHorizontalAlign(brls::HorizontalAlign::LEFT);
                            footerCol->addView(ipLabel);

                            discoveryStatusLabel = new brls::Label();
                            discoveryStatusLabel->setFontSize(13);
                            discoveryStatusLabel->setTextColor(akira::ui::active().textDim);
                            discoveryStatusLabel->setHorizontalAlign(brls::HorizontalAlign::LEFT);
                            footerCol->addView(discoveryStatusLabel);

                            rowBox->addView(footerCol);

                            discoveryStatusTimer.setCallback([this]() {
                                if (!discoveryStatusLabel)
                                    return;
                                DiscoveryManager::SweepStatus st = DiscoveryManager::getInstance()->getSweepStatus();
                                std::string text;
                                if (!st.serviceRunning) {
                                    text = "akira/app/discovery_off"_i18n;
                                } else if (!st.sweepActive || st.subnets.empty()) {
                                    text = "akira/app/discovery_local"_i18n;
                                } else {
                                    std::string subs;
                                    for (size_t i = 0; i < st.subnets.size(); i++) {
                                        if (i > 0)
                                            subs += ", ";
                                        subs += st.subnets[i];
                                    }
                                    text = "akira/app/discovery_running_for"_i18n + " " + subs;
                                    if (!st.currentTarget.empty())
                                        text += " · " + st.currentTarget;
                                }
                                discoveryStatusLabel->setText(text);
                            });
                            discoveryStatusTimer.start(500);
                        }
                    }
                }
            }
        }

        this->registerAction("akira/tabs/trophies"_i18n, brls::ControllerButton::BUTTON_LB, [](brls::View*) {
            akiraOpenTrophies();
            return true;
        }, false);

        this->registerAction("akira/tabs/settings"_i18n, brls::ControllerButton::BUTTON_RB, [](brls::View*) {
            akiraOpenSettings();
            return true;
        }, false);

        akira::UpdateFlow::checkOnLaunch();
    }
};

class AppletWarningActivity : public brls::Activity
{
public:
    CONTENT_FROM_XML_RES("activity/applet_warning.xml");
};

int main(int argc, char* argv[])
{
    brls::Logger::setLogLevel(brls::LogLevel::LOG_INFO);
    auto* settings = SettingsManager::getInstance();
    settings->applyChiakiLogVerbosity();

    akira::UpdateManager::setSelfPath(argc > 0 && argv[0] ? argv[0] : "");

    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "-d") == 0)
        {
            brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        }
        else if (std::strcmp(argv[i], "-v") == 0)
        {
            brls::Application::enableDebuggingView(true);
        }
    }

    av_log_set_callback(ffmpeg_log_callback);

    std::string overrideLocale = settings->getDebugLocale();
    if (!overrideLocale.empty()) {
        brls::Platform::APP_LOCALE_DEFAULT = overrideLocale;
    } else {
        brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;
    }

    initCustomTheme();

    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    brls::getStyle().addMetric("brls/tab_frame/sidebar_width", 369.0f);
    brls::getStyle().addMetric("brls/sidebar/item_font_size", 24.0f);

    brls::Application::getWindowFocusChangedEvent()->subscribe([](bool focused) {
        if (focused)
            httpMarkConnectionsStale();
    });

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
    {
        brls::Logger::error("SDL_Init failed: {}", SDL_GetError());
        return EXIT_FAILURE;
    }

    const CURLcode curlInit = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curlInit != CURLE_OK)
    {
        brls::Logger::error("[NET] curl_global_init failed code={} reason=\"{}\"",
            static_cast<int>(curlInit), curl_easy_strerror(curlInit));
        return EXIT_FAILURE;
    }

    std::optional<akira::ChiakiCore> core;
    core.emplace(*settings);
    if (!core->opened())
        brls::Logger::error("[CORE] running without the core: streaming, sign-in and discovery are unavailable");
    brls::Application::getWindowFocusChangedEvent()->subscribe([&core](bool focused) {
        if (focused && core)
            core->http().drop_connections("applet regained focus");
    });

    if (SettingsManager::getInstance()->getEnableFileLogging())
        akira::AppLog::open(SettingsManager::getLogFilePath());

    brls::Logger::setAsyncLogging(true);
    if (const curl_version_info_data* curlInfo = curl_version_info(CURLVERSION_NOW))
    {
        brls::Logger::info(
            "[NET] curl_runtime version={} ssl={} features=0x{:x} ipv6={} async_dns={}",
            curlInfo->version ? curlInfo->version : "-",
            curlInfo->ssl_version ? curlInfo->ssl_version : "-",
            static_cast<unsigned long long>(curlInfo->features),
            (curlInfo->features & CURL_VERSION_IPV6) != 0,
            (curlInfo->features & CURL_VERSION_ASYNCHDNS) != 0);
    }
    brls::Application::getRunLoopEvent()->subscribe([]() {
        using namespace std::chrono_literals;
        static auto nextFlush = std::chrono::steady_clock::now();
        static std::atomic<bool> flushInFlight{false};
        const auto now = std::chrono::steady_clock::now();
        if (now < nextFlush || flushInFlight.exchange(true, std::memory_order_acq_rel))
            return;
        nextFlush = now + 250ms;

        brls::async([]() {
            brls::Logger::flushAsyncLogs();
            flushInFlight.store(false, std::memory_order_release);
        }, true);
    });
    brls::Logger::info("Async logging enabled via thread pool");

    brls::Logger::info("Chiaki library initialized");

    AppletType appletType = appletGetAppletType();
    brls::Logger::info("Applet type: {} ({})", appletTypeToString(appletType), static_cast<int>(appletType));

    Session::GetInstance()->SetMesaConfig();

    brls::Application::registerXMLView("HostListTab", HostListTab::create);
    brls::Application::registerXMLView("PsnGatedBox", PsnGatedBox::create);
    brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);
    brls::Application::registerXMLView("TrophyListTab", TrophyListTab::create);
    brls::Application::registerXMLView("AddHostTab", AddHostTab::create);
    brls::Application::registerXMLView("BuildInfoTab", BuildInfoTab::create);
    brls::Application::registerXMLView("ConfigViewTab", ConfigViewTab::create);
    brls::Application::registerXMLView("NetworkUtilitiesTab", NetworkUtilitiesTab::create);
    brls::Application::registerXMLView("StreamView", StreamView::create);
    brls::Application::registerXMLView("EnterPinView", EnterPinView::create);

    brls::Application::createWindow("Akira");

    if (!brls::Application::loadFontFromFile("mono", BRLS_ASSET("font/Cousine-Regular.ttf")))
        brls::Logger::warning("Could not load mono font, stats overlay will fall back to the regular font");

    brls::Application::getPlatform()->exitToHomeMode(false);

    if (appletType == AppletType_Application)
    {
        brls::Application::pushActivity(new MainActivity());
        startSonyDnsStartupCheck();
    }
    else
    {
        brls::Application::pushActivity(new AppletWarningActivity());
    }

    try{
        while (brls::Application::mainLoop())
        {
        }
    } catch (const std::exception& e) {
        brls::Logger::error("CRASH: {}", e.what());
        fflush(stdout);
    }

    brls::Logger::info("Application exiting");

    DiscoveryManager::getInstance()->setServiceEnabled(false);
    for (auto& [name, host] : *settings->getHostsMap())
    {
        if (host)
            host->abandonRegistration();
    }

    HttpPool::instance().stop();

    WireGuardManager::instance().disconnect();

    core.reset();

    SDL_Quit();
    curl_global_cleanup();

    // Drain the final batch after all subsystems have reported shutdown.
    brls::Logger::flushAsyncLogs();
    nvExit();

    return EXIT_SUCCESS;
}
