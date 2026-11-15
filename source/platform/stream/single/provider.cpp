#include "platform/stream/single/provider.hpp"

namespace akira::platform::single
{

SingleStreamControlsProvider::SingleStreamControlsProvider(std::weak_ptr<StreamControlsHost> host)
    : m_host(std::move(host))
{
}

bool SingleStreamControlsProvider::beforeStart()
{
    return false;
}

}
