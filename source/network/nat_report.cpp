#include "network/nat_report.hpp"

#include <utility>

namespace akira::network {

using libchiaking::play::NatType;
using libchiaking::play::NatTypeFailure;

NatReport::NatReport(libchiaking::Expected<NatType, NatTypeFailure> result)
    : result(std::move(result))
{
}

std::optional<int> NatReport::type() const
{
    if (!result)
        return std::nullopt;
    return static_cast<int>(*result);
}

const char* NatReport::detailKey() const
{
    if (result) {
        switch (*result) {
            case NatType::type1: return "akira/network/nat_detail_type1";
            case NatType::type2: return "akira/network/nat_detail_type2";
            case NatType::type3: return "akira/network/nat_detail_type3";
        }
    }
    switch (result.error().reason) {
        case NatTypeFailure::Reason::unanswered: return "akira/network/nat_failed_unanswered";
        case NatTypeFailure::Reason::incomplete: return "akira/network/nat_failed_incomplete";
        case NatTypeFailure::Reason::unresolved: return "akira/network/nat_failed_unresolved";
        case NatTypeFailure::Reason::network:
        case NatTypeFailure::Reason::cancelled: break;
    }
    return "akira/network/nat_failed";
}

NatReport::Tone NatReport::tone() const
{
    if (!result)
        return Tone::Bad;
    return *result == NatType::type3 ? Tone::Warning : Tone::Good;
}

}
