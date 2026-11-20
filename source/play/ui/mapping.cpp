#include "play/ui/mapping.hpp"

#include <utility>
#include <vector>

#include "play/button_capture.hpp"
#include "settings/settings_store.hpp"
#include "settings/ui/control_options.hpp"

namespace akira::play::ui {

Mapping::Mapping(ControllerSettings& controllers, settings::SettingsStore& store, settings::ui::ControlOptions& options,
    ButtonCapture& capture)
    : buttons(controllers)
    , store(store)
    , touch(options)
    , source(capture)
{
}

Mapping::~Mapping()
{
    if (state.get().capturing)
        source.end();
}

void Mapping::capture(std::uint32_t key)
{
    if (state.get().capturing)
        return;
    combo = ComboCapture();
    source.begin();
    state.update([key](Status& status) {
        status.capturing = key;
        status.peak = 0;
    });
}

void Mapping::poll()
{
    const std::optional<std::uint32_t> target = state.get().capturing;
    if (!target)
        return;
    std::optional<std::vector<std::uint64_t>> finished = combo.feed(source.held());
    if (!finished)
    {
        std::uint64_t peak = 0;
        for (const std::uint64_t button : combo.peak())
            peak |= button;
        if (peak != state.get().peak)
            state.write(&Status::peak, peak);
        return;
    }
    source.end();
    buttons.assign(*target, std::move(*finished));
    state.update([](Status& status) {
        status.capturing.reset();
        status.peak = 0;
    });
    saved();
}

void Mapping::cancel()
{
    if (!state.get().capturing)
        return;
    source.end();
    state.update([](Status& status) {
        status.capturing.reset();
        status.peak = 0;
    });
}

void Mapping::reset(std::uint32_t key)
{
    buttons.reset(key);
    saved();
}

void Mapping::resetAll()
{
    buttons.resetAll();
    touch.enableAll();
    saved();
}

void Mapping::saved()
{
    store.save();
    state.update([](Status& status) { ++status.revision; });
}

}
