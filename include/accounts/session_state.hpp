#pragma once

namespace akira::accounts {

enum class SessionState {
    NotLinked,
    Valid,
    Expired,
    Refreshing
};

}
