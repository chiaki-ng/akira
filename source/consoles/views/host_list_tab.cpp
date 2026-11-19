#include "consoles/views/host_list_tab.hpp"
#include "log/log.hpp"
#if AKIRA_CLOUD
#include "cloud/views/library_view.hpp"
#include "cloud/cloud_connect_task.hpp"
#include "cloud/service.hpp"
#endif
#include "app/views/akira_header.hpp"
#include "app/views/theme.hpp"
#include "log/views/log_pane.hpp"
#include "app/views/motion.hpp"
#include "play/views/stream_view.hpp"
#include "play/views/connecting_view.hpp"
#include "play/views/host_connect_task.hpp"
#include "consoles/views/enter_pin_view.hpp"
#include "consoles/views/pair_view.hpp"
#include "accounts/views/profile_setup_view.hpp"
#include "accounts/views/qr_sign_in_view.hpp"
#include "consoles/host.hpp"
#include "trophies/trophy_manager.hpp"
#include "play/chiaki_core.hpp"
#include "app/views/shared_view_holder.hpp"

#include <borealis/core/i18n.hpp>

#include <ctime>
#include <functional>
#include <iterator>
#include <algorithm>
#include <cctype>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>

namespace akira::consoles::views {

using namespace brls::literals;

static const brls::ButtonStyle BUTTONSTYLE_BLUE = {
    .shadowType              = brls::ShadowType::GENERIC,
    .hideHighlightBackground = true,
    .highlightPadding = "",
    .borderThickness  = "",
    .enabledBackgroundColor = "",
    .enabledLabelColor      = "brls/button/primary_enabled_text",
    .enabledBorderColor     = "",
    .disabledBackgroundColor = "",
    .disabledLabelColor      = "brls/button/primary_disabled_text",
    .disabledBorderColor     = "",
};

static const brls::ButtonStyle BUTTONSTYLE_ORANGE = {
    .shadowType              = brls::ShadowType::GENERIC,
    .hideHighlightBackground = true,
    .highlightPadding = "",
    .borderThickness  = "",
    .enabledBackgroundColor = "",
    .enabledLabelColor      = "brls/button/primary_enabled_text",
    .enabledBorderColor     = "",
    .disabledBackgroundColor = "",
    .disabledLabelColor      = "brls/button/primary_disabled_text",
    .disabledBorderColor     = "",
};

HostListTab* HostListTab::currentInstance = nullptr;
bool HostListTab::isConnecting = false;
bool HostListTab::isRegistering = false;
bool HostListTab::isActive = false;
bool HostListTab::connectionActive = false;


static brls::Activity* s_autoRegProgress = nullptr;
static brls::Label* s_autoRegStageLabel = nullptr;
static std::atomic<bool> s_autoRegCanceled{false};

class AutoRegProgressBox : public brls::Box {
public:
    explicit AutoRegProgressBox(bool showStages) : showStages(showStages) {
        if (!showStages)
            logPane.subscribe();
    }

    ~AutoRegProgressBox() override {
        logPane.unsubscribe();
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override {
        Box::draw(vg, x, y, width, height, style, ctx);

        if (showStages)
            return;

        auto* logArea = this->getView("autoreg/logArea");
        if (!logArea)
            return;

        logPane.render(vg, logArea->getX(), logArea->getY(),
                       logArea->getWidth(), logArea->getHeight());
    }

private:
    bool showStages;
    log::views::LogPane logPane;
};

static void setAutoRegStage(int index, int total) {
    if (!s_autoRegStageLabel)
        return;
    if (!settings::SettingsManager::getInstance()->app().connectionShowStages())
        return;
    std::string name;
    switch (index) {
        case 1: name = "akira/hosts/register_stage_account"_i18n; break;
        case 2: name = "akira/hosts/register_stage_console"_i18n; break;
        case 3: name = "akira/hosts/register_stage_connect"_i18n; break;
        default: name = "akira/hosts/register_stage_finish"_i18n; break;
    }
    s_autoRegStageLabel->setText(brls::getStr("akira/hosts/register_stage_fmt", index, total, name));
}

class HostItemView : public brls::Box {
public:
    HostItemView(Host* host) : host(host), hostName(host->getHostName()) {
        this->setAxis(brls::Axis::COLUMN);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setJustifyContent(brls::JustifyContent::FLEX_START);
        this->setWidth(174);
        this->setHeight(238);
        this->setPadding(14);
        this->setMarginRight(14);
        this->setMarginBottom(14);
        this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
        this->setCornerRadius(14);
        this->setFocusable(true);

        consoleImg = new brls::Image();
        consoleImg->setImageFromRes(host->isPS5() ? "img/console/ps5.png" : "img/console/ps4.png");
        consoleImg->setScalingType(brls::ImageScalingType::FIT);
        consoleImg->setDimensions(146, 118);
        consoleImg->setMarginBottom(12);
        this->addView(consoleImg);

        nameLabel = new brls::Label();
        nameLabel->setFontSize(22);
        nameLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        nameLabel->setMarginBottom(6);
        nameLabel->setAutoAnimate(true);
        this->addView(nameLabel);

        typePill = new brls::Box();
        typePill->setAxis(brls::Axis::ROW);
        typePill->setCornerRadius(11);
        typePill->setPaddingLeft(11);
        typePill->setPaddingRight(11);
        typePill->setPaddingTop(2);
        typePill->setPaddingBottom(3);
        typePill->setMarginBottom(8);

        typeLabel = new brls::Label();
        typeLabel->setFontSize(16);
        typePill->addView(typeLabel);
        this->addView(typePill);

        statusPill = new brls::Box();
        statusPill->setAxis(brls::Axis::ROW);
        statusPill->setCornerRadius(11);
        statusPill->setPaddingLeft(11);
        statusPill->setPaddingRight(11);
        statusPill->setPaddingTop(2);
        statusPill->setPaddingBottom(3);

        statusLabel = new brls::Label();
        statusLabel->setFontSize(16);
        statusPill->addView(statusLabel);
        this->addView(statusPill);

        this->registerClickAction([this](brls::View*) {
            doPrimaryAction();
            return true;
        });
        this->registerAction("akira/hosts/options"_i18n, brls::ControllerButton::BUTTON_Y,
            [this](brls::View*) { openContextMenu(); return true; }, false);

        app::views::motion::liftOnFocus(this, focusAnim);

        updateState();
    }

    void playEntrance(int delayMs) {
        app::views::motion::fadeIn(this, entranceAnim, delayMs, 300);
    }

