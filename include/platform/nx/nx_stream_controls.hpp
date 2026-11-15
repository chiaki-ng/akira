#pragma once

#include <memory>

#include <switch.h>

#include "platform/stream_controls.hpp"

namespace akira::platform
{

class NxPadSource;

class NxStreamControls final : public StreamControls
{
public:
    NxStreamControls(NxPadSource& pads, std::weak_ptr<StreamControlsHost> host);

    void attach() override;
    bool beforeStart() override;
    void streaming() override;
    void decorateMenu(StreamMenu& menu) override;

private:
    void waitForPads(int attempt);
    void finishPadChoice();
    void abandonStart();
    void onPadArrived(HidNpadIdType npad);
    void openCouchClaim();
    void openPrimaryControllerPicker();

    NxPadSource& m_pads;
    std::weak_ptr<StreamControlsHost> m_host;
    bool m_pad_choice_done = false;
    bool m_arrival_prompt_open = false;
};

}
