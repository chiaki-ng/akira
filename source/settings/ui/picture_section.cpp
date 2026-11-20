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

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <iterator>
#include <utility>
#include <vector>

#include "cloud/cloud_settings.hpp"
#include "play/stream_settings.hpp"

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

PictureSection::PictureSection(PictureOptions& options, SettingsModel& model, const HelpCatalog& help,
    const app::Strings& strings)
    : options(&options)
    , model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto PictureSection::helpId(play::StreamProfile profile, std::string_view row) -> std::string
{
    std::string prefix = "settings/local";
    if (profile == play::StreamProfile::Remote)
        prefix = "settings/remote";
    else if (profile == play::StreamProfile::Vpn)
        prefix = "settings/vpn";
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
    static_cast<void>(context.read(model->view()));
    const bool legacy = model->parts().maintenance.legacyProfile();
    const auto chosen = context.state(route);
    const play::StreamProfile shown =
        legacy && chosen.get() == play::StreamProfile::Remote ? play::StreamProfile::Local : chosen.get();
    const play::PictureProfile& profile = options->profile(shown);
    const SettingRows rows(*model, *help, *strings);
    std::vector<composition::Element> sections;
    sections.push_back(structure::Section(routePicker(chosen, legacy)));
    sections.push_back(stream(profile));
    sections.push_back(upscaling(profile, values));
    if (model->features().cloud && !legacy)
    {
        std::vector<composition::Element> services = cloud(rows, true);
        std::ranges::move(cloud(rows, false), std::back_inserter(services));
        sections.push_back(rows.section(rows.text("akira/settings/cloud"), std::move(services)));
    }
    sections.push_back(adjustments(rows));
    if (std::vector<composition::Element> choices = renderer(rows); !choices.empty())
        sections.push_back(rows.section(rows.text("akira/settings/renderer"), std::move(choices)));
    return structure::Form(std::move(sections));
}

auto PictureSection::cloud(const SettingRows& rows, bool pscloud) const -> std::vector<composition::Element>
{
    ::akira::cloud::CloudSettings& settings = model->parts().cloud;
    play::StreamSettings& stream = model->parts().stream;
    const bool hevcTiers = pscloud && model->features().hevc;
    std::vector<std::pair<int, std::string>> choices{ { 0, rows.text("akira/settings/res_720p") },
        { 1, rows.text("akira/settings/res_1080p") }, { 2, "1080p (FSR)" } };
    if (hevcTiers)
    {
        choices.emplace_back(3, "1440p");
        choices.emplace_back(4, "2160p");
    }
    auto index = [&settings, pscloud, hevcTiers] {
        if (settings.fsrEnabled(pscloud))
            return 2;
        const int height = settings.videoResolution(pscloud);
        if (hevcTiers && height >= 2160)
            return 4;
        if (hevcTiers && height >= 1440)
            return 3;
        return height <= 720 ? 0 : 1;
    };
    auto choose = [&settings, &stream, pscloud](int selected) {
        constexpr std::array heights{ 720, 1080, 720, 1440, 2160 };
        const int height = selected >= 0 && selected < static_cast<int>(heights.size()) ? heights[static_cast<std::size_t>(selected)] : 1080;
        settings.setVideoResolution(pscloud, height);
        settings.setFsrEnabled(pscloud, selected == 2);
        settings.setVideoBitrate(pscloud, stream.defaultBitrate(play::resolutionFromHeight(height)));
    };
    const play::VideoResolution resolution = play::resolutionFromHeight(settings.videoResolution(pscloud));
    const int lowest = stream.minBitrate(resolution);
    const int highest = stream.maxBitrate(resolution);
    const int current = std::clamp(settings.videoBitrate(pscloud), lowest, highest);
    std::vector<composition::Element> shown;
    shown.push_back(rows.picker<int>(rows.text(pscloud ? "akira/settings/pscloud_resolution" : "akira/settings/psnow_resolution"),
        pscloud ? "settings/pscloudResolution" : "settings/psnowResolution", std::move(choices), index, choose));
    shown.push_back(rows.slider(rows.text(pscloud ? "akira/settings/pscloud_bitrate" : "akira/settings/psnow_bitrate"),
        pscloud ? "settings/pscloudBitrate" : "settings/psnowBitrate",
        controls::ClosedRange<double>{ .lower_bound = static_cast<double>(lowest), .upper_bound = static_cast<double>(highest) },
        static_cast<double>(play::PictureProfile::bitrateStep),
        [&settings, pscloud] { return static_cast<double>(settings.videoBitrate(pscloud)); },
        [&settings, pscloud, lowest, highest](double value) {
            const int snapped = ((static_cast<int>(std::lround(value)) + 250) / 500) * 500;
            settings.setVideoBitrate(pscloud, std::clamp(snapped, lowest, highest));
        },
        strings->format("akira/settings/kbps", current)));
    return shown;
}