    void updateState() {
        std::string displayName = host->getHostName();
        if (host->isRemote() && displayName.length() > 9 && displayName.substr(displayName.length() - 9) == " (Remote)")
            displayName = displayName.substr(0, displayName.length() - 9);
        nameLabel->setText(displayName);

        std::string typeText;
        NVGcolor typeColor;
        if (host->isRemote()) {
            typeText = "akira/hosts/remote"_i18n;
            typeColor = app::views::active().accentStrong;
        } else if (host->isManual()) {
            typeText = "akira/hosts/manual"_i18n;
            typeColor = app::views::active().media;
        } else if (host->isAuto()) {
            typeText = "akira/hosts/auto"_i18n;
            typeColor = app::views::active().accent;
        } else {
            typeText = "akira/hosts/local"_i18n;
            typeColor = app::views::active().textMuted;
        }
        typeLabel->setText(typeText);
        typeLabel->setTextColor(typeColor);
        typePill->setBackgroundColor(app::views::withAlpha(typeColor, 0x2e));

        std::string pillText;
        NVGcolor fg = app::views::active().textMuted;
        bool dim = true;
        if (offersLink()) {
            pillText = "akira/hosts/link"_i18n;
            fg = app::views::active().accent;
        } else if (host->usesInternetPairing()) {
            pillText = "akira/hosts/ready"_i18n;
            fg = app::views::active().success;
            dim = false;
        } else if (!host->hasRpKey() && (host->isDiscovered() || host->canPairOverInternet())) {
            pillText = "akira/hosts/register"_i18n;
            fg = app::views::active().accent;
        } else if (host->isStandby() && host->hasRpKey()) {
            pillText = "akira/hosts/standby"_i18n;
        } else if ((host->hasRpKey() && host->isReady()) || (host->isRemote() && host->hasRpKey())) {
            pillText = "akira/hosts/ready"_i18n;
            fg = app::views::active().success;
            dim = false;
        } else {
            pillText = host->getStateString();
        }
        statusLabel->setText(pillText);
        statusLabel->setTextColor(fg);
        statusPill->setBackgroundColor(app::views::withAlpha(fg, 0x2e));

        int wantDim = dim ? 1 : 0;
        if (wantDim != lastDim) {
            float target = dim ? 0.5f : 1.0f;
            if (lastDim < 0) {
                consoleImg->setAlpha(target);
                dimAnim.reset(target);
            } else {
                app::views::motion::animate(dimAnim, dimAnim.getValue(), target, 260,
                    brls::EasingFunction::quadraticOut,
                    [this](float v) { consoleImg->setAlpha(v); });
            }
            lastDim = wantDim;
        }
    }

    Host* getHost() { return host; }
    std::string getHostName() const { return hostName; }

    void doPrimaryAction() {
        if (offersLink()) doLink();
        else if (host->usesInternetPairing()) doConnect();
        else if (!host->hasRpKey() && (host->isDiscovered() || host->canPairOverInternet())) doRegister();
        else if (host->isStandby() && host->hasRpKey() && !host->isRemote()) doWake();
        else if (host->hasRpKey()) doConnect();
        else log::warning("No primary action for {} (remote={}, needsLink={}, discovered={}, rpkey={}, canAuto={})",
            host->getHostName(), host->isRemote(), host->needsLink(), host->isDiscovered(), host->hasRpKey(), host->canPairOverInternet());
    }

    void openContextMenu() {
        std::vector<std::string> options;
        std::vector<std::function<void()>> actions;
        if (host->hasRpKey() && !host->isRemote()) {
            options.push_back("akira/host_settings/console_pin"_i18n);
            actions.emplace_back([this] { doConsolePIN(); });
        }
        if (!host->isCloud()) {
            options.push_back(host->getHostAddr().empty() ? "akira/hosts/add_address"_i18n : "akira/hosts/set_address"_i18n);
            actions.emplace_back([this] { doAddress(); });
        }
        if (host->isRegistered() && !host->isCloud()) {
            options.push_back("akira/hosts/takion_version"_i18n);
            actions.emplace_back([this] { doTakionPin(); });
        }
        if (host->isRegistered()) {
            options.push_back("akira/hosts/delete"_i18n);
            actions.emplace_back([this] { doDelete(); });
        }
        if (options.empty()) return;
        auto* dropdown = new brls::Dropdown("akira/hosts/options"_i18n, options,
            [](int) {}, 0,
            [actions](int sel) {
                if (sel < 0 || sel >= static_cast<int>(actions.size())) return;
                actions[static_cast<size_t>(sel)]();
            });
        brls::Application::pushActivity(new brls::Activity(dropdown));
    }

private:
    Host* host;
    std::string hostName;
    brls::Image* consoleImg;
    brls::Label* nameLabel;
    brls::Box* typePill;
    brls::Label* typeLabel;
    brls::Box* statusPill;
    brls::Label* statusLabel;
    brls::Animatable entranceAnim{1.0f};
    brls::Animatable focusAnim{0.0f};
    brls::Animatable dimAnim{1.0f};
    int lastDim = -1;

    void doConnect() {
        HostListTab::connectToHost(host);
    }

    void doWake() {
            log::info("Wake {}", host->getHostName());

            if (host->wake()) {
                brls::Application::notify(brls::getStr("akira/hosts/wake_sent", host->getHostName()));
            } else {
                brls::Application::notify(brls::getStr("akira/hosts/wake_failed", host->getHostName()));
            }
    }

    void doRegister() {
            if (HostListTab::isRegistering) {
                return;
            }

            log::info("Register button clicked for {}", host->getHostName());

            auto* settings = settings::SettingsManager::getInstance();
            auto* accounts = play::ChiakiCore::current().accounts();
            log::info("Register {}: remoteDuid='{}' signedIn={} hasAccountId={} canAuto={}",
                host->getHostName(), host->getRemoteDuid(),
                accounts && accounts->state() == accounts::SessionState::Valid,
                !settings->accounts().psnAccountId().empty(), host->canPairOverInternet());
            bool needsAccountId = host->needsAccountId();
            std::string accountId = settings->accounts().psnAccountId();
            std::string onlineId = settings->accounts().psnOnlineId();

            if (needsAccountId && accountId.empty()) {
                auto* dialog = new brls::Dialog("akira/hosts/psn_account_id_required_register"_i18n);
                dialog->addButton("akira/common/ok"_i18n, [dialog]() {
                    dialog->close();
                });
                dialog->open();
                return;
            }

            if (!needsAccountId && onlineId.empty()) {
                auto* dialog = new brls::Dialog("akira/hosts/psn_online_id_required_register"_i18n);
                dialog->addButton("akira/common/ok"_i18n, [dialog]() {
                    dialog->close();
                });
                dialog->open();
                return;
            }

            if (host->canPairOverInternet()) {
                std::vector<std::string> options = {
                    "akira/hosts/register_auto"_i18n,
                    "akira/hosts/register_pin"_i18n
                };
                Host* hp = host;
                auto* dropdown = new brls::Dropdown("akira/hosts/register_how"_i18n, options,
                    [](int) {}, 0,
                    [hp](int sel) {
                        if (sel == 0) {
                            startAutoRegistration(hp);
                        } else if (sel == 1) {
                            startPinRegistration(hp);
                        }
                    });
                brls::Application::pushActivity(new brls::Activity(dropdown));
                return;
            }

            startPinRegistration(host);
    }

