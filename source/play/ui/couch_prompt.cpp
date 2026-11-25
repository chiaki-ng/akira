#include "play/ui/couch_prompt.hpp"

#include <string>
#include <utility>
#include <vector>

#include "app/arisa/confirmation.hpp"

namespace akira::play::ui {

namespace composition = ::arisa::composition;

CouchPrompt::CouchPrompt(Couch& couch, const app::Strings& strings)
    : couch(&couch)
    , strings(&strings)
{
}

auto CouchPrompt::body(composition::Context& context) const -> composition::Element
{
    static_cast<void>(context.read(couch->view()));
    Couch* held = couch;
    std::vector<app::arisa::Confirmation::Choice> choices;
    choices.push_back(app::arisa::Confirmation::Choice{ .label = strings->text("akira/couch/not_now"),
        .chosen = [held] { held->answerArrival(Couch::Arrival::NotNow); },
        .key = "couch-prompt.not-now" });
    choices.push_back(app::arisa::Confirmation::Choice{ .label = strings->text("akira/couch/add_player"),
        .chosen = [held] { held->answerArrival(Couch::Arrival::AddPlayer); },
        .emphasis = app::arisa::Confirmation::Emphasis::Prominent,
        .key = "couch-prompt.add",
        .symbol = "person_add" });
    return app::arisa::Confirmation(strings->text("akira/couch/controller_arrived"), std::string(), std::move(choices),
        [held] { held->answerArrival(Couch::Arrival::Dismissed); })
        .withWidth(width)
        .focusing(context.namespace_id(focus))
        .dimmed("couch-prompt");
}

}