auto PictureSection::adjustments(const SettingRows& rows) const -> composition::Element
{
    play::StreamSettings& stream = model->parts().stream;
    std::vector<composition::Element> shown;
    shown.push_back(rows.toggle(rows.text("akira/settings/enable_dithering"), "settings/enableDithering",
        [&stream] { return stream.ditheringEnabled(); }, [&stream](bool value) { stream.setDitheringEnabled(value); }));
    shown.push_back(rows.slider(rows.text("akira/settings/dithering_strength"), "settings/ditheringStrength",
        controls::ClosedRange<double>{ .lower_bound = 1.0, .upper_bound = 10.0 }, 1.0,
        [&stream] { return static_cast<double>(stream.ditheringStrength()); },
        [&stream](double value) { stream.setDitheringStrength(static_cast<float>(std::floor(value))); },
        std::format("{}", static_cast<int>(stream.ditheringStrength()))));
    return rows.section(rows.text("akira/settings/picture_adjustments"), std::move(shown));
}

auto PictureSection::renderer(const SettingRows& rows) const -> std::vector<composition::Element>
{
    play::StreamSettings& stream = model->parts().stream;
    const DeviceFeatures& features = model->features();
    std::vector<composition::Element> shown;
    if (features.renderers.size() > 1)
    {
        std::vector<std::pair<play::RendererKind, std::string>> choices;
        for (const DeviceFeatures::Renderer& choice : features.renderers)
            choices.emplace_back(choice.kind, choice.label);
        shown.push_back(rows.picker<play::RendererKind>(rows.text("akira/settings/renderer_choice"), "settings/rendererChoice",
            std::move(choices), [&stream] { return stream.videoRenderer(); },
            [&stream](play::RendererKind kind) { stream.setVideoRenderer(kind); }));
    }
    if (features.softwareHevc || features.zeroCopy)
        shown.push_back(rows.value(rows.text("akira/settings/renderer"), features.videoName));
    if (features.softwareHevc)
        shown.push_back(rows.toggle(rows.text("akira/settings/software_hevc"), "settings/softwareHevc",
            [&stream] { return stream.softwareHevc(); }, [&stream](bool value) { stream.setSoftwareHevc(value); }));
    if (features.zeroCopy)
        shown.push_back(rows.toggle(rows.text("akira/settings/zero_copy"), "settings/zeroCopy",
            [&stream] { return stream.zeroCopyVideo(); }, [&stream](bool value) { stream.setZeroCopyVideo(value); }));
    return shown;
}

auto PictureSection::routePicker(composition::StateHandle<play::StreamProfile> chosen, bool legacy) const
    -> composition::Element
{
    const std::string local = strings->text("akira/settings/profile_local");
    const std::string remote = strings->text("akira/settings/profile_remote");
    const std::string vpn = strings->text("akira/settings/profile_vpn");
    return controls::Picker(strings->text("akira/settings/stream_settings_for"),
        composition::Binding<play::StreamProfile>(
            [chosen, legacy] {
                return legacy && chosen.get() == play::StreamProfile::Remote ? play::StreamProfile::Local : chosen.get();
            },
            [chosen](play::StreamProfile value) { chosen.set(value); }),
        [local, remote, vpn, legacy] {
            std::vector<composition::Element> items;
            items.push_back(components::Text(local).tag(play::StreamProfile::Local));
            if (!legacy)
                items.push_back(components::Text(remote).tag(play::StreamProfile::Remote));
            items.push_back(components::Text(vpn).tag(play::StreamProfile::Vpn));
            return composition::Element(components::Group(std::move(items)));
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
