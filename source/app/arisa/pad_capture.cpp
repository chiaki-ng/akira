#include "app/arisa/pad_capture.hpp"

namespace akira::app::arisa {

PadCapture::PadCapture(::arisa::runtime::Application& application)
    : application(application)
{
    padInitializeDefault(&pad);
}

void PadCapture::begin()
{
    if (blocked)
        return;
    blocked.emplace(application.block_input());
}

auto PadCapture::held() -> std::uint64_t
{
    padUpdate(&pad);
    return padGetButtons(&pad);
}

void PadCapture::end()
{
    blocked.reset();
}

}
