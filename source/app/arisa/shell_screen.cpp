#include "app/arisa/shell_screen.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

ShellScreen::ShellScreen(const ShellModel& model, ShellCommands& commands, const app::Strings& strings)
    : model(&model)
    , commands(&commands)
    , strings(&strings)
{
}

bool ShellScreen::operator==(const ShellScreen& other) const
{
    return model == other.model && commands == other.commands && strings == other.strings;
}

auto ShellScreen::body(composition::Context& context) const -> composition::Element
{
    const ShellModel::View& view = context.read(model->view());
    switch (view.phase)
    {
        case ShellModel::Phase::Connecting: return connecting(view);
        case ShellModel::Phase::Streaming:  return streaming();
        case ShellModel::Phase::Consoles:   break;
    }
    return consoleList(view);
}

auto ShellScreen::consoleList(const ShellModel::View& view) const -> composition::Element
{
    std::vector<composition::Element> rows;
    rows.push_back(components::Text("akira").font(theme::Font::large_title));
    if (!view.status.empty())
        rows.push_back(components::Text(view.status).font(theme::Font::callout));
    if (view.consoles.empty())
        rows.push_back(components::Text(strings->text("akira/hosts/no_consoles")).font(theme::Font::body));
    for (const ShellModel::Console& console : view.consoles)
    {
        const std::string label = console.paired ? console.name : console.name + " · " + strings->text("akira/hosts/register");
        rows.push_back(components::Button(label, [commands = commands, name = console.catalogName] {
            commands->connect(name);
        }).disabled(!console.paired).id(composition::ElementKey{console.catalogName}));
    }
    rows.push_back(components::Button(strings->text("akira/common/close"), [commands = commands] {
        commands->quit();
    }).id(composition::ElementKey{"quit"}));
    return components::VStack(components::VStackOptions{ .spacing = theme::space::md }, std::move(rows))
        .padding(theme::space::xl);
}

auto ShellScreen::connecting(const ShellModel::View& view) const -> composition::Element
{
    return components::VStack(components::VStackOptions{ .spacing = theme::space::md },
        components::Text(view.target).font(theme::Font::title),
        components::Text(view.status).font(theme::Font::body),
        components::Button(strings->text("akira/common/cancel"), [commands = commands] { commands->cancel(); })
            .id(composition::ElementKey{"cancel"}))
        .padding(theme::space::xl);
}

auto ShellScreen::streaming() const -> composition::Element
{
    return components::Text("");
}

}