    static void startPinRegistration(Host* host) {
        if (HostListTab::isRegistering) {
            return;
        }

        Host* hostPtr = host;

        auto* pinView = new EnterPinView(host, PinViewType::Registration);
        pinView->setOnCancel([hostPtr]() {
            hostPtr->cancelRegistration();
            HostListTab::isRegistering = false;
            log::info("PIN entry cancelled");
        });
        pinView->setOnPinEntered([hostPtr](const std::string& pin) {
            HostListTab::isRegistering = true;
            int pinValue = std::atoi(pin.c_str());
            int result = hostPtr->registerHost(pinValue, [hostPtr](Host::PairingOutcome outcome) {
                HostListTab::isRegistering = false;
                switch (outcome) {
                    case Host::PairingOutcome::Paired:
                        brls::Application::notify("akira/hosts/registration_success"_i18n);
                        hostPtr->setNeedsLink(false);
                        settings::SettingsManager::getInstance()->store().save();
                        if (HostListTab::currentInstance) {
                            HostListTab::currentInstance->updateHostItem(hostPtr);
                        }
                        break;
                    case Host::PairingOutcome::Canceled:
                        brls::Application::notify("akira/hosts/registration_canceled"_i18n);
                        break;
                    case Host::PairingOutcome::Refused:
                    case Host::PairingOutcome::Failed:
                        brls::Application::notify("akira/hosts/registration_failed"_i18n);
                        break;
                }
            });
            if (result != HOST_REGISTER_OK) {
                HostListTab::isRegistering = false;
                std::string errorMsg;
                switch (result) {
                    case HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID:
                        errorMsg = "akira/hosts/error_psn_account_id"_i18n;
                        break;
                    case HOST_REGISTER_ERROR_SETTING_PSNONLINEID:
                        errorMsg = "akira/hosts/error_psn_online_id"_i18n;
                        break;
                    case HOST_REGISTER_ERROR_UNDEFINED_TARGET:
                        errorMsg = "akira/hosts/error_console_type"_i18n;
                        break;
                    default:
                        errorMsg = "akira/hosts/error_registration_failed"_i18n;
                        break;
                }
                brls::Application::notify(errorMsg);
            }
        });

        brls::Application::pushActivity(new brls::Activity(pinView));
    }

    static void dismissAutoRegProgress() {
        s_autoRegStageLabel = nullptr;
        if (s_autoRegProgress) {
            s_autoRegProgress = nullptr;
            brls::Application::popActivity();
        }
    }

    static void showAutoRegProgress(Host* host) {
        bool showStages = settings::SettingsManager::getInstance()->app().connectionShowStages();

        auto* box = new AutoRegProgressBox(showStages);
        box->setAxis(brls::Axis::COLUMN);
        box->setJustifyContent(showStages ? brls::JustifyContent::CENTER
                                          : brls::JustifyContent::FLEX_START);
        box->setAlignItems(brls::AlignItems::CENTER);
        box->setWidthPercentage(100.0f);
        box->setHeightPercentage(100.0f);
        box->setBackgroundColor(nvgRGB(0, 0, 0));

        if (showStages) {
            auto* spinner = new brls::ProgressSpinner();
            spinner->setWidth(64);
            spinner->setHeight(64);
            box->addView(spinner);
        } else {
            box->setPadding(20, 40, 20, 40);
        }

        auto* label = new brls::Label();
        label->setText("akira/hosts/registering_psn"_i18n);
        label->setFontSize(showStages ? 22 : 24);
        if (showStages)
            label->setMarginTop(24);
        box->addView(label);
        s_autoRegStageLabel = label;

        if (!showStages) {
            auto* logArea = new brls::Box();
            logArea->setId("autoreg/logArea");
            logArea->setWidthPercentage(100.0f);
            logArea->setGrow(1.0f);
            box->addView(logArea);
        }

        auto* hint = new brls::Label();
        hint->setText("akira/hosts/register_cancel_hint"_i18n);
        hint->setFontSize(16);
        hint->setTextColor(nvgRGB(0x88, 0x88, 0x88));
        hint->setMarginTop(12);
        box->addView(hint);

        box->setFocusable(true);
        box->registerAction("akira/common/cancel"_i18n, brls::ControllerButton::BUTTON_B,
            [host](brls::View*) {
                s_autoRegCanceled = true;
                host->cancelRegistration();
                HostListTab::isRegistering = false;
                dismissAutoRegProgress();
                return true;
            }, false);

        auto* activity = new brls::Activity(box);
        s_autoRegProgress = activity;
        brls::Application::pushActivity(activity);
        brls::Application::giveFocus(box);
    }

    static void startAutoRegistration(Host* host) {
        if (HostListTab::isRegistering) {
            return;
        }
        HostListTab::isRegistering = true;
        s_autoRegCanceled = false;

        Host* hostPtr = host;

        showAutoRegProgress(host);
        setAutoRegStage(1, 4);

        const HostRegisterError result = host->registerOverInternet(
            [](int index, int total) {
                setAutoRegStage(index, total);
            },
            [hostPtr](Host::PairingOutcome outcome) {
                HostListTab::isRegistering = false;
                dismissAutoRegProgress();
                if (outcome == Host::PairingOutcome::Paired) {
                    brls::Application::notify("akira/hosts/registration_success"_i18n);
                    hostPtr->setNeedsLink(false);
                    settings::SettingsManager::getInstance()->store().save();
                    if (HostListTab::currentInstance) {
                        HostListTab::currentInstance->updateHostItem(hostPtr);
                    }
                    return;
                }
                if (outcome == Host::PairingOutcome::Canceled || s_autoRegCanceled) {
                    return;
                }
                brls::Application::notify("akira/hosts/auto_register_fallback"_i18n);
                startPinRegistration(hostPtr);
            });

        if (result != HOST_REGISTER_OK) {
            HostListTab::isRegistering = false;
            dismissAutoRegProgress();
            brls::Application::notify("akira/hosts/auto_register_fallback"_i18n);
            startPinRegistration(hostPtr);
        }
    }

    bool offersLink() const {
        return host->isRemote() && host->needsLink() && (!linkableHosts().empty() || !host->canPairOverInternet());
    }

