#pragma once

#include <memory>

#include "platform/stream_controls.hpp"

namespace akira::platform
{

class OunceStreamControls final : public StreamControls
{
public:
    explicit OunceStreamControls(std::weak_ptr<StreamControlsHost> host);

    void attach() override {}
    bool beforeStart() override;
    void streaming() override {}
    void decorateMenu(StreamMenu&) override {}

private:
    std::weak_ptr<StreamControlsHost> m_host;
};

}
