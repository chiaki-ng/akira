#include "consoles/psn_identity.hpp"

#include <utility>

namespace akira::consoles {

PsnIdentity::PsnIdentity(std::string accountId, std::string onlineId)
    : accountId(std::move(accountId))
    , onlineId(std::move(onlineId))
{
}

std::optional<PsnIdentity::Missing> PsnIdentity::missingFor(bool needsAccountId) const
{
    if (needsAccountId && accountId.empty())
        return Missing::AccountId;
    if (!needsAccountId && onlineId.empty())
        return Missing::OnlineId;
    return std::nullopt;
}

}
