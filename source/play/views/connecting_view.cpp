#include "play/views/connecting_view.hpp"
#include "log/log_files.hpp"
#include "play/views/connection_stage.hpp"
#include "play/views/stream_nav.hpp"
#include "log/log.hpp"

#include <borealis/core/i18n.hpp>

#include "consoles/host.hpp"
#include "app/view_services.hpp"
#include "app/views/theme.hpp"
#include "log/app_log.hpp"
#include "app/views/shared_view_holder.hpp"
#include "play/views/stream_view.hpp"

namespace akira::play::views {

using namespace brls::literals;

}

namespace akira::play::views {

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
                       if (flow.showingFailure())
                           dismissAfterFailure();
                       else
                           cancelFromUser();
                       return true;
                   }, false);
}

ConnectingView::~ConnectingView()
{
    logSubscription.reset();

    if (!flow.settled())
        task->cancel();

    task.reset();

    connectionLog.reset();
    app::views::SharedViewHolder::release(this);
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

    auto* services = &app::ViewServices::current();
    if (services->appSettings.hideAccountName())
    {
        for (const accounts::Profile& profile : services->accounts.profiles())
        {
            logTail.mask(profile.onlineId, services->appSettings.maskAccountName(profile.onlineId));
            logTail.mask(profile.accountId, services->appSettings.maskAccountName(profile.accountId));
        }
    }

    logSubscription = log::Log::process().subscribe(
        [weak](const log::Entry& entry) {
            if (auto self = weak.lock())
                self->addLogLine(std::string(entry.message));
        });

    switchToConnectionLog();

    log::info("========================================");
    log::info("CONNECTION ATTEMPT: {} to {}", task->logKind(), task->title());
    log::info("========================================");

    addLogLine("akira/connection/starting"_i18n);

    task->start(*this);
}

void ConnectingView::cancelFromUser()
{
    if (!flow.cancel())
        return;

    log::info("Connection cancelled by user");
    task->cancel();

    brls::sync([]() { stream_nav::unwindToBase(); });
}

void ConnectingView::addLogLine(const std::string& line)
{
    logTail.push(line);
}

void ConnectingView::switchToConnectionLog()
{
    if (!app::ViewServices::current().developer.fileLogging())
        return;

    const std::string logPath = log::LogFiles::connectionLogPath(task->logKind());
    connectionLog = log::ConnectionLog::open(logPath);
    if (connectionLog)
        log::info("Switched to connection log: {}", logPath);
}

void ConnectingView::progress(ConnectionStage stage)
{
    flow.advance(stage);
}

void ConnectingView::progressStep(int index, int total, const std::string& label)
{
    flow.step(index, total, label);
}

bool ConnectingView::cancelled() const
{
    return flow.cancelled();
}

void ConnectingView::succeeded(consoles::Host* host, std::shared_ptr<consoles::Host> owner)
{
    if (!flow.settle())
        return;

    log::info("Transitioning to stream view...");

    auto streamView = app::views::SharedViewHolder::holdNew<StreamView>(host, owner);
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
    if (flow.settled())
        return;

    log::error("{}", error);

    if (task->presentFailure(error, *this))
        return;

    /*
     * Named by stage when the ring is what is on screen, because "it failed
     * while linking" is worth more than the last error string the stack
     * produced - and the ring is already showing which stage that was.
     */
    if (app::ViewServices::current().appSettings.connectionShowStages()) {
        failureText = brls::getStr(flow.failOnRing());

        auto weak = weak_from_this();
        brls::delay(kFailureLingerMs, [weak]() {
            if (auto self = weak.lock())
                self->dismissAfterFailure();
        });
        return;
    }

    flow.failOnDialog();

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
    if (!flow.dismiss())
        return;

    log::info("Connection failure acknowledged, returning to hosts");
    stream_nav::unwindToBase();
}

void ConnectingView::renderLogs(NVGcontext* vg, float x, float y, float width, float height)
{
    const std::vector<std::string> logLines = logTail.lines();

    nvgSave(vg);
    nvgScissor(vg, x, y, width, height);

    nvgFontSize(vg, 16);
    nvgFillColor(vg, app::views::active().textMuted);
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

    if (flow.showingFailure()) {
        drawConnectionFailure(vg, centerX, centerY, failureText);
        return;
    }

    if (app::ViewServices::current().appSettings.connectionShowStages()) {
        const ConnectFlow::Ring ring = flow.ring();
        drawConnectionRing(vg, centerX, centerY, ring.label.empty() ? brls::getStr(ring.labelKey) : ring.label,
                           ring.index, ring.total);
        return;
    }

    renderLogs(vg, logArea->getX(), logArea->getY(),
               logArea->getWidth(), logArea->getHeight());
}

void ConnectingView::open(std::unique_ptr<ConnectTask> task)
{
    auto view = app::views::SharedViewHolder::holdNew<ConnectingView>(std::move(task));
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
        auto* services = &app::ViewServices::current();
        services->developer.setFileLogging(false);
        services->store.save();
        log::AppLog::close();
        open(std::move(*pending));
    });
    dialog->addButton("akira/connection/file_logging_keep_on"_i18n, [pending]() {
        open(std::move(*pending));
    });
    dialog->open();
}

void startConnecting(std::unique_ptr<ConnectTask> task)
{
    if (app::ViewServices::current().developer.fileLogging())
        ConnectingView::askAboutFileLogging(std::move(task));
    else
        ConnectingView::open(std::move(task));
}

} // namespace akira::play::views