    static std::vector<std::pair<std::string, Host*>> linkableHosts() {
        std::vector<std::pair<std::string, Host*>> hosts;
        for (auto& [name, h] : settings::SettingsManager::getInstance()->hosts().entries()) {
            if (h && h->hasRpKey() && !h->isRemote())
                hosts.emplace_back(name, h.get());
        }
        return hosts;
    }

    void doLink() {
            log::info("Link button clicked for {}", host->getHostName());

            std::vector<std::string> hostNames;
            std::vector<Host*> hostPtrs;
            for (auto& [name, h] : linkableHosts()) {
                hostNames.push_back(name);
                hostPtrs.push_back(h);
            }

            if (hostNames.empty()) {
                brls::Application::notify("akira/hosts/no_registered_hosts"_i18n);
                return;
            }

            Host* remoteHost = host;
            auto* dropdown = new brls::Dropdown(
                "akira/hosts/select_host_to_link"_i18n,
                hostNames,
                [remoteHost, hostPtrs, hostNames](int selected) {
                    if (selected < 0 || selected >= (int)hostPtrs.size()) return;

                    Host* localHost = hostPtrs[selected];
                    std::string oldName = hostNames[selected];
                    auto* settings = settings::SettingsManager::getInstance();

                    std::string remoteName = remoteHost->getHostName();
                    if (remoteName.length() > 9 && remoteName.substr(remoteName.length() - 9) == " (Remote)") {
                        remoteName = remoteName.substr(0, remoteName.length() - 9);
                    }

                    if (oldName != remoteName) {
                        settings->hosts().rename(oldName, remoteName);
                    }
                    remoteHost->copyRegistrationFrom(localHost);
                    remoteHost->setNeedsLink(false);
                    localHost->setRemoteDuid(remoteHost->getRemoteDuid());
                    settings->store().save();

                    brls::Application::notify(brls::getStr("akira/hosts/linked_to", remoteName));

                    brls::sync([]() {
                        if (HostListTab::currentInstance) {
                            HostListTab::currentInstance->syncHostList();
                            if (HostListTab::currentInstance->findRemoteBtn) {
                                brls::Application::giveFocus(HostListTab::currentInstance->findRemoteBtn);
                            }
                        }
                    });
                }
            );
            brls::Application::pushActivity(new brls::Activity(dropdown));
    }

    void doConsolePIN() {
            std::string current = host->getConsolePIN();
            brls::Application::getImeManager()->openForText(
                [this](std::string text) {
                    if (!text.empty() && (text.length() != 4 || !std::ranges::all_of(text, ::isdigit))) {
                        brls::Application::notify("akira/host_settings/console_pin_invalid"_i18n);
                        return;
                    }
                    host->setConsolePIN(text);
                    settings::SettingsManager::getInstance()->store().save();
                    brls::Application::notify("akira/host_settings/console_pin_saved"_i18n);
                },
                "akira/host_settings/console_pin"_i18n, "akira/host_settings/console_pin_hint"_i18n, 4, current, 0);
    }

    void doAddress() {
        Host* target = host;
        const std::string current = host->getHostAddr();
        brls::Application::getImeManager()->openForText(
            [target](std::string text) {
                if (text.empty() || text == target->getHostAddr()) return;
                if (!Host::isValidAddress(text)) {
                    brls::Application::notify("akira/hosts/address_invalid"_i18n);
                    return;
                }
                if (auto moved = target->moveTo(text); !moved) {
                    brls::Application::notify(moved.error() == Host::AddressRefusal::Invalid
                            ? "akira/hosts/address_invalid"_i18n
                            : "akira/hosts/address_not_saved"_i18n);
                    return;
                }
                settings::SettingsManager::getInstance()->store().save();
                brls::Application::notify(brls::getStr("akira/hosts/address_saved", target->getHostName(), text));
                if (HostListTab::currentInstance)
                    HostListTab::currentInstance->updateHostItem(target);
            },
            current.empty() ? "akira/hosts/add_address"_i18n : "akira/hosts/set_address"_i18n,
            "akira/hosts/address_hint"_i18n, 255, current, 0);
    }

    void doTakionPin() {
        const auto versions = host->pinnableTakionVersions();
        const int configured = settings::SettingsManager::getInstance()->developer().takionVersion(
            host->isPS5() ? settings::TakionTarget::PS5 : settings::TakionTarget::PS4);
        const std::string fallback = configured > 0
            ? "v" + std::to_string(configured)
            : "akira/settings/dev_takion_version_auto"_i18n;
        std::vector<std::string> labels{brls::getStr("akira/hosts/takion_follow_settings", fallback)};
        std::ranges::transform(versions, std::back_inserter(labels),
            [](std::uint32_t version) { return "v" + std::to_string(version); });
        const auto pinned = host->getPinnedTakionVersion();
        const auto found = pinned ? std::ranges::find(versions, *pinned) : versions.end();
        const int selected = found == versions.end() ? 0 : static_cast<int>(found - versions.begin()) + 1;
        Host* target = host;
        auto* dropdown = new brls::Dropdown("akira/hosts/takion_version"_i18n, labels,
            [](int) {}, selected,
            [target, versions](int sel) {
                if (sel < 0 || sel > static_cast<int>(versions.size())) return;
                if (sel == 0) {
                    target->pinTakionVersion(std::nullopt);
                    brls::Application::notify(brls::getStr("akira/hosts/takion_follows", target->getHostName()));
                } else {
                    const std::uint32_t version = versions[static_cast<size_t>(sel - 1)];
                    target->pinTakionVersion(version);
                    brls::Application::notify(
                        brls::getStr("akira/hosts/takion_pinned", target->getHostName(), std::to_string(version)));
                }
                settings::SettingsManager::getInstance()->store().save();
            });
        brls::Application::pushActivity(new brls::Activity(dropdown));
    }

    void doDelete() {
            std::string hostName = host->getHostName();
            log::info("Delete button clicked for {}", hostName);

            auto* dialog = new brls::Dialog(brls::getStr("akira/hosts/delete_confirm", hostName));
            dialog->addButton("akira/common/cancel"_i18n, []() {});
            dialog->addButton("akira/common/delete"_i18n, [hostName]() {
                auto* settings = settings::SettingsManager::getInstance();
                auto& core = play::ChiakiCore::current();
                auto* accounts = core.accounts();
                auto hostIt = settings->hosts().entries().find(hostName);
                if (accounts && hostIt != settings->hosts().entries().end() && hostIt->second->getConsole()) {
                    if (const auto user = accounts->activeUser()) {
                        if (!core.chiaki().consoles().unpair(user->account_id(), *hostIt->second->getConsole()))
                            log::error("Could not forget the pairing with {}", hostName);
                    }
                }
                settings->hosts().removeActiveProfileRegistration(hostName);
                settings->store().save();
                brls::Application::notify("akira/hosts/host_deleted"_i18n);
                if (HostListTab::currentInstance) {
                    // If this was the last host, give focus to Find Remote button first
                    // to avoid dangling focus pointer when host list becomes empty
                    if (HostListTab::currentInstance->hostItems.size() <= 1 &&
                        HostListTab::currentInstance->findRemoteBtn) {
                        brls::Application::giveFocus(HostListTab::currentInstance->findRemoteBtn);
                    }
                    HostListTab::currentInstance->syncHostList();
                }
            });
            dialog->open();
    }
};

static void openAddHostFlow() {
    auto* view = AddHostTab::create();
    view->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    }, false);
    auto* frame = new brls::AppletFrame(view);
    frame->setTitle("akira/tabs/add_host"_i18n);
    brls::Application::pushActivity(new brls::Activity(frame));
}

