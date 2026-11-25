#include "consoles/register_flow.hpp"

namespace akira::consoles {

RegisterFlow::Stage RegisterFlow::stage(int index)
{
    switch (index) {
        case 1: return Stage::Account;
        case 2: return Stage::Console;
        case 3: return Stage::Connect;
        default: return Stage::Finish;
    }
}

void RegisterFlow::beginPin()
{
    m_registering = true;
}

void RegisterFlow::stopped()
{
    m_registering = false;
}

RegisterFlow::Notice RegisterFlow::pinEnded(Host::PairingOutcome outcome)
{
    m_registering = false;
    switch (outcome) {
        case Host::PairingOutcome::Paired: return Notice::Success;
        case Host::PairingOutcome::Canceled: return Notice::Canceled;
        case Host::PairingOutcome::Refused:
        case Host::PairingOutcome::Failed: break;
    }
    return Notice::Failed;
}

std::optional<RegisterFlow::Refusal> RegisterFlow::pinRefused(HostRegisterError result)
{
    if (result == HOST_REGISTER_OK)
        return std::nullopt;
    m_registering = false;
    switch (result) {
        case HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID: return Refusal::AccountId;
        case HOST_REGISTER_ERROR_SETTING_PSNONLINEID: return Refusal::OnlineId;
        case HOST_REGISTER_ERROR_UNDEFINED_TARGET: return Refusal::ConsoleType;
        default: return Refusal::Failed;
    }
}

bool RegisterFlow::beginInternet()
{
    if (m_registering)
        return false;
    m_registering = true;
    m_canceled = false;
    return true;
}

bool RegisterFlow::internetStarted(HostRegisterError result)
{
    if (result == HOST_REGISTER_OK)
        return true;
    m_registering = false;
    return false;
}

void RegisterFlow::userCanceled()
{
    m_canceled = true;
}

RegisterFlow::InternetEnd RegisterFlow::internetEnded(Host::PairingOutcome outcome)
{
    m_registering = false;
    if (outcome == Host::PairingOutcome::Paired)
        return InternetEnd::Success;
    if (outcome == Host::PairingOutcome::Canceled || m_canceled)
        return InternetEnd::Quiet;
    return InternetEnd::FallBackToPin;
}

}
