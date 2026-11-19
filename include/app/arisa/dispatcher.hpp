#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <arisa/async/executor.hpp>
#include <arisa/async/frame_scheduler.hpp>
#include <arisa/async/job.hpp>
#include <arisa/runtime/dispatcher.hpp>

#include "app/ui_dispatcher.hpp"

namespace akira::app::arisa {

class Dispatcher final : public UiDispatcher {
public:
    Dispatcher(::arisa::runtime::Dispatcher root, ::arisa::async::FrameScheduler& timers,
        ::arisa::async::Executor& workers);
    ~Dispatcher() override;

    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    void post(std::function<void()> work) override;
    void postAfter(std::chrono::milliseconds delay, std::function<void()> work) override;
    void runOnWorker(std::function<void()> work) override;
    auto repeatEvery(std::chrono::milliseconds interval, std::function<void()> work)
        -> std::unique_ptr<Repeating> override;

private:
    class Jobs {
    public:
        void keep(::arisa::async::Job job);
        void clear();

    private:
        std::mutex mutex;
        std::vector<::arisa::async::Job> jobs;
    };

    class Ticker final : public Repeating {
    public:
        Ticker(Dispatcher& dispatcher, std::chrono::milliseconds interval, std::function<void()> work);
        ~Ticker() override;

        Ticker(const Ticker&) = delete;
        Ticker& operator=(const Ticker&) = delete;

    private:
        struct State {
            std::chrono::milliseconds interval;
            std::function<void()> work;
            std::atomic<bool> alive{true};
        };

        static void schedule(Dispatcher& dispatcher, std::shared_ptr<State> state);

        std::shared_ptr<State> state;
    };

    ::arisa::runtime::Dispatcher root;
    ::arisa::async::FrameScheduler& timers;
    ::arisa::async::Executor& workers;
    Jobs timed;
    Jobs background;
};

}
