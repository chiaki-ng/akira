#include "app/arisa/notices.hpp"

#include <utility>

#include "log/log.hpp"

namespace akira::app::arisa {

void Notices::notify(std::string text)
{
    log::info("[NOTICE] {}", text);
    state.set(Notice{ .serial = ++serial, .text = std::move(text) });
}

void Notices::clear()
{
    state.set(std::nullopt);
}

}
