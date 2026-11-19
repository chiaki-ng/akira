#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

#include <libchiaking/play_types.hpp>

#include "play/connection_stage.hpp"

namespace akira::play {

class ConnectFlow {
public:
    static constexpr int stageCount = 6;

    struct Ring {
        std::string label;
        const char* labelKey = nullptr;
        int index = 0;
        int total = 0;
    };

    static ConnectionStage stageOf(libchiaking::play::ConnectStep step);
    static const char* labelKey(ConnectionStage stage);

    void advance(ConnectionStage stage);
    void follow(libchiaking::play::ConnectStep step, bool reconnecting);
    void restart();
    void connected();
    ConnectionStage stage() const;

    void step(int index, int total, const std::string& label);
    Ring ring() const;

    [[nodiscard]] bool settle();
    bool settled() const { return done.load(); }
    [[nodiscard]] bool cancel();
    bool cancelled() const { return wasCancelled.load(); }
    [[nodiscard]] std::optional<const char*> failOnRing();
    [[nodiscard]] bool failOnDialog();
    bool showingFailure() const { return failureShown.load(); }
    [[nodiscard]] bool dismiss();

private:
    std::atomic<int> current{0};
    std::atomic<int> stepIndex{0};
    std::atomic<int> stepTotal{0};
    std::atomic<bool> stepped{false};
    std::string stepLabel;
    mutable std::mutex stepMutex;
    std::atomic<bool> done{false};
    std::atomic<bool> wasCancelled{false};
    std::atomic<bool> failureShown{false};
    std::atomic<bool> dismissed{false};
};

}
