#include "views/psn_gated_box.hpp"
#include "ui/theme.hpp"
#include "core/chiaki_core.hpp"

using namespace brls::literals;

std::unordered_set<PsnGatedBox*> PsnGatedBox::liveBoxes;
bool PsnGatedBox::observerInstalled = false;

PsnGatedBox::PsnGatedBox()
{
    this->setAxis(brls::Axis::COLUMN);

    placeholder = new brls::Label();
    placeholder->setFontSize(18);
    placeholder->setTextColor(akira::ui::active().textDim);
    placeholder->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    placeholder->setVerticalAlign(brls::VerticalAlign::CENTER);
    placeholder->setVisibility(brls::Visibility::GONE);
    this->addView(placeholder);

    liveBoxes.insert(this);

    if (!observerInstalled)
    {
        observerInstalled = true;
        if (auto* accounts = akira::ChiakiCore::current().accounts()) {
            accounts->setStateObserver([]() {
                brls::sync([]() { PsnGatedBox::evaluateAll(); });
            });
        }
    }
}

PsnGatedBox::~PsnGatedBox()
{
    liveBoxes.erase(this);
}

brls::View* PsnGatedBox::create()
{
    return new PsnGatedBox();
}

void PsnGatedBox::evaluateAll()
{
    for (PsnGatedBox* box : liveBoxes)
        box->evaluate();
}

bool PsnGatedBox::evaluate()
{
    auto* accounts = akira::ChiakiCore::current().accounts();
    applyState(accounts ? accounts->state() : psn::SessionState::NotLinked);
    return gateOpen;
}

void PsnGatedBox::applyState(psn::SessionState state)
{
    if (applied && state == appliedState)
        return;

    applied = true;
    appliedState = state;

    bool open = state != psn::SessionState::NotLinked;
    gateOpen = open;

    for (brls::View* child : this->getChildren())
    {
        if (child == placeholder)
            continue;

        child->setVisibility(open ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }

    if (open)
    {
        this->setJustifyContent(brls::JustifyContent::FLEX_START);
        this->setAlignItems(brls::AlignItems::STRETCH);
        placeholder->setVisibility(brls::Visibility::GONE);
        return;
    }

    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setAlignItems(brls::AlignItems::CENTER);
    placeholder->setText("akira/trophies/no_psn_token"_i18n);
    placeholder->setVisibility(brls::Visibility::VISIBLE);

    brls::Logger::info("PSN gate closed: no account linked");
}
