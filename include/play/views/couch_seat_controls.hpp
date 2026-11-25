#pragma once

#include <memory>

#include <switch.h>

#include "play/views/players_panel_view.hpp"
#include "play/views/stream_controls_host.hpp"

namespace akira::play
{
class CouchSeats;
}

namespace akira::play::views
{

class StreamMenu;

class CouchSeatControls
{
public:
    CouchSeatControls(CouchSeats& seats, PlayersPanelView::Profiles profiles, std::weak_ptr<StreamControlsHost> host);

    void attach();
    bool beforeStart();
    void streaming();
    void decorateMenu(StreamMenu& menu);

private:
    void waitForPads(int attempt);
    void finishPadChoice();
    void abandonStart();
    void onPadArrived(HidNpadIdType npad);
    void openCouchClaim();
    void openPrimaryControllerPicker();

    CouchSeats& m_pads;
    PlayersPanelView::Profiles m_profiles;
    std::weak_ptr<StreamControlsHost> m_host;
    bool m_pad_choice_done = false;
    bool m_arrival_prompt_open = false;
};

}
