#pragma once

#include <optional>
#include <string>

namespace akira::consoles {

class PsnIdentity {
public:
    enum class Missing { AccountId, OnlineId };

    PsnIdentity(std::string accountId, std::string onlineId);

    std::optional<Missing> missingFor(bool needsAccountId) const;

private:
    std::string accountId;
    std::string onlineId;
};

}
