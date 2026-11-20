#include "settings/ui/controls_section.hpp"

#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/section.hpp>

#include <utility>
#include <vector>

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;

ControlsSection::ControlsSection(ControlOptions& options, const HelpCatalog& help, const app::Strings& strings)
    : options(&options)
    , help(&help)
    , strings(&strings)
{
}

auto ControlsSection::body(composition::Context& context) const -> composition::Element
{
    const ControlOptions::Values& values = context.read(options->view());
    return structure::Form(touchpad(values), rumble());
}

auto ControlsSection::swipe(ControlOptions::Swipe direction, const char* key, const char* id) const
    -> composition::Element
{
    ControlOptions* edit = options;
    return controls::Toggle(strings->text(key),
        composition::Binding<bool>(
            [edit, direction] {
                const ControlOptions::Values& values = edit->view().get();
                switch (direction)
                {
                    case ControlOptions::Swipe::Up: return values.swipeUp;
                    case ControlOptions::Swipe::Down: return values.swipeDown;
                    case ControlOptions::Swipe::Left: return values.swipeLeft;
                    case ControlOptions::Swipe::Right: return values.swipeRight;
                }
                return false;
            },
            [edit, direction](bool value) { edit->setSwipe(direction, value); }))
        .help(help->body("settings/swipeGestures"))
        .id(composition::ElementKey{ id });
}

auto ControlsSection::touchpad(const ControlOptions::Values& values) const -> composition::Element
{
    ControlOptions* edit = options;
    std::vector<composition::Element> rows;
    rows.push_back(controls::Toggle(strings->text("akira/settings/touchpad"),
        composition::Binding<bool>([edit] { return edit->view().get().touchpad; },
            [edit](bool value) { edit->setTouchpad(value); }))
                       .help(help->body("settings/touchpad"))
                       .id(composition::ElementKey{ "controls.touchpad" }));
    if (values.touchpad)
    {
        rows.push_back(swipe(ControlOptions::Swipe::Up, "akira/settings/swipe_up", "controls.swipe-up"));
        rows.push_back(swipe(ControlOptions::Swipe::Down, "akira/settings/swipe_down", "controls.swipe-down"));
        rows.push_back(swipe(ControlOptions::Swipe::Left, "akira/settings/swipe_left", "controls.swipe-left"));
        rows.push_back(swipe(ControlOptions::Swipe::Right, "akira/settings/swipe_right", "controls.swipe-right"));
    }
    return structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/settings/touchpad_heading")) },
        std::move(rows));
}

auto ControlsSection::rumble() const -> composition::Element
{
    ControlOptions* edit = options;
    const std::vector<std::pair<play::RumbleSource, std::string>> sources{
        { play::RumbleSource::Off, strings->text("akira/settings/rumble_source_off") },
        { play::RumbleSource::Derived, strings->text("akira/settings/rumble_source_derived") },
        { play::RumbleSource::Game, strings->text("akira/settings/rumble_source_game") },
    };
    const std::vector<std::pair<play::HapticIntensity, std::string>> intensities{
        { play::HapticIntensity::Off, strings->text("akira/settings/haptic_intensity_off") },
        { play::HapticIntensity::VeryWeak, strings->text("akira/settings/haptic_intensity_very_weak") },
        { play::HapticIntensity::Weak, strings->text("akira/settings/haptic_intensity_weak") },
        { play::HapticIntensity::Normal, strings->text("akira/settings/haptic_intensity_normal") },
        { play::HapticIntensity::Strong, strings->text("akira/settings/haptic_intensity_strong") },
        { play::HapticIntensity::VeryStrong, strings->text("akira/settings/haptic_intensity_very_strong") },
    };
    std::vector<composition::Element> rows;
    rows.push_back(controls::Picker(strings->text("akira/settings/rumble_source"),
        composition::Binding<play::RumbleSource>([edit] { return edit->view().get().rumble; },
            [edit](play::RumbleSource value) { edit->setRumble(value); }),
        [sources] {
            std::vector<composition::Element> items;
            for (const auto& [source, label] : sources)
                items.push_back(components::Text(label).tag(source));
            return composition::Element(components::Group(std::move(items)));
        })
                       .help(help->body("settings/rumbleSource"))
                       .id(composition::ElementKey{ "controls.rumble" }));
    rows.push_back(controls::Picker(strings->text("akira/settings/haptic_intensity"),
        composition::Binding<play::HapticIntensity>([edit] { return edit->view().get().intensity; },
            [edit](play::HapticIntensity value) { edit->setIntensity(value); }),
        [intensities] {
            std::vector<composition::Element> items;
            for (const auto& [intensity, label] : intensities)
                items.push_back(components::Text(label).tag(intensity));
            return composition::Element(components::Group(std::move(items)));
        })
                       .help(help->body("settings/hapticIntensity"))
                       .id(composition::ElementKey{ "controls.intensity" }));
    return structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/settings/rumble_source_heading")) },
        std::move(rows));
}

}
