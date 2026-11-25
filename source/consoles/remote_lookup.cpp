#include "consoles/remote_lookup.hpp"

namespace akira::consoles {

RemoteLookup::RemoteLookup(std::optional<accounts::SessionState> session)
    : state(session.value_or(accounts::SessionState::NotLinked))
{
}

RemoteLookup::Notice RemoteLookup::notice() const
{
    if (state == accounts::SessionState::NotLinked)
        return Notice::NoToken;
    return state == accounts::SessionState::Valid ? Notice::Finding : Notice::RefreshingToken;
}

}
