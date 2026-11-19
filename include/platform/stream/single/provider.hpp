#pragma once

#include <memory>

#include "platform/stream/provider.hpp"

namespace akira::platform::single
{

class SingleStreamControlsProvider final : public StreamControlsProvider
{
public:
    explicit SingleStreamControlsProvider(std::weak_ptr<StreamControlsHost> host);

    void attach() override {}
    bool beforeStart() override;
    void streaming() override {}
    void decorateMenu(play::views::StreamMenu&) override {}

private:
    std::weak_ptr<StreamControlsHost> m_host;
};

}
