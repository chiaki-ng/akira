#include "app/arisa/screenshots.hpp"

#include <arisa/provider/gpu/image.hpp>

#include <chrono>
#include <cstddef>
#include <ctime>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "app/screenshot.hpp"
#include "log/log.hpp"
#include "play/switch_button.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

Screenshots::Screenshots(provider::GpuProvider& gpu, StreamCapture& stream, Notifier& notifier,
    const AppSettings& settings, const app::Strings& strings, UiDispatcher& dispatcher)
    : gpu(gpu)
    , stream(stream)
    , notifier(notifier)
    , settings(settings)
    , strings(strings)
    , dispatcher(dispatcher)
{
}

Screenshots::~Screenshots()
{
    shared->alive.store(false);
}

auto Screenshots::held(const provider::InputFrame& input) -> std::uint64_t
{
    std::uint64_t mask = 0;
    for (const provider::PadState& pad : input.pads)
    {
        if (!pad.connected)
            continue;
        if (pad.buttons.contains(provider::Button::zl))
            mask |= play::bits(play::SwitchButton::ZL);
        if (pad.buttons.contains(provider::Button::zr))
            mask |= play::bits(play::SwitchButton::ZR);
        if (pad.buttons.contains(provider::Button::right_stick))
            mask |= play::bits(play::SwitchButton::StickR);
    }
    return mask;
}

void Screenshots::filter(provider::InputFrame& input)
{
    if (chord.update(settings.screenshotChord() ? held(input) : 0))
        requested = true;
    if (!chord.swallowing())
        return;
    for (provider::PadState& pad : input.pads)
        pad.buttons = pad.buttons.without(provider::Button::right_stick);
}

void Screenshots::observe(const ::arisa::runtime::FrameTimings&)
{
}

void Screenshots::tick()
{
    if (!requested)
        return;
    requested = false;
    take();
}

auto Screenshots::stamp() -> std::string
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_r(&now, &local);
    return Screenshot::fileName(local);
}

void Screenshots::take()
{
    if (shared->writing.exchange(true))
    {
        notifier.notify(strings.text("akira/screenshot/busy"));
        return;
    }
    StreamCapture::Delivery delivery = [state = shared, &dispatcher = dispatcher, this](
                                           std::expected<Screenshot, std::string> shot) mutable {
        dispatcher.post([state, this, shot = std::move(shot)]() mutable {
            if (state->alive.load())
                save(std::move(shot));
        });
    };
    if (stream.captureStream(delivery))
    {
        log::info("[SCREENSHOT] reading the stream frame back from akira's own swapchain image");
        return;
    }
    std::expected<provider::Image, provider::ProviderError> captured = gpu.capture();
    if (!captured)
    {
        save(std::unexpected(std::format("capture refused: {}", captured.error().explanation())));
        return;
    }
    const std::span<const std::byte> pixels = captured->pixels();
    std::optional<Screenshot> shot =
        Screenshot::create(captured->width(), captured->height(), std::vector<std::byte>(pixels.begin(), pixels.end()));
    if (!shot)
    {
        save(std::unexpected(std::string("bad frame")));
        return;
    }
    save(std::move(*shot));
}

void Screenshots::save(std::expected<Screenshot, std::string> shot)
{
    if (!shot)
    {
        shared->writing.store(false);
        log::warning("[SCREENSHOT] {}", shot.error());
        notifier.notify(strings.format("akira/screenshot/failed", shot.error()));
        return;
    }
    const std::string name = stamp();
    log::info("[SCREENSHOT] captured {}x{}, writing {}", shot->width(), shot->height(), name);
    dispatcher.runOnWorker([shot = std::make_shared<Screenshot>(std::move(*shot)), name, state = shared,
                               &notifier = notifier, &strings = strings, &dispatcher = dispatcher] {
        std::optional<std::string> failure;
        if (const auto saved = shot->save(std::filesystem::path(folder), name); !saved)
            failure = saved.error().message();
        if (failure)
            log::error("[SCREENSHOT] writing {} failed: {}", name, *failure);
        state->writing.store(false);
        if (!state->alive.load())
            return;
        dispatcher.post([state, &notifier, &strings, name, failure = std::move(failure)] {
            if (!state->alive.load())
                return;
            notifier.notify(failure ? strings.format("akira/screenshot/failed", *failure)
                                    : strings.format("akira/screenshot/saved", name));
        });
    });
}

}
