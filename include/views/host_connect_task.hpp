#ifndef AKIRA_HOST_CONNECT_TASK_HPP
#define AKIRA_HOST_CONNECT_TASK_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <libchiaking/platform/platform.hpp>

#include "views/connect_task.hpp"

class Host;

namespace akira::views {

/*
 * A console akira already knows about, on the same network or over the
 * internet.
 *
 * Both are one task because the difference between them is one branch: a local
 * console is ready the moment it is asked, a remote one needs a PSN token, a
 * holepunch, and time to wake up. Splitting them would duplicate the parts that
 * are the same and hide how little actually differs.
 */
class HostConnectTask : public ConnectTask {
public:
    explicit HostConnectTask(Host* host);
    ~HostConnectTask() override;

    std::string title() const override;
    const char* logKind() const override;

    void start(ConnectSink& sink) override;
    void cancel() override;

private:
    Host* host = nullptr;
    ConnectSink* sink = nullptr;

    std::atomic<bool> running{false};
    std::shared_ptr<const bool> alive = std::make_shared<const bool>(true);
    const std::weak_ptr<const bool> aliveToken = alive;
    libchiaking::platform::Thread worker;

    void run(const libchiaking::platform::StopToken& stop);
    void post(std::function<void(ConnectSink&)> work);
};

} // namespace akira::views

#endif // AKIRA_HOST_CONNECT_TASK_HPP
