#pragma once

#include <optional>

#include "accounts/session_state.hpp"

namespace akira::consoles {

class RemoteLookup {
public:
    enum class Notice { NoToken, Finding, RefreshingToken };

    explicit RemoteLookup(std::optional<accounts::SessionState> session);

    bool allowed() const { return state != accounts::SessionState::NotLinked; }
    Notice notice() const;

private:
    accounts::SessionState state;
};

}
