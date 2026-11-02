#include "views/connecting_view.hpp"
#include "views/stream_nav.hpp"

#include <chrono>
#include <ctime>
#include <format>

#include <borealis/core/i18n.hpp>

#include "core/host.hpp"
#include "core/settings_manager.hpp"
#include "ui/theme.hpp"
#include "util/app_log.hpp"
#include "util/shared_view_holder.hpp"
#include "views/stream_view.hpp"

using namespace brls::literals;

namespace akira::views {

namespace {

void replaceAll(std::string& text, const std::string& needle, const std::string& replacement)
{
    if (needle.empty() || needle == replacement)
        return;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos)
    {
        text.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

} // namespace

ConnectingView::ConnectingView(std::unique_ptr<ConnectTask> task)
    : task(std::move(task))
{
    this->inflateFromXMLRes("xml/views/connection_view.xml");

    auto* titleLabel = (brls::Label*)this->getView("connection/title");
    titleLabel->setText(this->task->title());
    titleLabel->setMarginLeft(14);

    auto* titleRow = (brls::Box*)this->getView("connection/titleRow");
    auto* spinner = new brls::ProgressSpinner();
    spinner->setWidth(28);
    spinner->setHeight(28);
    titleRow->addView(spinner, 0);

    setFocusable(true);

    /*
     * Swallowed, not left to fall through.
     *
     * L and R open the trophy and settings tabs from the home screen, and this
     * screen sits above it with a stream half-built underneath. Registering
     * them here means they stop at the top of the stack whatever else is on it.
     */
    registerAction("", brls::ControllerButton::BUTTON_LB, [](brls::View*) { return true; }, false);
    registerAction("", brls::ControllerButton::BUTTON_RB, [](brls::View*) { return true; }, false);

    registerAction("akira/common/cancel"_i18n, brls::ControllerButton::BUTTON_B,
                   [this](brls::View*) {
                       if (showFailure.load())
                           dismissAfterFailure();
                       else
                           cancelFromUser();
                       return true;
                   }, false);
}

ConnectingView::~ConnectingView()
{
    brls::Logger::unsubscribeFromLog(logSubscription);

    if (!settled.load())
        task->cancel();

    task.reset();

    connectionLog.reset();
    SharedViewHolder::release(this);
}

void ConnectingView::setupAndStart()
{
    auto weak = weak_from_this();

    auto* cancelBtn = (brls::Button*)this->getView("connection/cancel");
    if (cancelBtn) {
        cancelBtn->registerClickAction([weak](brls::View*) {
            if (auto self = weak.lock())
                self->cancelFromUser();
            return true;
        });
    }

    SettingsManager* settings = SettingsManager::getInstance();
    if (settings->getHideAccountName())
    {
        for (const Profile& profile : settings->getProfiles())
        {
            logMasks.emplace_back(profile.onlineId, settings->maskAccountName(profile.onlineId));
            logMasks.emplace_back(profile.accountId, settings->maskAccountName(profile.accountId));
        }
    }

    logSubscription = brls::Logger::subscribeToLog(
        [weak](brls::Logger::TimePoint, brls::LogLevel, std::string msg) {
            if (auto self = weak.lock())
                self->addLogLine(msg);
        });

    switchToConnectionLog();

    brls::Logger::info("========================================");
    brls::Logger::info("CONNECTION ATTEMPT: {} to {}", task->logKind(), task->title());
    brls::Logger::info("========================================");

    addLogLine("akira/connection/starting"_i18n);

    task->start(*this);
}

void ConnectingView::cancelFromUser()
{
    if (settled.exchange(true))
        return;

    brls::Logger::info("Connection cancelled by user");
    wasCancelled.store(true);
    task->cancel();

    brls::sync([]() { stream_nav::unwindToBase(); });
}

void ConnectingView::addLogLine(const std::string& line)
{
    std::string safeLine = line;
    for (const auto& [needle, mask] : logMasks)
        replaceAll(safeLine, needle, mask);
    {
        std::lock_guard<std::mutex> lock(logMutex);

        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()).count() % 1000;
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm time_tm = *std::localtime(&tt);

        logLines.push_back(std::format("{:02}:{:02}:{:02}.{:03} {}",
                                       time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec,
                                       static_cast<int>(ms), safeLine));

        while (logLines.size() > MAX_LOG_LINES)
            logLines.pop_front();
    }
}

void ConnectingView::switchToConnectionLog()
{
    if (!SettingsManager::getInstance()->getEnableFileLogging())
        return;

    const std::string logPath = SettingsManager::getConnectionLogFilePath(task->logKind());
    connectionLog = akira::ConnectionLog::open(logPath);
    if (connectionLog)
        brls::Logger::info("Switched to connection log: {}", logPath);
}

void ConnectingView::progress(ConnectionStage stage)
{
    int reported = static_cast<int>(stage);
    int current = currentStage.load();
    while (reported > current && !currentStage.compare_exchange_weak(current, reported))
        ;
}

void ConnectingView::progressStep(int index, int total, const std::string& label)
{
    steppedRoute.store(true);

    if (index > 0 && total > 0) {
        stepIndex.store(index);
        stepTotal.store(total);
    }

    std::lock_guard<std::mutex> lock(stepMutex);
    stepLabel = label;
}

bool ConnectingView::cancelled() const
{
    return wasCancelled.load();
}

void ConnectingView::succeeded(Host* host, std::shared_ptr<Host> owner)
{
    if (settled.exchange(true))
        return;

    brls::Logger::info("Transitioning to stream view...");

    auto streamView = SharedViewHolder::holdNew<StreamView>(host, owner);
    streamView->setupCallbacks();
    if (connectionLog)
        streamView->adoptConnectionLog(std::move(*connectionLog));
    connectionLog.reset();

    /*
     * Popped and pushed in the same callback, and this is the moment the home
     * screen used to take focus back: popping makes it briefly the top activity,
     * its onResume fires, and the callback it queues runs a frame later - by
     * which time the stream view, and often the controller picker, are above it.
     */
    brls::sync([streamView]() {
        brls::Application::popActivity();
        brls::Application::pushActivity(new brls::Activity(streamView.get()));
        streamView->startStream();
    });
}

void ConnectingView::failed(const std::string& error)
{
    if (settled.load())
        return;

    brls::Logger::error("{}", error);

    if (task->presentFailure(error, *this))
        return;

    /*
     * Named by stage when the ring is what is on screen, because "it failed
     * while linking" is worth more than the last error string the stack
     * produced - and the ring is already showing which stage that was.
     */
    if (SettingsManager::getInstance()->getConnectionShowStages()) {
        const auto stage = static_cast<ConnectionStage>(currentStage.load());
        failureText = brls::getStr(connectionFailureKeyForStage(stage));
        settled.store(true);
        showFailure.store(true);

        auto weak = weak_from_this();
        brls::delay(kFailureLingerMs, [weak]() {
            if (auto self = weak.lock())
                self->dismissAfterFailure();
        });
        return;
    }

    settled.store(true);

    brls::sync([error]() {
        auto* dialog = new brls::Dialog(brls::getStr("akira/connection/connection_failed", error));
        dialog->addButton("akira/common/ok"_i18n, []() {
            stream_nav::unwindToBase();
        });
        dialog->open();
    });
}

void ConnectingView::dismissAfterFailure()
{
    if (dismissed.exchange(true))
        return;

    brls::Logger::info("Connection failure acknowledged, returning to hosts");
    stream_nav::unwindToBase();
}

void ConnectingView::renderLogs(NVGcontext* vg, float x, float y, float width, float height)
{
    std::lock_guard<std::mutex> lock(logMutex);

    nvgSave(vg);
    nvgScissor(vg, x, y, width, height);

    nvgFontSize(vg, 16);
    nvgFillColor(vg, akira::ui::active().textMuted);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    const float padding = 20;
    const float maxTextWidth = width - padding * 2;
    const float availableHeight = height - padding * 2;

    /* Fitted from the bottom up: the newest line is the one worth keeping when
     * there is not room for all of them. */
    std::size_t startIdx = logLines.size();
    float totalHeight = 0;
    while (startIdx > 0) {
        float bounds[4];
        nvgTextBoxBounds(vg, 0, 0, maxTextWidth, logLines[startIdx - 1].c_str(), nullptr, bounds);
        const float lineH = bounds[3] - bounds[1];
        if (totalHeight + lineH > availableHeight)
            break;
        totalHeight += lineH;
        startIdx--;
    }

    float currentY = y + padding;
    for (std::size_t i = startIdx; i < logLines.size(); i++) {
        nvgTextBox(vg, x + padding, currentY, maxTextWidth, logLines[i].c_str(), nullptr);

        float bounds[4];
        nvgTextBoxBounds(vg, x + padding, currentY, maxTextWidth, logLines[i].c_str(), nullptr, bounds);
        currentY += (bounds[3] - bounds[1]);
    }

    nvgRestore(vg);
}

void ConnectingView::draw(NVGcontext* vg, float x, float y, float width, float height,
                          brls::Style style, brls::FrameContext* ctx)
{
    Box::draw(vg, x, y, width, height, style, ctx);

    auto* logArea = this->getView("connection/logArea");
    if (!logArea)
        return;

    const float centerX = logArea->getX() + logArea->getWidth() / 2.0f;
    const float centerY = logArea->getY() + logArea->getHeight() / 2.0f;

    if (showFailure.load()) {
        drawConnectionFailure(vg, centerX, centerY, failureText);
        return;
    }

    if (SettingsManager::getInstance()->getConnectionShowStages()) {
        if (steppedRoute.load()) {
            std::string label;
            {
                std::lock_guard<std::mutex> lock(stepMutex);
                label = stepLabel;
            }
            if (label.empty())
                label = brls::getStr("akira/connection/stage_connecting");
            drawConnectionRing(vg, centerX, centerY, label,
                               stepIndex.load(), stepTotal.load());
            return;
        }

        const auto stage = static_cast<ConnectionStage>(currentStage.load());
        const int raw = static_cast<int>(stage);
        const int idx = raw < 1 ? 1 : (raw > 6 ? 6 : raw);
        drawConnectionRing(vg, centerX, centerY,
                           brls::getStr(connectionStageLabelKey(stage)), idx, 6);
        return;
    }

    renderLogs(vg, logArea->getX(), logArea->getY(),
               logArea->getWidth(), logArea->getHeight());
}

void ConnectingView::open(std::unique_ptr<ConnectTask> task)
{
    auto view = SharedViewHolder::holdNew<ConnectingView>(std::move(task));
    stream_nav::markBase();
    brls::Application::pushActivity(new brls::Activity(view.get()));
    view->setupAndStart();
}

void ConnectingView::askAboutFileLogging(std::unique_ptr<ConnectTask> task)
{
    auto pending = std::make_shared<std::unique_ptr<ConnectTask>>(std::move(task));
    auto* dialog = new brls::Dialog("akira/connection/file_logging_warning"_i18n);
    dialog->setCancelable(false);
    dialog->addButton("akira/connection/file_logging_turn_off"_i18n, [pending]() {
        SettingsManager* settings = SettingsManager::getInstance();
        settings->setEnableFileLogging(false);
        settings->writeFile();
        AppLog::close();
        open(std::move(*pending));
    });
    dialog->addButton("akira/connection/file_logging_keep_on"_i18n, [pending]() {
        open(std::move(*pending));
    });
    dialog->open();
}

void startConnecting(std::unique_ptr<ConnectTask> task)
{
    if (SettingsManager::getInstance()->getEnableFileLogging())
        ConnectingView::askAboutFileLogging(std::move(task));
    else
        ConnectingView::open(std::move(task));
}

} // namespace akira::views
