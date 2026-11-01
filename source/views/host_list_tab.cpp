#include "views/host_list_tab.hpp"
#include "cloud/library_view.hpp"
#include "cloud/cloud_connect_task.hpp"
#include "ui/akira_header.hpp"
#include "cloud/service.hpp"
#include "ui/theme.hpp"
#include "ui/log_pane.hpp"
#include "ui/motion.hpp"
#include "views/stream_view.hpp"
#include "views/connecting_view.hpp"
#include "views/host_connect_task.hpp"
#include "views/enter_pin_view.hpp"
#include "views/pair_view.hpp"
#include "views/profile_setup_view.hpp"
#include "core/host.hpp"
#include "core/trophy_manager.hpp"
#include "psn/auth.hpp"
#include "psn/models.hpp"
#include "util/http_pool.hpp"
#include "stream/session.hpp"
#include "util/shared_view_holder.hpp"

#include <borealis/core/i18n.hpp>
using namespace brls::literals;

#include <ctime>
#include <functional>
#include <algorithm>
#include <cctype>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>

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
    akira::ui::LogPane logPane;
};

static void setAutoRegStage(int index, int total) {
    if (!s_autoRegStageLabel)
        return;
    if (!SettingsManager::getInstance()->getConnectionShowStages())
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

		std::string imgPath = "img/console/ps5_fat.png";
		switch (host->getConsoleModel()) {
			case ConsoleModel::PS4Fat:
				imgPath = "img/console/ps4_fat.png";
				break;
			case ConsoleModel::PS4Slim:
				imgPath = "img/console/ps4_slim.png";
				break;
			case ConsoleModel::PS4Pro:
				imgPath = "img/console/ps4_pro.png";
				break;
			case ConsoleModel::PS5Fat:
				imgPath = "img/console/ps5_fat.png";
				break;
			case ConsoleModel::PS5Slim:
				imgPath = "img/console/ps5_slim.png";
				break;
			case ConsoleModel::PS5Pro:
				imgPath = "img/console/ps5_pro.png";
				break;
		}

		consoleImg = new brls::Image();
		consoleImg->setImageFromRes(imgPath);
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

        akira::ui::motion::liftOnFocus(this, focusAnim);

        updateState();
    }

    void playEntrance(int delayMs) {
        akira::ui::motion::fadeIn(this, entranceAnim, delayMs, 300);
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
            typeColor = akira::ui::active().accentStrong;
        } else if (host->isManual()) {
            typeText = "akira/hosts/manual"_i18n;
            typeColor = akira::ui::active().media;
        } else if (host->isAuto()) {
            typeText = "akira/hosts/auto"_i18n;
            typeColor = akira::ui::active().accent;
        } else {
            typeText = "akira/hosts/local"_i18n;
            typeColor = akira::ui::active().textMuted;
        }
        typeLabel->setText(typeText);
        typeLabel->setTextColor(typeColor);
        typePill->setBackgroundColor(akira::ui::withAlpha(typeColor, 0x2e));

        std::string pillText;
        NVGcolor fg = akira::ui::active().textMuted;
        bool dim = true;
        if (host->isRemote() && host->needsLink()) {
            pillText = "akira/hosts/link"_i18n;
            fg = akira::ui::active().accent;
        } else if (host->usesTransientPsnRegistration()) {
            pillText = "akira/hosts/ready"_i18n;
            fg = akira::ui::active().success;
            dim = false;
        } else if (!host->hasRpKey() && (host->isDiscovered() || host->canAutoRegister())) {
            pillText = "akira/hosts/register"_i18n;
            fg = akira::ui::active().accent;
        } else if (host->isStandby() && host->hasRpKey()) {
            pillText = "akira/hosts/standby"_i18n;
        } else if ((host->hasRpKey() && host->isReady()) || (host->isRemote() && host->hasRpKey())) {
            pillText = "akira/hosts/ready"_i18n;
            fg = akira::ui::active().success;
            dim = false;
        } else {
            pillText = host->getStateString();
        }
        statusLabel->setText(pillText);
        statusLabel->setTextColor(fg);
        statusPill->setBackgroundColor(akira::ui::withAlpha(fg, 0x2e));

        int wantDim = dim ? 1 : 0;
        if (wantDim != lastDim) {
            float target = dim ? 0.5f : 1.0f;
            if (lastDim < 0) {
                consoleImg->setAlpha(target);
                dimAnim.reset(target);
            } else {
                akira::ui::motion::animate(dimAnim, dimAnim.getValue(), target, 260,
                    brls::EasingFunction::quadraticOut,
                    [this](float v) { consoleImg->setAlpha(v); });
            }
            lastDim = wantDim;
        }
    }

    Host* getHost() { return host; }
    std::string getHostName() const { return hostName; }

    void doPrimaryAction() {
        if (host->isRemote() && host->needsLink()) doLink();
        else if (host->usesTransientPsnRegistration()) doConnect();
        else if (!host->hasRpKey() && (host->isDiscovered() || host->canAutoRegister())) doRegister();
        else if (host->isStandby() && host->hasRpKey() && !host->isRemote()) doWake();
        else if (host->hasRpKey()) doConnect();
        else brls::Logger::warning("No primary action for {} (remote={}, needsLink={}, discovered={}, rpkey={}, canAuto={})",
            host->getHostName(), host->isRemote(), host->needsLink(), host->isDiscovered(), host->hasRpKey(), host->canAutoRegister());
    }

    void openContextMenu() {
        std::vector<std::string> options;
        std::vector<int> ids;
        if (host->hasRpKey() && !host->isRemote()) {
            options.push_back("akira/host_settings/console_pin"_i18n);
            ids.push_back(0);
        }
        if (host->isRegistered()) {
            options.push_back("akira/hosts/delete"_i18n);
            ids.push_back(1);
        }
        if (options.empty()) return;
        auto* dropdown = new brls::Dropdown("akira/hosts/options"_i18n, options,
            [](int) {}, 0,
            [this, ids](int sel) {
                if (sel < 0 || sel >= static_cast<int>(ids.size())) return;
                if (ids[sel] == 0) doConsolePIN();
                else doDelete();
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
            brls::Logger::info("Wake {}", host->getHostName());

            int result = host->wakeup();
            if (result == 0) {
                brls::Application::notify(brls::getStr("akira/hosts/wake_sent", host->getHostName()));
            } else {
                brls::Application::notify(brls::getStr("akira/hosts/wake_failed", host->getHostName()));
            }
    }

    void doRegister() {
            if (HostListTab::isRegistering) {
                return;
            }

            brls::Logger::info("Register button clicked for {}", host->getHostName());

            auto* settings = SettingsManager::getInstance();
            brls::Logger::info("Register {}: remoteDuid='{}' hasToken={} hasAccountId={} canAuto={}",
                host->getHostName(), host->getRemoteDuid(),
                !settings->getPsnAccessToken().empty(),
                !settings->getPsnAccountId(host).empty(), host->canAutoRegister());
            bool isPS5 = host->isPS5();
            bool needsAccountId = isPS5 || host->getChiakiTarget() >= CHIAKI_TARGET_PS4_9;
            std::string accountId = settings->getPsnAccountId(host);
            std::string onlineId = settings->getPsnOnlineId(host);

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

            if (host->canAutoRegister()) {
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

        host->setOnRegistSuccess([hostPtr]() {
            brls::sync([hostPtr]() {
                hostPtr->finiRegist();
                HostListTab::isRegistering = false;
                brls::Application::notify("akira/hosts/registration_success"_i18n);
                SettingsManager::getInstance()->writeFile();
                if (HostListTab::currentInstance) {
                    HostListTab::currentInstance->updateHostItem(hostPtr);
                }
            });
        });

        host->setOnRegistFailed([hostPtr]() {
            brls::sync([hostPtr]() {
                hostPtr->finiRegist();
                HostListTab::isRegistering = false;
                brls::Application::notify("akira/hosts/registration_failed"_i18n);
            });
        });

        host->setOnRegistCanceled([hostPtr]() {
            brls::sync([hostPtr]() {
                hostPtr->finiRegist();
                HostListTab::isRegistering = false;
                brls::Application::notify("akira/hosts/registration_canceled"_i18n);
            });
        });

        auto* pinView = new EnterPinView(host, PinViewType::Registration);
        pinView->setOnCancel([]() {
            HostListTab::isRegistering = false;
            brls::Logger::info("PIN entry cancelled");
        });
        pinView->setOnPinEntered([hostPtr](const std::string& pin) {
            HostListTab::isRegistering = true;
            int pinValue = std::atoi(pin.c_str());
            int result = hostPtr->registerHost(pinValue);
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
        bool showStages = SettingsManager::getInstance()->getConnectionShowStages();

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
                host->cancelHolepunch();
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

        host->setOnRegistSuccess([hostPtr]() {
            brls::sync([hostPtr]() {
                hostPtr->finiRegist();
                HostListTab::isRegistering = false;
                dismissAutoRegProgress();
                brls::Application::notify("akira/hosts/registration_success"_i18n);
                SettingsManager::getInstance()->writeFile();
                if (HostListTab::currentInstance) {
                    HostListTab::currentInstance->updateHostItem(hostPtr);
                }
            });
        });

        host->setOnRegistFailed([hostPtr]() {
            brls::sync([hostPtr]() {
                hostPtr->finiRegist();
                HostListTab::isRegistering = false;
                dismissAutoRegProgress();
                if (s_autoRegCanceled) {
                    return;
                }
                brls::Application::notify("akira/hosts/auto_register_fallback"_i18n);
                startPinRegistration(hostPtr);
            });
        });

        host->setOnRegistCanceled([hostPtr]() {
            brls::sync([hostPtr]() {
                hostPtr->finiRegist();
                HostListTab::isRegistering = false;
                dismissAutoRegProgress();
            });
        });

        host->setOnRegistStage([](int index, int total) {
            brls::sync([index, total]() {
                setAutoRegStage(index, total);
            });
        });

        showAutoRegProgress(host);
        setAutoRegStage(1, 4);

        HttpPool::instance().submit([hostPtr](HttpSession& session) {
            int result = HOST_REGISTER_ERROR_HOLEPUNCH;
            try {
                if (!psn::Auth::instance().tokenValid()) {
                    brls::Logger::info("Auto-register: token stale, refreshing before holepunch");
                    psn::Auth::instance().ensureSession(session, false);
                    brls::Logger::info("Auto-register: token refresh returned (valid={})",
                        psn::Auth::instance().tokenValid());
                }
                result = hostPtr->registerHostAuto();
            } catch (const std::exception& e) {
                brls::Logger::error("Auto-register worker threw: {}", e.what());
            } catch (...) {
                brls::Logger::error("Auto-register worker threw unknown exception");
            }

            if (result != HOST_REGISTER_OK) {
                brls::sync([hostPtr]() {
                    HostListTab::isRegistering = false;
                    dismissAutoRegProgress();
                    if (s_autoRegCanceled) {
                        return;
                    }
                    brls::Application::notify("akira/hosts/auto_register_fallback"_i18n);
                    startPinRegistration(hostPtr);
                });
            }
        });
    }

    void doLink() {
            brls::Logger::info("Link button clicked for {}", host->getHostName());

            auto* settings = SettingsManager::getInstance();
            auto* hostsMap = settings->getHostsMap();

            std::vector<std::string> hostNames;
            std::vector<Host*> hostPtrs;
            for (auto& [name, h] : *hostsMap) {
                if (h && h->hasRpKey() && !h->isRemote()) {
                    hostNames.push_back(name);
                    hostPtrs.push_back(h.get());
                }
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
                    auto* settings = SettingsManager::getInstance();

                    std::string remoteName = remoteHost->getHostName();
                    if (remoteName.length() > 9 && remoteName.substr(remoteName.length() - 9) == " (Remote)") {
                        remoteName = remoteName.substr(0, remoteName.length() - 9);
                    }

                    if (oldName != remoteName) {
                        settings->renameHost(oldName, remoteName);
                    }
                    remoteHost->copyRegistrationFrom(localHost);
                    remoteHost->setNeedsLink(false);
                    localHost->setRemoteDuid(remoteHost->getRemoteDuid());
                    settings->writeFile();

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
            std::string current = SettingsManager::getInstance()->getConsolePIN(host);
            brls::Application::getImeManager()->openForText(
                [this](std::string text) {
                    if (!text.empty() && (text.length() != 4 || !std::ranges::all_of(text, ::isdigit))) {
                        brls::Application::notify("akira/host_settings/console_pin_invalid"_i18n);
                        return;
                    }
                    SettingsManager::getInstance()->setConsolePIN(host, text);
                    SettingsManager::getInstance()->writeFile();
                    brls::Application::notify("akira/host_settings/console_pin_saved"_i18n);
                },
                "akira/host_settings/console_pin"_i18n, "akira/host_settings/console_pin_hint"_i18n, 4, current, 0);
    }

    void doDelete() {
            std::string hostName = host->getHostName();
            brls::Logger::info("Delete button clicked for {}", hostName);

            auto* dialog = new brls::Dialog(brls::getStr("akira/hosts/delete_confirm", hostName));
            dialog->addButton("akira/common/cancel"_i18n, []() {});
            dialog->addButton("akira/common/delete"_i18n, [hostName]() {
                auto* settings = SettingsManager::getInstance();
                settings->removeActiveProfileRegistration(hostName);
                settings->writeFile();
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
        this->setBorderColor(akira::ui::withAlpha(akira::ui::active().accent, 0x66));
        this->setBorderThickness(2);
        this->setFocusable(true);

        auto* plus = new brls::Label();
        plus->setText("+");
        plus->setFontSize(64);
        plus->setTextColor(akira::ui::active().accent);
        plus->setMarginBottom(6);
        this->addView(plus);

        auto* label = new brls::Label();
        label->setText("akira/add_host/add_console"_i18n);
        label->setFontSize(22);
        label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        label->setTextColor(akira::ui::active().textMuted);
        this->addView(label);

        this->registerClickAction([](brls::View*) {
            openAddHostFlow();
            return true;
        });
        akira::ui::motion::liftOnFocus(this, focusAnim);
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
        this->setBorderColor(akira::ui::withAlpha(akira::ui::active().accent, 0x66));
        this->setBorderThickness(2);
        this->setFocusable(true);

        auto* glyph = new brls::Label();
        glyph->setText("\xEE\xA1\x93");
        glyph->setFontSize(64);
        glyph->setTextColor(akira::ui::active().accent);
        glyph->setMarginBottom(10);
        this->addView(glyph);

        auto* label = new brls::Label();
        label->setText("akira/hosts/setup_account"_i18n);
        label->setFontSize(22);
        label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        label->setTextColor(akira::ui::active().text);
        this->addView(label);

        auto* hint = new brls::Label();
        hint->setText("akira/hosts/setup_account_hint"_i18n);
        hint->setFontSize(15);
        hint->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        hint->setTextColor(akira::ui::active().textMuted);
        hint->setMarginTop(6);
        this->addView(hint);

        this->registerClickAction([](brls::View*) {
            brls::Application::pushActivity(new brls::Activity(new ProfileSetupView()));
            return true;
        });
        akira::ui::motion::liftOnFocus(this, focusAnim);
    }

private:
    brls::Animatable focusAnim{0.0f};
};

class CloudCard : public brls::Box {
public:
    CloudCard()
    {
        cloud::Snapshot snapshot = cloud::Service::instance().snapshotForActiveProfile();
        const auto& pal = akira::ui::active();

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
            ? akira::ui::withAlpha(pal.accentStrong, 0x24)
            : pal.surface);
        if (active)
        {
            this->setBorderColor(akira::ui::withAlpha(pal.accent, 0x55));
            this->setBorderThickness(1);
        }

        auto* glyph = new brls::Label();
        glyph->setText("\xEE\x8A\xBD");
        glyph->setFontSize(52);
        glyph->setTextColor(active ? pal.accent : pal.textDim);
        glyph->setMarginTop(10);
        glyph->setMarginBottom(12);
        this->addView(glyph);

        auto* name = new brls::Label();
        name->setText("akira/cloud/card_badge"_i18n);
        name->setFontSize(20);
        name->setWidth(142);
        name->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        name->setTextColor(active ? pal.text : pal.textMuted);
        name->setMarginBottom(6);
        this->addView(name);

        auto* pill = new brls::Box();
        pill->setAxis(brls::Axis::ROW);
        pill->setCornerRadius(11);
        pill->setPaddingLeft(11);
        pill->setPaddingRight(11);
        pill->setPaddingTop(2);
        pill->setPaddingBottom(3);
        pill->setBackgroundColor(akira::ui::withAlpha(pillColor, 0x2e));
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
            sub->setWidth(140);
            this->addView(sub);
        }

        this->registerClickAction([snapshot](brls::View*) {
            if (snapshot.status.canPair && !snapshot.status.canBrowse)
            {
                brls::Application::pushActivity(new brls::Activity(new ProfileSetupView()));
                return true;
            }

            auto* frame = new brls::AppletFrame(new cloud::LibraryView());
            decorateAkiraHeader(frame);
            registerAkiraTabActions(frame);
            brls::Application::pushActivity(new brls::Activity(frame));
            return true;
        });

        akira::ui::motion::liftOnFocus(this, focusAnim);
    }

private:
    brls::Animatable focusAnim{0.0f};
};


HostListTab::HostListTab() {
    currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/host_list.xml");

    settings = SettingsManager::getInstance();
    discovery = DiscoveryManager::getInstance();

    discovery->setOnHostDiscovered([](Host* host) {
        if (!host) {
            brls::Logger::warning("onHostDiscovered: host is null");
            return;
        }
        brls::Logger::info("onHostDiscovered: isActive={}, currentInstance={}",
            HostListTab::isActive ? "true" : "false",
            HostListTab::currentInstance ? "valid" : "null");
        if (!HostListTab::isActive || HostListTab::connectionActive) {
            brls::Logger::info("onHostDiscovered: suspended, skipping {}", host->getHostName());
            return;
        }
        if (!HostListTab::currentInstance) {
            brls::Logger::warning("onHostDiscovered: currentInstance is null");
            return;
        }
        brls::Logger::info("Host discovered/updated: {}", host->getHostName());
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

    profileChip = new ProfileChipView();
    chipSlot->addView(profileChip);
    profileChip->registerClickAction([this](brls::View*) {
        const auto& profiles = settings->getProfiles();
        if (profiles.empty()) {
            brls::Application::notify("akira/hosts/no_profiles_add"_i18n);
            return true;
        }

        if (cloud::Service::instance().snapshotForActiveProfile().status.availability
                == cloud::Availability::Checking) {
            brls::Application::notify("akira/cloud/checking_wait"_i18n);
            return true;
        }

        std::vector<std::string> names;
        std::vector<int64_t> ids;
        int selected = 0;
        for (size_t i = 0; i < profiles.size(); i++) {
            std::string lbl = profiles[i].label();
            if (lbl.empty())
                lbl = "akira/settings/unnamed_profile"_i18n;
            else
                lbl = settings->maskAccountName(lbl);
            names.push_back(lbl);
            ids.push_back(profiles[i].id);
            if (profiles[i].id == settings->getActiveProfileId())
                selected = static_cast<int>(i);
        }

        auto* dropdown = new brls::Dropdown(
            "akira/hosts/switch_profile"_i18n, names,
            [ids](int sel) {
                if (sel < 0 || sel >= static_cast<int>(ids.size()))
                    return;
                auto* s = SettingsManager::getInstance();
                s->setActiveProfileId(ids[sel]);
                s->writeFile();
                TrophyManager::getInstance()->onActiveProfileChanged();
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
                brls::sync([]() {
                    if (HostListTab::currentInstance) {
                        HostListTab::currentInstance->syncHostList();
                        if (HostListTab::currentInstance->profileChip)
                            HostListTab::currentInstance->profileChip->refresh();
                        if (HostListTab::currentInstance->shortcutsRail)
                            HostListTab::currentInstance->shortcutsRail->refresh();
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
    const bool legacy = settings->isLegacyProfileActive();

    if (findRemoteBtn)
        findRemoteBtn->setVisibility(legacy ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

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

    shortcutsRail = new CloudShortcutsRail();
    shortcutsRail->setLaunchHandler([](const cloud::Game& game) {
        const cloud::Snapshot snapshot = cloud::Service::instance().snapshotForActiveProfile();
        if (snapshot.status.plusMembership == cloud::PlusMembership::None)
        {
            auto* dialog = new brls::Dialog("akira/cloud/status_subscription_detail"_i18n);
            dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
            dialog->open();
            return;
        }
        bool skipAttr = SettingsManager::getInstance()->getCloudAttrPassed();
        HostListTab::connectionActive = true;
        akira::views::startConnecting(std::make_unique<cloud::CloudConnectTask>(game, skipAttr));
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
}

brls::View* HostListTab::getDefaultFocus() {
    if (emptyActionCard)
        return emptyActionCard->getDefaultFocus();
    return brls::Box::getDefaultFocus();
}

void HostListTab::connectToHost(Host* host) {
    if (!host || HostListTab::isConnecting)
        return;

    brls::Logger::info("Connect to {}", host->getHostName());

    auto* settings = SettingsManager::getInstance();
    bool isPS5 = host->isPS5();
    bool needsAccountId = isPS5 || host->getChiakiTarget() >= CHIAKI_TARGET_PS4_9;
    std::string accountId = settings->getPsnAccountId(host);
    std::string onlineId = settings->getPsnOnlineId(host);

    if (host->isPsnRemotePlayDisabled()) {
        brls::Logger::warning("Connect {}: PSN reports remote play disabled for this account",
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
    akira::views::startConnecting(std::make_unique<akira::views::HostConnectTask>(host));

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
        akira::ui::active().accent,
        "akira/hosts/find_remote"_i18n,
        "akira/hosts/find_remote_busy"_i18n,
        "akira/hosts/find_remote_wait",
        []() { return DiscoveryManager::getInstance()->getRemoteRefreshStatus(); }
    );

    findRemoteBtn->registerClickAction([this](brls::View* view) {
        if (!findRemoteGate.isReady()) {
            return true;
        }

        std::string refreshToken = settings->getPsnRefreshToken();

        if (refreshToken.empty()) {
            brls::Application::notify("akira/hosts/no_psn_token"_i18n);
            return true;
        }

        auto onComplete = [](const psn::AuthResult& result) {
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

        if (psn::Auth::instance().tokenValid()) {
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
    brls::Logger::debug("HostListTab::willAppear - resuming discovery callbacks");
    findRemoteGate.start();
    if (profileChip) profileChip->refresh();
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
    syncHostList();
}

void HostListTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    isActive = false;
    brls::Logger::debug("HostListTab::willDisappear - pausing discovery callbacks");
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
        if (currentInstance->shortcutsRail)
            currentInstance->shortcutsRail->refresh();

        brls::View* f = brls::Application::getCurrentFocus();
        bool underHome = false;
        for (brls::View* v = f; v; v = v->getParent())
            if (v == currentInstance) { underHome = true; break; }
        brls::Logger::info("refreshRailsIfActive: focus={} isActive={} underHome={} stack={}",
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
    cloud::Service::instance().refreshActiveProfile(false);
    if (!currentInstance)
        return;
    currentInstance->applyProfileMode();
    currentInstance->syncHostList();
    if (currentInstance->profileChip)
        currentInstance->profileChip->refresh();
    if (currentInstance->shortcutsRail)
        currentInstance->shortcutsRail->refresh();
}

void HostListTab::notifyAccountNameDisplayChanged() {
    if (!currentInstance || !currentInstance->profileChip)
        return;
    currentInstance->profileChip->refresh();
}

void HostListTab::syncHostList() {
    brls::Logger::debug("Syncing host list...");

    if (!hostContainer) {
        brls::Logger::error("syncHostList: hostContainer is null");
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
    const bool noProfile = settings->getProfiles().empty();
    if (noProfile)
        entrancePlayed = false;

    bool animateEntrance = !entrancePlayed;
    int shownIndex = 0;

    const int perRow = 6;
    int col = 0;
    brls::Box* row = nullptr;
    auto* hostsMap = settings->getHostsMap();
    if (hostsMap && !noProfile) {
        for (auto& [name, host] : *hostsMap) {
            if (!host) {
                brls::Logger::error("Null host in hosts map for key: {}", name);
                continue;
            }
            if (!host->hasRpKey() && !host->isDiscovered() && !host->isManual())
                continue;
            /*
             * A profile converted from a normal one keeps its "(Remote)" entries in the
             * toml. They are inert without a token, but they would still list and then
             * dead-end on click.
             */
            if (host->isRemote() && settings->isLegacyProfileActive())
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

    if (settings->getDevFakeHosts()) {
        static std::vector<Host*> fakeHosts;
        if (fakeHosts.empty()) {
            for (int i = 1; i <= 20; i++) {
                Host* h = new Host("PS5-Debug-" + std::to_string(i));
                h->setChiakiTarget(CHIAKI_TARGET_PS5_1);
                h->setState((i % 4 == 0) ? CHIAKI_DISCOVERY_HOST_STATE_STANDBY : CHIAKI_DISCOVERY_HOST_STATE_READY);
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
            for (auto* child : row->getChildren())
                child->setCustomNavigationRoute(brls::FocusDirection::DOWN, "cloud/rail/lead");
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

    brls::Logger::debug("Host list sync complete, {} hosts", hostItems.size());
}

void HostListTab::updateHostItem(Host* host) {
    if (!host) return;

    auto it = hostItems.find(host);
    if (it != hostItems.end()) {
        it->second->updateState();
        brls::Logger::debug("Updated host item: {}", host->getHostName());
    } else {
        brls::Logger::debug("Host not found in items, syncing: {}", host->getHostName());
        syncHostList();
    }
}
