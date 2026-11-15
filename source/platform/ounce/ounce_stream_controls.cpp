#include "platform/ounce/ounce_stream_controls.hpp"

namespace akira::platform
{

OunceStreamControls::OunceStreamControls(std::weak_ptr<StreamControlsHost> host)
    : m_host(std::move(host))
{
}

bool OunceStreamControls::beforeStart()
{
    return false;
}

}