class AddHostCard : public brls::Box {
public:
    AddHostCard() {
        this->setAxis(brls::Axis::COLUMN);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setJustifyContent(brls::JustifyContent::CENTER);
        this->setWidth(174);
        this->setHeight(238);
        this->setPadding(14);
        this->setMarginRight(14);
        this->setMarginBottom(14);
        this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
        this->setCornerRadius(14);
        this->setBorderColor(app::views::withAlpha(app::views::active().accent, 0x66));
        this->setBorderThickness(2);
        this->setFocusable(true);

        auto* plus = new brls::Label();
        plus->setText("+");
        plus->setFontSize(64);
        plus->setTextColor(app::views::active().accent);
        plus->setMarginBottom(6);
        this->addView(plus);

        auto* label = new brls::Label();
        label->setText("akira/add_host/add_console"_i18n);
        label->setFontSize(22);
        label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        label->setTextColor(app::views::active().textMuted);
        this->addView(label);

        this->registerClickAction([](brls::View*) {
            openAddHostFlow();
            return true;
        });
        app::views::motion::liftOnFocus(this, focusAnim);
    }

private:
    brls::Animatable focusAnim{0.0f};
};

class SetupAccountCard : public brls::Box {
public:
    SetupAccountCard() {
        this->setAxis(brls::Axis::COLUMN);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setJustifyContent(brls::JustifyContent::CENTER);
        this->setWidth(300);
        this->setHeight(238);
        this->setPadding(20);
        this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
        this->setCornerRadius(14);
        this->setBorderColor(app::views::withAlpha(app::views::active().accent, 0x66));
        this->setBorderThickness(2);
        this->setFocusable(true);

        auto* glyph = new brls::Label();
        glyph->setText("\xEE\xA1\x93");
        glyph->setFontSize(64);
        glyph->setTextColor(app::views::active().accent);
        glyph->setMarginBottom(10);
        this->addView(glyph);

        auto* label = new brls::Label();
        label->setText("akira/hosts/setup_account"_i18n);
        label->setFontSize(22);
        label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        label->setTextColor(app::views::active().text);
        this->addView(label);

        auto* hint = new brls::Label();
        hint->setText("akira/hosts/setup_account_hint"_i18n);
        hint->setFontSize(15);
        hint->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        hint->setTextColor(app::views::active().textMuted);
        hint->setMarginTop(6);
        this->addView(hint);

        this->registerClickAction([](brls::View*) {
            brls::Application::pushActivity(new brls::Activity(new accounts::views::ProfileSetupView()));
            return true;
        });
        app::views::motion::liftOnFocus(this, focusAnim);
    }

private:
    brls::Animatable focusAnim{0.0f};
};

#if AKIRA_CLOUD
class CloudCard : public brls::Box {
public:
    CloudCard()
    {
        cloud::Snapshot snapshot = cloud::Service::instance().snapshotForActiveProfile();
        const auto& pal = app::views::active();

        this->setAxis(brls::Axis::COLUMN);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setJustifyContent(brls::JustifyContent::FLEX_START);
        this->setWidth(174);
        this->setHeight(238);
        this->setPadding(16);
        this->setMarginRight(14);
        this->setMarginBottom(14);
        this->setCornerRadius(14);
        this->setFocusable(true);

        std::string pillText = "akira/cloud/pill_unavailable"_i18n;
        NVGcolor pillColor = pal.textDim;
        std::string subText = snapshot.status.detail;
        bool active = true;

        switch (snapshot.status.availability)
        {
            case cloud::Availability::Ready:
                pillText = "akira/cloud/pill_ready"_i18n;
                pillColor = pal.success;
                subText = brls::getStr("akira/cloud/card_count", snapshot.status.gameCount);
                break;
            case cloud::Availability::CatalogAvailable:
                pillText = "akira/cloud/pill_catalog"_i18n;
                pillColor = pal.warning;
                subText = "akira/cloud/card_eligibility"_i18n;
                break;
            case cloud::Availability::SubscriptionRequired:
                pillText = "akira/cloud/pill_unavailable"_i18n;
                pillColor = pal.danger;
                subText = "akira/cloud/card_subscription"_i18n;
                break;
            case cloud::Availability::Checking:
                pillText = "akira/cloud/pill_checking"_i18n;
                pillColor = pal.accent;
                subText = "akira/cloud/card_checking"_i18n;
                break;
            case cloud::Availability::NeedsPairing:
                pillText = "akira/cloud/pill_pair"_i18n;
                pillColor = pal.accent;
                subText = "akira/cloud/card_link"_i18n;
                break;
            case cloud::Availability::Warning:
            case cloud::Availability::LaunchBlocked:
                pillText = "akira/cloud/pill_limited"_i18n;
                pillColor = pal.warning;
                subText = snapshot.status.canPair ? "akira/cloud/card_relink"_i18n : "akira/cloud/card_limited"_i18n;
                break;
            case cloud::Availability::Error:
                pillText = "akira/cloud/pill_offline"_i18n;
                pillColor = pal.danger;
                subText = "akira/cloud/card_offline"_i18n;
                break;
            case cloud::Availability::Empty:
                pillColor = pal.textDim;
                subText = "akira/cloud/card_none"_i18n;
                active = false;
                break;
            case cloud::Availability::NoProfile:
            default:
                active = false;
                break;
        }

        this->setBackgroundColor(active
            ? app::views::withAlpha(pal.accentStrong, 0x24)
            : pal.surface);
        if (active)
        {
            this->setBorderColor(app::views::withAlpha(pal.accent, 0x55));
            this->setBorderThickness(1);
        }

        auto* glyph = new brls::Label();
        glyph->setText("\xEE\x8A\xBD");
        glyph->setFontSize(44);
        glyph->setTextColor(active ? pal.accent : pal.textDim);
        glyph->setMarginTop(4);
        glyph->setMarginBottom(8);
        glyph->setShrink(0);
        this->addView(glyph);

        auto* name = new brls::Label();
        name->setText("akira/cloud/card_badge"_i18n);
        name->setFontSize(20);
        name->setWidth(142);
        name->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        name->setTextColor(active ? pal.text : pal.textMuted);
        name->setMarginBottom(8);
        name->setShrink(0);
        this->addView(name);

        auto* pill = new brls::Box();
        pill->setAxis(brls::Axis::ROW);
        pill->setCornerRadius(11);
        pill->setPaddingLeft(11);
        pill->setPaddingRight(11);
        pill->setPaddingTop(2);
        pill->setPaddingBottom(3);
        pill->setBackgroundColor(app::views::withAlpha(pillColor, 0x2e));
        pill->setShrink(0);
        auto* pillLabel = new brls::Label();
        pillLabel->setText(pillText);
        pillLabel->setFontSize(16);
        pillLabel->setTextColor(pillColor);
        pill->addView(pillLabel);
        this->addView(pill);

        if (!subText.empty())
        {
            auto* sub = new brls::Label();
            sub->setText(subText);
            sub->setFontSize(14);
            sub->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            sub->setTextColor(pal.textDim);
            sub->setMarginTop(8);
            sub->setWidth(142);
            sub->setShrink(0);
            this->addView(sub);
        }

        this->registerClickAction([snapshot](brls::View*) {
            if (snapshot.status.canPair && snapshot.status.degraded)
            {
                brls::Application::pushActivity(new brls::Activity(new accounts::views::QrSignInView()));
                return true;
            }
            if (snapshot.status.canPair && !snapshot.status.canBrowse)
            {
                brls::Application::pushActivity(new brls::Activity(new accounts::views::ProfileSetupView()));
                return true;
            }

            auto* frame = new brls::AppletFrame(new cloud::views::LibraryView());
            app::views::AkiraHeader::decorate(frame);
            app::views::AkiraHeader::registerTabActions(frame);
            brls::Application::pushActivity(new brls::Activity(frame));
            return true;
        });

        app::views::motion::liftOnFocus(this, focusAnim);
    }

private:
    brls::Animatable focusAnim{0.0f};
};
#endif


