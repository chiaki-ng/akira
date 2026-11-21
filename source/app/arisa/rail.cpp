#include "app/arisa/rail.hpp"

#include <arisa/components/control_size.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/structure/dock_bar.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;

Rail::Rail(ShellModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto Rail::body(composition::Context& context) const -> composition::Element
{
    const ShellModel::View& view = context.read(model->view());
    const std::vector<ShellModel::Page> shown = pages(view);
    std::vector<structure::DockBar::Item> items;
    for (const ShellModel::Page page : shown)
        items.push_back(structure::DockBar::Item{ .title = title(page), .system_image = symbol(page) });
    ShellModel* held = model;
    std::vector<structure::DockBar::Action> trailing;
    trailing.push_back(structure::DockBar::Action{ .title = strings->text("akira/tabs/settings"),
        .system_image = "settings",
        .perform = [held] { held->showSettings(true); } });
    const auto current = std::ranges::find(shown, view.page);
    const std::size_t index = current == shown.end() ? 0 : static_cast<std::size_t>(std::distance(shown.begin(), current));
    composition::Binding<std::size_t> selection([index] { return index; },
        [held, shown](std::size_t chosen) {
            if (chosen < shown.size())
                held->show(shown[chosen]);
        });
    return composition::Element(structure::DockBar(std::move(selection), std::move(items), std::move(trailing),
                                    structure::DockBar::Options{ .axis = composition::Axis::vertical }))
        .control_size(components::ControlSize::small)
        .id(composition::ElementKey{ "rail" });
}

auto Rail::pages(const ShellModel::View& view) -> std::vector<ShellModel::Page>
{
    std::vector<ShellModel::Page> shown{ ShellModel::Page::Consoles };
    if (view.cloudOffered)
        shown.push_back(ShellModel::Page::Cloud);
    if (view.trophiesOffered)
        shown.push_back(ShellModel::Page::Trophies);
    return shown;
}

auto Rail::title(ShellModel::Page page) const -> std::string
{
    switch (page)
    {
        case ShellModel::Page::Consoles: return strings->text("akira/tabs/consoles");
        case ShellModel::Page::Cloud: return strings->text("akira/cloud/card_title");
        case ShellModel::Page::Trophies: return strings->text("akira/tabs/trophies");
    }
    return {};
}

auto Rail::symbol(ShellModel::Page page) -> std::string
{
    switch (page)
    {
        case ShellModel::Page::Consoles: return "sports_esports";
        case ShellModel::Page::Cloud: return "cloud";
        case ShellModel::Page::Trophies: return "trophy";
    }
    return "circle";
}

}
