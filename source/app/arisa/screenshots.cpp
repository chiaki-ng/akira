#include "app/arisa/screenshots.hpp"

#include <arisa/provider/gpu/image.hpp>

#include <chrono>
#include <cstddef>
#include <ctime>
#include <expected>
#include <filesystem>
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

Screenshots::Screenshots(provider::GpuProvider& gpu, Notifier& notifier, const AppSettings& settings,
    const app::Strings& strings, UiDispatcher& dispatcher)
    : gpu(gpu)
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
    if (shared->writing.load())
    {
        notifier.notify(strings.text("akira/screenshot/busy"));
        return;
    }
    std::expected<provider::Image, provider::ProviderError> captured = gpu.capture();
    if (!captured)
    {
        log::warning("[SCREENSHOT] capture refused: {}", captured.error().explanation());
        notifier.notify(strings.format("akira/screenshot/failed", captured.error().explanation()));
        return;
    }
    const std::string name = stamp();
    auto image = std::make_shared<provider::Image>(std::move(*captured));
    shared->writing.store(true);
    log::info("[SCREENSHOT] captured {}x{}, writing {}", image->width(), image->height(), name);
    dispatcher.runOnWorker([image, name, state = shared, &notifier = notifier, &strings = strings, &dispatcher = dispatcher] {
        const std::span<const std::byte> pixels = image->pixels();
        std::optional<Screenshot> shot =
            Screenshot::create(image->width(), image->height(), std::vector<std::byte>(pixels.begin(), pixels.end()));
        std::optional<std::string> failure;
        if (!shot)
            failure = "bad frame";
        else if (const auto saved = shot->save(std::filesystem::path(folder), name); !saved)
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
