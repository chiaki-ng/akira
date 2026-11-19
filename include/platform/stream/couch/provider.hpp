#pragma once

#include <memory>

#include <switch.h>

#include "platform/stream/couch/players_panel_view.hpp"
#include "platform/stream/provider.hpp"

namespace akira::platform::extended
{
class ExtendedPadProvider;
}

namespace akira::platform::couch
{

class CouchStreamControlsProvider final : public StreamControlsProvider
{
public:
    CouchStreamControlsProvider(extended::ExtendedPadProvider& pads, PlayersPanelView::Profiles profiles,
        std::weak_ptr<StreamControlsHost> host);

    void attach() override;
    bool beforeStart() override;
    void streaming() override;
    void decorateMenu(play::views::StreamMenu& menu) override;

private:
    void waitForPads(int attempt);
    void finishPadChoice();
    void abandonStart();
    void onPadArrived(HidNpadIdType npad);
    void openCouchClaim();
    void openPrimaryControllerPicker();

    extended::ExtendedPadProvider& m_pads;
    PlayersPanelView::Profiles m_profiles;
    std::weak_ptr<StreamControlsHost> m_host;
    bool m_pad_choice_done = false;
    bool m_arrival_prompt_open = false;
};

}
