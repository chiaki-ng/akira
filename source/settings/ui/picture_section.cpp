#include "settings/ui/picture_section.hpp"

#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <cmath>
#include <utility>
#include <vector>

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

PictureSection::PictureSection(PictureOptions& options, const HelpCatalog& help, const app::Strings& strings)
    : options(&options)
    , help(&help)
    , strings(&strings)
{
}

auto PictureSection::helpId(play::StreamProfile profile, std::string_view row) -> std::string
{
    const std::string prefix = profile == play::StreamProfile::Remote ? "settings/remote" : "settings/local";
    return prefix + std::string(row);
}

auto PictureSection::upscaleLabel(int target) const -> std::string
{
    if (target <= 0)
        return strings->text("akira/settings/upscale_off");
    return play::toString(play::resolutionFromHeight(target));
}

auto PictureSection::body(composition::Context& context) const -> composition::Element
{
    const PictureOptions::Values& values = context.read(options->view());
    const auto chosen = context.state(route);
    const play::PictureProfile& profile = options->profile(chosen.get());
    return structure::Form(
        structure::Section(routePicker(chosen)),
        stream(profile),
        upscaling(profile, values));
}

auto PictureSection::routePicker(composition::StateHandle<play::StreamProfile> chosen) const -> composition::Element
{
    const std::string local = strings->text("akira/settings/profile_local");
    const std::string remote = strings->text("akira/settings/profile_remote");
    return controls::Picker(strings->text("akira/settings/stream_settings_for"),
        composition::Binding<play::StreamProfile>(chosen),
        [local, remote] {
            return composition::Element(components::Group(components::Text(local).tag(play::StreamProfile::Local),
                components::Text(remote).tag(play::StreamProfile::Remote)));
        })
        .picker_style(controls::PickerStyle::segmented())
        .help(help->title("settings/streamProfile"), help->body("settings/streamProfile"))
        .id(composition::ElementKey{ "picture.route" });
}

auto PictureSection::stream(const play::PictureProfile& profile) const -> composition::Element
{
    const play::StreamProfile which = profile.profile();
    PictureOptions* edit = options;
    std::vector<composition::Element> rows;
    if (!profile.upscaling())
    {
        std::vector<std::pair<play::VideoResolution, std::string>> choices;
        for (const play::VideoResolution resolution : play::PictureProfile::resolutions())
            choices.emplace_back(resolution, strings->text("akira/settings/res_" + play::toString(resolution)));
        rows.push_back(controls::Picker(strings->text("akira/settings/resolution"),
            composition::Binding<play::VideoResolution>(
                [edit, which] { return edit->profile(which).resolution(); },
                [edit, which](play::VideoResolution value) { edit->chooseResolution(which, value); }),
            [choices] {
                std::vector<composition::Element> items;
                for (const auto& [resolution, label] : choices)
                    items.push_back(components::Text(label).tag(resolution));
                return composition::Element(components::Group(std::move(items)));
            })
                           .help(help->title(helpId(which, "Resolution")), help->body(helpId(which, "Resolution")))
                           .id(composition::ElementKey{ "picture.resolution" }));
    }
    const std::string fps30 = strings->text("akira/settings/fps_30");
    const std::string fps60 = strings->text("akira/settings/fps_60");
    rows.push_back(controls::Picker(strings->text("akira/settings/frame_rate"),
        composition::Binding<play::VideoFps>([edit, which] { return edit->profile(which).fps(); },
            [edit, which](play::VideoFps value) { edit->chooseFps(which, value); }),
        [fps30, fps60] {
            return composition::Element(components::Group(components::Text(fps30).tag(play::VideoFps::Fps30),
                components::Text(fps60).tag(play::VideoFps::Fps60)));
        })
                       .help(help->title(helpId(which, "Fps")), help->body(helpId(which, "Fps")))
                       .id(composition::ElementKey{ "picture.fps" }));
    rows.push_back(components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::xs },
        components::HStack(components::Text(strings->text("akira/settings/bitrate_short")), components::Spacer(),
            components::Text(strings->format("akira/settings/kbps", profile.bitrate()))
                .foreground_style(theme::ColourRole::secondary_label)),
        controls::Slider(composition::Binding<double>(
                             [edit, which] { return static_cast<double>(edit->profile(which).bitrate()); },
                             [edit, which](double value) { edit->chooseBitrate(which, static_cast<int>(std::lround(value))); }),
            controls::ClosedRange<double>{ .lower_bound = static_cast<double>(profile.minBitrate()),
                .upper_bound = static_cast<double>(profile.maxBitrate()) },
            static_cast<double>(play::PictureProfile::bitrateStep),
            components::Text(strings->text("akira/settings/bitrate_short")))
            .labels_hidden())
                       .help(help->title(helpId(which, "Bitrate")), help->body(helpId(which, "Bitrate")))
                       .id(composition::ElementKey{ "picture.bitrate" }));
    return structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/settings/stream_heading")) },
        std::move(rows));
}

auto PictureSection::upscaling(const play::PictureProfile& profile, const PictureOptions::Values& values) const
    -> composition::Element
{
    const play::StreamProfile which = profile.profile();
    PictureOptions* edit = options;
    std::vector<std::pair<int, std::string>> choices{ { 0, upscaleLabel(0) } };
    for (const int target : profile.upscaleTargets())
        choices.emplace_back(target, upscaleLabel(target));
    std::vector<composition::Element> rows;
    rows.push_back(controls::Picker(strings->text("akira/settings/upscale_to"),
        composition::Binding<int>([edit, which] { return edit->profile(which).upscaleTarget(); },
            [edit, which](int value) { edit->chooseUpscale(which, value); }),
        [choices] {
            std::vector<composition::Element> items;
            for (const auto& [target, label] : choices)
                items.push_back(components::Text(label).tag(target));
            return composition::Element(components::Group(std::move(items)));
        })
                       .help(help->title("settings/upscaleTarget"), help->body("settings/upscaleTarget"))
                       .id(composition::ElementKey{ "picture.upscale" }));
    if (profile.upscaling())
        rows.push_back(components::LabeledContent(strings->text("akira/settings/streams_at"),
            play::toString(profile.resolution()))
                           .id(composition::ElementKey{ "picture.source" }));
    rows.push_back(controls::Toggle(strings->text("akira/settings/rcas"),
        composition::Binding<bool>([edit] { return edit->view().get().rcas; },
            [edit](bool value) { edit->setRcas(value); }))
                       .help(help->title("settings/rcasEnabled"), help->body("settings/rcasEnabled"))
                       .id(composition::ElementKey{ "picture.rcas" }));
    if (values.rcas)
        rows.push_back(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                              .spacing = theme::space::xs },
            components::HStack(components::Text(strings->text("akira/settings/rcas_strength")), components::Spacer(),
                components::Text(strings->format("akira/settings/percent_format", values.rcasStrength))
                    .foreground_style(theme::ColourRole::secondary_label)),
            controls::Slider(composition::Binding<double>([edit] { return static_cast<double>(edit->view().get().rcasStrength); },
                                 [edit](double value) { edit->setRcasStrength(static_cast<int>(std::lround(value))); }),
                controls::ClosedRange<double>{ .lower_bound = 0.0, .upper_bound = 100.0 }, 5.0,
                components::Text(strings->text("akira/settings/rcas_strength")))
                .labels_hidden())
                           .help(help->title("settings/rcasSharpness"), help->body("settings/rcasSharpness"))
                           .id(composition::ElementKey{ "picture.rcas-strength" }));
    return structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/settings/upscaling")) },
        std::move(rows));
}

}
