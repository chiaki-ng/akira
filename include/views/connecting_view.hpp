#ifndef AKIRA_CONNECTING_VIEW_HPP
#define AKIRA_CONNECTING_VIEW_HPP

#include "util/connection_log.hpp"

#include <optional>

#include <borealis.hpp>

#include <atomic>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "views/connect_task.hpp"

class Host;

namespace akira::views {

/*
 * The one screen shown while a stream is being brought up.
 *
 * There used to be two of these - one for a console over the internet, one for
 * cloud - grown from the same XML and drifted apart, and a third route that had
 * none at all and pushed the stream view straight onto whatever was focused.
 * That last one is why the three behaved differently around the controller
 * picker: nothing owned the activity stack in the same way twice.
 *
 * What varies by route is behind ConnectTask. What a person sees is here.
 */
class ConnectingView : public brls::Box,
                       public ConnectSink,
                       public std::enable_shared_from_this<ConnectingView> {
public:
    explicit ConnectingView(std::unique_ptr<ConnectTask> task);
    ~ConnectingView() override;

    /* Called after make_shared, so weak_from_this() is usable by the task. */
    void setupAndStart();
    static void open(std::unique_ptr<ConnectTask> task);
    static void askAboutFileLogging(std::unique_ptr<ConnectTask> task);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    /*
     * This view, and nothing inside it. The cancel button is reachable by
     * pressing B rather than by focusing it, so there is one focus owner for
     * the whole screen and no way to be left focused on a control that a
     * moment later belongs to a different activity.
     */
    brls::View* getDefaultFocus() override { return this; }

    /* ConnectSink */
    void progress(ConnectionStage stage) override;
    void progressStep(int index, int total, const std::string& label) override;
    void succeeded(Host* host, std::shared_ptr<Host> owner) override;
    void failed(const std::string& error) override;
    bool cancelled() const override;

private:
    std::unique_ptr<ConnectTask> task;

    std::deque<std::string> logLines;
    mutable std::mutex logMutex;
    std::vector<std::pair<std::string, std::string>> logMasks;
    static constexpr std::size_t MAX_LOG_LINES = 100;

    std::atomic<int> currentStage{0};
    std::atomic<int> stepIndex{0};
    std::atomic<int> stepTotal{0};
    std::atomic<bool> steppedRoute{false};
    std::string stepLabel;
    mutable std::mutex stepMutex;
    std::atomic<bool> settled{false};
    std::atomic<bool> wasCancelled{false};
    std::atomic<bool> showFailure{false};
    std::atomic<bool> dismissed{false};
    static constexpr int kFailureLingerMs = 4000;
    std::string failureText;

    brls::Event<brls::Logger::TimePoint, brls::LogLevel, std::string>::Subscription logSubscription;

    std::optional<akira::ConnectionLog> connectionLog;

    void addLogLine(const std::string& line);
    void renderLogs(NVGcontext* vg, float x, float y, float width, float height);
    void switchToConnectionLog();
    void cancelFromUser();
    void dismissAfterFailure();
};

/*
 * The single way in. Builds the view, pushes it, and starts the task - so no
 * caller has to remember the order, and no route can push a stream view
 * directly onto whatever happened to be underneath.
 */
void startConnecting(std::unique_ptr<ConnectTask> task);

} // namespace akira::views

#endif // AKIRA_CONNECTING_VIEW_HPP
