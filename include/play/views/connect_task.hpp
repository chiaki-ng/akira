#ifndef AKIRA_CONNECT_TASK_HPP
#define AKIRA_CONNECT_TASK_HPP

#include <memory>
#include <string>

#include "play/views/connection_stage.hpp"

namespace akira::consoles {
class Host;
}

namespace akira::play::views {

/*
 * What a route reports while it is connecting.
 *
 * Implemented by the connecting view, passed to the task. Everything here may
 * be called from a worker thread; the view is responsible for getting it onto
 * the UI thread, so a task never has to think about brls::sync.
 */
class ConnectSink {
public:
    virtual ~ConnectSink() = default;

    virtual void progress(ConnectionStage stage) = 0;

    virtual void progressStep(int index, int total, const std::string& label) = 0;

    /*
     * Both forms of the host, because the two routes own it differently: a
     * console akira already knows outlives the view, and a cloud session is
     * provisioned for this attempt and has to be kept alive by whoever streams
     * it. StreamView takes exactly this pair.
     */
    virtual void succeeded(consoles::Host* host, std::shared_ptr<consoles::Host> owner) = 0;

    virtual void failed(const std::string& error) = 0;

    /* Cancelled from the screen. Long waits should check this and give up. */
    virtual bool cancelled() const = 0;
};

/*
 * The part of connecting that differs by route.
 *
 * Everything a person sees - the log tail, the stage ring, cancel, the handover
 * to the stream - belongs to the connecting view. A task only knows how to
 * start work, how to stop it, and what to call itself.
 */
class ConnectTask {
public:
    virtual ~ConnectTask() = default;

    /* Shown beside the spinner. */
    virtual std::string title() const = 0;

    /* Names this attempt's log file: "remote", "local", "cloud". */
    virtual const char* logKind() const = 0;

    virtual void start(ConnectSink& sink) = 0;

    virtual void cancel() {}

    /*
     * A route's own answer to a failure, for when there is something to offer
     * beyond acknowledging it - cloud can retry past a privacy check, and only
     * cloud knows that. Return true to say the failure has been presented and
     * the view should not raise its own dialog.
     */
    virtual bool presentFailure(const std::string& error, ConnectSink& sink)
    {
        (void)error;
        (void)sink;
        return false;
    }
};

} // namespace akira::play::views

#endif // AKIRA_CONNECT_TASK_HPP
