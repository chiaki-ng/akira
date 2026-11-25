#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>

#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/frame_observer.hpp>

#include "app/app_settings.hpp"
#include "app/arisa/stream_capture.hpp"
#include "app/screenshot.hpp"
#include "app/notifier.hpp"
#include "app/strings.hpp"
#include "app/ui_dispatcher.hpp"
#include "play/screenshot_chord.hpp"

namespace akira::app::arisa {

class Screenshots final : public ::arisa::runtime::FrameObserver {
public:
    static constexpr std::string_view folder = "sdmc:/switch/akira/screenshots";

    Screenshots(::arisa::provider::GpuProvider& gpu, StreamCapture& stream, Notifier& notifier, const AppSettings& settings,
        const app::Strings& strings, UiDispatcher& dispatcher);
    ~Screenshots() override;

    void filter(::arisa::provider::InputFrame& input) override;
    void observe(const ::arisa::runtime::FrameTimings& frame) override;
    void tick();

private:
    struct Shared {
        std::atomic<bool> alive{ true };
        std::atomic<bool> writing{ false };
    };

    static auto held(const ::arisa::provider::InputFrame& input) -> std::uint64_t;
    static auto stamp() -> std::string;
    void take();
    void save(std::expected<Screenshot, std::string> shot);

    ::arisa::provider::GpuProvider& gpu;
    StreamCapture& stream;
    Notifier& notifier;
    const AppSettings& settings;
    const app::Strings& strings;
    UiDispatcher& dispatcher;
    play::ScreenshotChord chord;
    bool requested = false;
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
};

}
