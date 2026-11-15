#pragma once

#include <optional>

#include "platform/ounce/hos_pad.hpp"
#include "platform/pad_source.hpp"

namespace akira::platform
{

class OuncePadSource final : public PadSource
{
public:
    void open() override;
    void close() override;
    input::Pad* acquire(Callbacks& callbacks) override;
    input::Pad* current() override;
    std::vector<input::PadDescription> scanPads() const override;

private:
    std::optional<HosPad> m_pad;
};

}