HostListTab::HostListTab() {
    currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/host_list.xml");

    settings = settings::SettingsManager::getInstance();
    discovery = DiscoveryManager::getInstance();

    discovery->setOnHostDiscovered([](Host* host) {
        if (!host) {
            log::warning("onHostDiscovered: host is null");
            return;
        }
        log::info("onHostDiscovered: isActive={}, currentInstance={}",
            HostListTab::isActive ? "true" : "false",
            HostListTab::currentInstance ? "valid" : "null");
        if (!HostListTab::isActive || HostListTab::connectionActive) {
            log::info("onHostDiscovered: suspended, skipping {}", host->getHostName());
            return;
        }
        if (!HostListTab::currentInstance) {
            log::warning("onHostDiscovered: currentInstance is null");
            return;
        }
        log::info("Host discovered/updated: {}", host->getHostName());
        HostListTab::currentInstance->updateHostItem(host);
    });

    discovery->setOnHostsChanged([]() {
        if (HostListTab::isActive && HostListTab::currentInstance
            && !HostListTab::connectionActive)
            HostListTab::currentInstance->syncHostList();
    });

    if (!discovery->isServiceEnabled()) {
        discovery->setServiceEnabled(true);
    }

    initFindRemoteButton();

    profileChip = new accounts::views::ProfileChipView();
    chipSlot->addView(profileChip);
    profileChip->registerClickAction([this](brls::View*) {
        const auto& profiles = settings->accounts().profiles();
        if (profiles.empty()) {
            brls::Application::notify("akira/hosts/no_profiles_add"_i18n);
            return true;
        }

#if AKIRA_CLOUD
        if (cloud::Service::instance().snapshotForActiveProfile().status.availability
                == cloud::Availability::Checking) {
            brls::Application::notify("akira/cloud/checking_wait"_i18n);
            return true;
        }
#endif

        std::vector<std::string> names;
        std::vector<int64_t> ids;
        int selected = 0;
        for (size_t i = 0; i < profiles.size(); i++) {
            std::string lbl = profiles[i].label();
            if (lbl.empty())
                lbl = "akira/settings/unnamed_profile"_i18n;
            else
                lbl = settings->app().maskAccountName(lbl);
            names.push_back(lbl);
            ids.push_back(profiles[i].id);
            if (profiles[i].id == settings->accounts().activeProfileId())
                selected = static_cast<int>(i);
        }

        auto* dropdown = new brls::Dropdown(
            "akira/hosts/switch_profile"_i18n, names,
            [ids](int sel) {
                if (sel < 0 || sel >= static_cast<int>(ids.size()))
                    return;
                auto* s = settings::SettingsManager::getInstance();
                s->accounts().setActiveProfileId(ids[sel]);
                s->store().save();
                trophies::TrophyManager::getInstance()->onActiveProfileChanged();
                brls::sync([] { accounts::views::QrSignInView::openIfSignInExpired(); });
#if AKIRA_CLOUD
                cloud::Service::instance().refreshActiveProfile(false,
                    [](const cloud::Snapshot&) {
                        if (!HostListTab::currentInstance)
                            return;
                        HostListTab::currentInstance->syncHostList();
                        if (HostListTab::currentInstance->profileChip)
                            HostListTab::currentInstance->profileChip->refresh();
                        if (HostListTab::currentInstance->shortcutsRail)
                            HostListTab::currentInstance->shortcutsRail->refresh();
                    });
#endif
                brls::sync([]() {
                    if (HostListTab::currentInstance) {
                        HostListTab::currentInstance->syncHostList();
                        if (HostListTab::currentInstance->profileChip)
                            HostListTab::currentInstance->profileChip->refresh();
#if AKIRA_CLOUD
                        if (HostListTab::currentInstance->shortcutsRail)
                            HostListTab::currentInstance->shortcutsRail->refresh();
#endif
                    }
                });
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    applyProfileMode();

    syncHostList();
}

/*
 * Which of these surfaces exist depends on the active profile, and this tab outlives any
 * number of profile switches - so the decision cannot live in the constructor. Idempotent
 * on purpose: called once at construction and again on every switch.
 */
void HostListTab::applyProfileMode() {
    const bool legacy = settings->accounts().isLegacyProfileActive();

    if (findRemoteBtn)
        findRemoteBtn->setVisibility(legacy ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

#if AKIRA_CLOUD
    /* Cloud needs an npsso: a permanent "pair to use this" card is a dead end. */
    if (legacy) {
        if (shortcutsRail) {
            railSlot->clearViews();
            shortcutsRail = nullptr;
        }
        return;
    }

    if (shortcutsRail)
        return;

    shortcutsRail = new cloud::views::CloudShortcutsRail();
    shortcutsRail->setLaunchHandler([](const cloud::Game& game) {
        const cloud::Snapshot snapshot = cloud::Service::instance().snapshotForActiveProfile();
        if (snapshot.status.plusMembership == cloud::PlusMembership::None)
        {
            auto* dialog = new brls::Dialog("akira/cloud/status_subscription_detail"_i18n);
            dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
            dialog->open();
            return;
        }
        HostListTab::connectionActive = true;
        play::views::startConnecting(std::make_unique<cloud::CloudConnectTask>(game));
    });
    shortcutsRail->setLeadingCard([]() -> brls::View* { return new CloudCard(); });
    railSlot->addView(shortcutsRail);

    cloud::Service::instance().refreshActiveProfile(false,
        [](const cloud::Snapshot&) {
            if (!HostListTab::currentInstance)
                return;
            HostListTab::currentInstance->syncHostList();
            if (HostListTab::currentInstance->profileChip)
                HostListTab::currentInstance->profileChip->refresh();
            if (HostListTab::currentInstance->shortcutsRail)
                HostListTab::currentInstance->shortcutsRail->refresh();
        });
#endif
}

brls::View* HostListTab::getDefaultFocus() {
    if (emptyActionCard)
        return emptyActionCard->getDefaultFocus();
    return brls::Box::getDefaultFocus();
}

void HostListTab::connectToHost(Host* host) {
    if (!host || HostListTab::isConnecting)
        return;

    log::info("Connect to {}", host->getHostName());

    auto* settings = settings::SettingsManager::getInstance();
    bool isPS5 = host->isPS5();
    bool needsAccountId = host->needsAccountId();
    std::string accountId = settings->accounts().psnAccountId();
    std::string onlineId = settings->accounts().psnOnlineId();

    if (host->isPsnRemotePlayDisabled()) {
        log::warning("Connect {}: PSN reports remote play disabled for this account",
            host->getHostName());
        brls::Application::notify("akira/connection/fail_rp_disabled"_i18n);
    }

    if (needsAccountId && accountId.empty()) {
        auto* dialog = new brls::Dialog("akira/hosts/psn_account_id_required"_i18n);
        dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
        dialog->open();
        return;
    }

    if (!needsAccountId && onlineId.empty()) {
        auto* dialog = new brls::Dialog("akira/hosts/psn_online_id_required"_i18n);
        dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
        dialog->open();
        return;
    }

    HostListTab::isConnecting = true;
    HostListTab::connectionActive = true;

    /* Local and remote alike. A local console resolves immediately, so the
     * screen is barely seen - but the activity stack ends up the same shape it
     * does for every other route, which is the point. */
    play::views::startConnecting(std::make_unique<play::views::HostConnectTask>(host));

    HostListTab::isConnecting = false;
}

void HostListTab::initFindRemoteButton() {
    /*
     * Wired unconditionally; applyProfileMode decides whether it is on screen. Find
     * Remote is a PSN device lookup, so legacy hides it rather than offering a button
     * that always fails.
     */
    findRemoteBtn->setStyle(&BUTTONSTYLE_BLUE);

    findRemoteGate.attach(
        findRemoteBtn,
        app::views::active().accent,
        "akira/hosts/find_remote"_i18n,
        "akira/hosts/find_remote_busy"_i18n,
        "akira/hosts/find_remote_wait",
        []() { return DiscoveryManager::getInstance()->getRemoteRefreshStatus(); }
    );

    findRemoteBtn->registerClickAction([this](brls::View* view) {
        if (!findRemoteGate.isReady()) {
            return true;
        }

        auto* accounts = play::ChiakiCore::current().accounts();
        const accounts::SessionState state = accounts ? accounts->state() : accounts::SessionState::NotLinked;
        if (state == accounts::SessionState::NotLinked) {
            brls::Application::notify("akira/hosts/no_psn_token"_i18n);
            return true;
        }

        auto onComplete = [](const DiscoveryManager::RemoteRefreshResult& result) {
            if (!result.success) {
                brls::Application::notify(brls::getStr("akira/hosts/token_refresh_failed", result.message));
            }

            if (!HostListTab::currentInstance) {
                return;
            }
            HostListTab::currentInstance->syncHostList();
            if (HostListTab::currentInstance->findRemoteBtn) {
                brls::Application::giveFocus(HostListTab::currentInstance->findRemoteBtn);
            }
        };

        if (state == accounts::SessionState::Valid) {
            brls::Application::notify("akira/hosts/finding_remote"_i18n);
        } else {
            brls::Application::notify("akira/hosts/token_expired_refreshing"_i18n);
        }

        discovery->refreshRemoteDevices(onComplete, true);
        findRemoteGate.apply();

        return true;
    });
}

HostListTab::~HostListTab() {
    if (currentInstance == this) {
        currentInstance = nullptr;
    }

    hostItems.clear();

    discovery->setOnHostDiscovered(nullptr);
    discovery->setOnHostsChanged(nullptr);
}

void HostListTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    isActive = true;
    log::debug("HostListTab::willAppear - resuming discovery callbacks");
    findRemoteGate.start();
    if (profileChip) profileChip->refresh();
#if AKIRA_CLOUD
    if (shortcutsRail) shortcutsRail->refresh();
    cloud::Service::instance().refreshActiveProfile(false,
        [](const cloud::Snapshot&) {
            if (!HostListTab::currentInstance)
                return;
            HostListTab::currentInstance->syncHostList();
            if (HostListTab::currentInstance->profileChip)
                HostListTab::currentInstance->profileChip->refresh();
            if (HostListTab::currentInstance->shortcutsRail)
                HostListTab::currentInstance->shortcutsRail->refresh();
        });
#endif
    syncHostList();
}

void HostListTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    isActive = false;
    log::debug("HostListTab::willDisappear - pausing discovery callbacks");
    findRemoteGate.stop();
}

brls::View* HostListTab::create() {
    return new HostListTab();
}

void HostListTab::refreshRailsIfActive() {
    HostListTab::connectionActive = false;
    if (!currentInstance)
        return;
    brls::sync([]() {
        if (!currentInstance)
            return;
#if AKIRA_CLOUD
        if (currentInstance->shortcutsRail)
            currentInstance->shortcutsRail->refresh();
#endif

        brls::View* f = brls::Application::getCurrentFocus();
        bool underHome = false;
        for (brls::View* v = f; v; v = v->getParent())
            if (v == currentInstance) { underHome = true; break; }
        log::info("refreshRailsIfActive: focus={} isActive={} underHome={} stack={}",
            f ? f->describe() : std::string("null"),
            currentInstance->isActive ? "yes" : "no",
            underHome ? "yes" : "no",
            (int)brls::Application::getActivitiesStack().size());
        /*
         * Only while this really is the screen in front.
         *
         * isActive says the home tab is the selected tab, not that its activity
         * is on top - and this runs from onResume through brls::sync, a frame
         * later. The remote route pops the connection view and pushes the
         * stream view in the same breath, so the pop fires onResume, and by the
         * time the callback runs the stream view and the controller picker are
         * both above us. The old guard then read a focus it did not recognise
         * as its own and took it back, leaving the picker on screen with focus
         * four activities beneath it - where L and R are the home tab's, and
         * open trophies out from under the stream.
         */
        const auto stack = brls::Application::getActivitiesStack();
        const bool onTop = !stack.empty() &&
                           currentInstance->getParentActivity() == stack.back();

        if (currentInstance->isActive && !underHome && onTop)
            brls::Application::giveFocus(currentInstance);
    });
}

void HostListTab::notifyActiveProfileChanged() {
#if AKIRA_CLOUD
    cloud::Service::instance().refreshActiveProfile(false);
#endif
    if (!currentInstance)
        return;
    currentInstance->applyProfileMode();
    currentInstance->syncHostList();
    if (currentInstance->profileChip)
        currentInstance->profileChip->refresh();
#if AKIRA_CLOUD
    if (currentInstance->shortcutsRail)
        currentInstance->shortcutsRail->refresh();
#endif
}

void HostListTab::notifyAccountNameDisplayChanged() {
    if (!currentInstance || !currentInstance->profileChip)
        return;
    currentInstance->profileChip->refresh();
}

void HostListTab::syncHostList() {
    log::debug("Syncing host list...");

    if (!hostContainer) {
        log::error("syncHostList: hostContainer is null");
        return;
    }
    brls::View* focused = brls::Application::getCurrentFocus();
    bool childFocused = false;
    for (brls::View* v = focused; v; v = v->getParent()) {
        if (v == this) {
            childFocused = true;
            break;
        }
    }

    // borealis really hates it when you remove a view.
    // just rebuild the damn thing.
    if (childFocused)
        brls::Application::giveFocus(this);

    hostContainer->clearViews();
    hostItems.clear();

    /*
     * activeRegistration() keys off the active profile, so with no profile every host
     * reports hasRpKey() == false and none of them can be connected to or registered.
     * Listing a console the user cannot use is worse than not listing it: the whole
     * screen becomes the same starting state a fresh install shows.
     */
    const bool noProfile = settings->accounts().profiles().empty();
    if (noProfile)
        entrancePlayed = false;

    bool animateEntrance = !entrancePlayed;
    int shownIndex = 0;

    const int perRow = 6;
    int col = 0;
    brls::Box* row = nullptr;
    auto* hostsMap = &settings->hosts().entries();
    if (hostsMap && !noProfile) {
        for (auto& [name, host] : *hostsMap) {
            if (!host) {
                log::error("Null host in hosts map for key: {}", name);
                continue;
            }
            if (!host->hasRpKey() && !host->isDiscovered() && !host->isManual())
                continue;
            /*
             * A profile converted from a normal one keeps its "(Remote)" entries in the
             * toml. They are inert without a token, but they would still list and then
             * dead-end on click.
             */
            if (host->isRemote() && settings->accounts().isLegacyProfileActive())
                continue;
            if (col == 0) {
                row = new brls::Box();
                row->setAxis(brls::Axis::ROW);
                hostContainer->addView(row);
            }
            auto* item = new HostItemView(host.get());
            hostItems[host.get()] = item;
            row->addView(item);
            if (animateEntrance)
                item->playEntrance(shownIndex * 45);
            shownIndex++;
            col = (col + 1) % perRow;
        }
    }

    bool hasHosts = !hostItems.empty();

    if (settings->developer().fakeHosts()) {
        static std::vector<Host*> fakeHosts;
        if (fakeHosts.empty()) {
            for (int i = 1; i <= 20; i++) {
                Host* h = new Host(HostRecord{.nickname = "PS5-Debug-" + std::to_string(i)}, settings->accounts(), settings->app());
                h->setFirmware(Host::Firmware::newest(libchiaking::consoles::Generation::ps5));
                h->setPresence((i % 4 == 0) ? Host::Presence::rest_mode : Host::Presence::ready);
                h->setHostType((i % 2 == 0) ? HostType::Auto : HostType::Manual);
                fakeHosts.push_back(h);
            }
        }
        for (Host* h : fakeHosts) {
            if (col == 0) {
                row = new brls::Box();
                row->setAxis(brls::Axis::ROW);
                hostContainer->addView(row);
            }
            row->addView(new HostItemView(h));
            col = (col + 1) % perRow;
        }
        hasHosts = true;
    }

    if (hasHosts && animateEntrance)
        entrancePlayed = true;

    bool hasProfile = !noProfile;

    emptyActionCard = nullptr;

    if (hasHosts) {
        if (hasProfile) {
            if (col == 0 || !row) {
                row = new brls::Box();
                row->setAxis(brls::Axis::ROW);
                hostContainer->addView(row);
            }
            row->addView(new AddHostCard());
#if AKIRA_CLOUD
            for (auto* child : row->getChildren())
                child->setCustomNavigationRoute(brls::FocusDirection::DOWN, "cloud/rail/lead");
#endif
        }
    } else {
        auto* cardRow = new brls::Box();
        cardRow->setAxis(brls::Axis::ROW);
        cardRow->setWidthPercentage(100.0f);
        cardRow->setJustifyContent(brls::JustifyContent::CENTER);
        hostContainer->addView(cardRow);
        if (hasProfile) {
            auto* addHost = new AddHostCard();
            cardRow->addView(addHost);
            emptyActionCard = addHost;
        } else {
            auto* card = new SetupAccountCard();
            cardRow->addView(card);
            emptyActionCard = card;
        }
    }

    if (emptyMessage) {
        emptyMessage->setVisibility((!hasHosts && hasProfile) ? brls::Visibility::VISIBLE
                                                              : brls::Visibility::GONE);
    }

    if (childFocused || noProfile) {
        if (hasHosts) {
            brls::Application::giveFocus(hostContainer);
        } else if (emptyActionCard) {
            brls::Application::giveFocus(emptyActionCard);
        } else if (findRemoteBtn) {
            brls::Application::giveFocus(findRemoteBtn);
        }
    }

    log::debug("Host list sync complete, {} hosts", hostItems.size());
}

void HostListTab::updateHostItem(Host* host) {
    if (!host) return;

    auto it = hostItems.find(host);
    if (it != hostItems.end()) {
        it->second->updateState();
        log::debug("Updated host item: {}", host->getHostName());
    } else {
        log::debug("Host not found in items, syncing: {}", host->getHostName());
        syncHostList();
    }
}

}
