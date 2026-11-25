#include "network/nat_probe.hpp"

#include "log/log.hpp"
#include "play/chiaki_core.hpp"

namespace akira::network {

NatProbe::NatProbe(play::ChiakiCore& core)
    : core(core)
{
}

NatProbe::Result NatProbe::check(const libchiaking::platform::StopToken& stop)
{
    using libchiaking::play::NatTypeFailure;

    if (!core.opened()) {
        log::warning("NAT: the core is not open, cannot check the NAT type");
        return std::unexpected(NatTypeFailure{NatTypeFailure::Reason::network});
    }

    Result result = core.chiaki().play().nat_type(stop);
    if (result)
        log::info("NAT: type {}", static_cast<int>(*result));
    else
        log::warning("NAT: check failed, reason {}", static_cast<int>(result.error().reason));
    return result;
}

}
