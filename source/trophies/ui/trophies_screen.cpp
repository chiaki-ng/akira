#include "trophies/ui/trophies_screen.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/presentation_sizing.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "trophies/ui/trophy_list.hpp"
#include "trophies/ui/trophy_page.hpp"
#include "trophies/ui/trophy_sheet.hpp"

namespace akira::trophies::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

TrophiesScreen::TrophiesScreen(TrophyModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto TrophiesScreen::body(composition::Context& context) const -> composition::Element
{
    const TrophyModel::View& view = context.read(model->view());
    std::optional<TrophyModel::Trophy> shown;
    if (view.page)
        shown = view.page->shown;
    return content(view)
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .on_appear([model = model] { model->appear(); })
        .sheet(detailShown(), [model = model, strings = strings, shown] {
            if (!shown)
                return composition::Element(components::Group());
            return composition::Element(TrophySheet(*shown, *model, *strings))
                .presentation_sizing(composition::PresentationSizing::fitted);
        })
        .id(composition::ElementKey{ "trophies" });
}

auto TrophiesScreen::content(const TrophyModel::View& view) const -> composition::Element
{
    switch (view.phase)
    {
        case TrophyModel::Phase::NotLinked:
            return unavailable(strings->text("akira/trophies/no_psn_token"), "trophy", false)
                .id(composition::ElementKey{ "trophies.not-linked" });
        case TrophyModel::Phase::Disabled:
            return unavailable(strings->text("akira/trophies/disabled_for_profile"), "visibility_off", false)
                .id(composition::ElementKey{ "trophies.disabled" });
        case TrophyModel::Phase::Failed:
            return unavailable(view.error, "error", true).id(composition::ElementKey{ "trophies.failed" });
        case TrophyModel::Phase::Loading:
            return composition::Element(controls::ProgressView(strings->text("akira/trophies/loading")))
                .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
                .id(composition::ElementKey{ "trophies.loading" });
        case TrophyModel::Phase::Ready: break;
    }
    if (view.page)
        return composition::Element(TrophyPage(*model, *strings)).id(composition::ElementKey{ "trophies.page" });
    return composition::Element(TrophyList(*model, *strings)).id(composition::ElementKey{ "trophies.list" });
}

auto TrophiesScreen::unavailable(const std::string& title, const std::string& symbol, bool retry) const
    -> composition::Element
{
    std::vector<composition::Element> shown;
    shown.push_back(components::ContentUnavailableView(title, symbol));
    if (retry)
        shown.push_back(components::Button(strings->text("akira/trophies/refresh"),
            components::Image::SystemName{ "refresh" }, [model = model] { model->retry(); })
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .id(composition::ElementKey{ "trophies.retry" }));
    return components::VStack(components::VStackOptions{ .spacing = theme::space::lg }, std::move(shown))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity });
}

auto TrophiesScreen::detailShown() const -> composition::Binding<bool>
{
    return composition::Binding<bool>(
        [model = model] {
            const auto& page = model->view().get().page;
            return page.has_value() && page->shown.has_value();
        },
        [model = model](bool open) {
            if (!open)
                model->closeTrophy();
        });
}

}
