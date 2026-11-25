#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

#include <arisa/provider/lifecycle/provider.hpp>
#include <arisa/provider/surface/provider.hpp>

#include "app/ui_window.hpp"

namespace akira::app::arisa {

class Window final : public UiWindow {
public:
    Window(::arisa::provider::SurfaceProvider& surface, ::arisa::provider::LifecycleProvider& lifecycle);

    auto width() const -> unsigned override;
    auto height() const -> unsigned override;
    void suspendRendering() override;
    auto watchFocus(std::function<void(bool focused)> changed) -> std::unique_ptr<FocusWatch> override;

    void tick();
    [[nodiscard]] auto takeVideoStart() -> bool;

private:
    class Watchers {
    public:
        auto add(std::function<void(bool focused)> changed) -> std::uint64_t;
        void remove(std::uint64_t id);
        void notify(bool focused);

    private:
        std::mutex mutex;
        std::uint64_t next = 1;
        std::map<std::uint64_t, std::shared_ptr<std::function<void(bool focused)>>> watchers;
    };

    class Watch final : public FocusWatch {
    public:
        Watch(std::shared_ptr<Watchers> watchers, std::uint64_t id);
        ~Watch() override;

        Watch(const Watch&) = delete;
        Watch& operator=(const Watch&) = delete;

    private:
        std::shared_ptr<Watchers> watchers;
        std::uint64_t id;
    };

    ::arisa::provider::SurfaceProvider& surface;
    ::arisa::provider::LifecycleProvider& lifecycle;
    std::shared_ptr<Watchers> watchers = std::make_shared<Watchers>();
    ::arisa::provider::ScenePhase phase;
    std::atomic<bool> videoStarted{false};
};

}
