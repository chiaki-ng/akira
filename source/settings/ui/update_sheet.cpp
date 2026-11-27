#include "settings/ui/update_sheet.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/text/byte_count_format_style.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include "app/version.hpp"

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace text = ::arisa::text;
namespace theme = ::arisa::theme;

using Stage = Maintenance::UpdateProgress::Stage;

UpdateSheet::UpdateSheet(Maintenance::UpdateCheck update, std::optional<Maintenance::UpdateProgress> progress, SettingsModel& model,
    const app::Strings& strings)
    : update(std::move(update))
    , progress(std::move(progress))
    , model(&model)
    , strings(&strings)
{
}

auto UpdateSheet::body(composition::Context&) const -> composition::Element
{
    return (progress ? installing(*progress) : offer()).padding(theme::Space{ 24.0F }).frame({ .min_width = 440.0F, .max_width = 560.0F });
}

auto UpdateSheet::offer() const -> composition::Element
{
    std::vector<composition::Element> shown;
    shown.push_back(components::Text(strings->text("akira/update/prompt_title")).font(theme::Font::headline));
    shown.push_back(components::Text(strings->format("akira/update/prompt_body", update.version, app::version::semver())));
    if (!update.notes.empty())
        shown.push_back(notes());
    shown.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::md }, components::Spacer(),
        components::Button(strings->text("akira/update/later"), components::ButtonRole::cancel, [model = model] { model->dismissOffer(); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear())),
        components::Button(strings->text("akira/update/update_now"), [model = model] { model->installUpdate(); })
            .button_style(components::ButtonStyle::glass_prominent())
            .id(composition::ElementKey{ "settings-update.install" })));
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::lg }, std::move(shown));
}

auto UpdateSheet::notes() const -> composition::Element
{
    std::string written = update.notes;
    std::erase(written, '\r');
    return components::ScrollView(components::Text::markdown(written)
                                      .padding(theme::Space{ 14.0F })
                                      .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading }))
        .frame({ .max_height = notesHeight })
        .background(theme::ColourValue(theme::ColourRole::label).opacity(0.05F), theme::Shape::rounded(12.0F))
        .id(composition::ElementKey{ "settings-update.notes" });
}

auto UpdateSheet::installing(const Maintenance::UpdateProgress& shown) const -> composition::Element
{
    const bool done = settled(shown);
    auto close = components::Button(strings->text(done ? "akira/common/ok" : "akira/common/cancel"), components::ButtonRole::cancel,
        [model = model] { model->dismissInstall(); })
                     .id(composition::ElementKey{ "settings-update.close" });
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::lg },
        components::Text(strings->format("akira/update/updating_to", update.version)).font(theme::Font::headline),
        stage(shown),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, components::Spacer(),
            done ? std::move(close).button_style(components::ButtonStyle::glass_prominent())
                 : std::move(close).button_style(components::ButtonStyle::glass(theme::Glass::clear()))));
}

auto UpdateSheet::stage(const Maintenance::UpdateProgress& shown) const -> composition::Element
{
    switch (shown.stage)
    {
        case Stage::Downloading:
            if (shown.total <= 0)
                return composition::Element(controls::ProgressView(strings->text("akira/update/downloading_label")))
                    .frame({ .max_width = composition::infinity });
            return composition::Element(controls::ProgressView(static_cast<double>(shown.received), static_cast<double>(shown.total),
                composition::Element(components::Text(strings->text("akira/update/downloading_label"))),
                composition::Element(components::Text(transferred(shown)))));
        case Stage::Verifying:
            return composition::Element(controls::ProgressView(strings->text("akira/update/verifying")))
                .frame({ .max_width = composition::infinity });
        case Stage::Installed: return composition::Element(components::Text(strings->text("akira/update/installed_relaunch")));
        case Stage::Failed: break;
    }
    return composition::Element(components::Text(shown.error));
}

auto UpdateSheet::transferred(const Maintenance::UpdateProgress& shown) const -> std::string
{
    const text::ByteCountFormatStyle style;
    const text::Locale locale = text::Locale::undetermined();
    return strings->format("akira/update/transferred", style.format(shown.received, locale), style.format(shown.total, locale));
}

auto UpdateSheet::settled(const Maintenance::UpdateProgress& shown) -> bool
{
    return shown.stage == Stage::Installed || shown.stage == Stage::Failed;
}

}
