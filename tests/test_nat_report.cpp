#include "test_util.hpp"

#include "network/nat_report.hpp"

#include <string>

using akira::network::NatReport;
using libchiaking::play::NatType;
using libchiaking::play::NatTypeFailure;

namespace {

NatReport failed(NatTypeFailure::Reason reason)
{
    return NatReport(std::unexpected(NatTypeFailure{reason}));
}

}

TEST(nat_report_shows_the_core_type)
{
    NatReport open(NatType::type1);
    CHECK(open.found());
    CHECK(open.type() == std::optional<int>(1));
    CHECK(open.tone() == NatReport::Tone::Good);
    CHECK_EQ(std::string(open.detailKey()), std::string("akira/network/nat_detail_type1"));

    CHECK(NatReport(NatType::type2).tone() == NatReport::Tone::Good);
    CHECK_EQ(std::string(NatReport(NatType::type2).detailKey()), std::string("akira/network/nat_detail_type2"));
    CHECK(NatReport(NatType::type3).tone() == NatReport::Tone::Warning);
    CHECK(NatReport(NatType::type3).type() == std::optional<int>(3));
}

TEST(nat_report_explains_each_failure)
{
    CHECK(!failed(NatTypeFailure::Reason::unanswered).found());
    CHECK(!failed(NatTypeFailure::Reason::unanswered).type().has_value());
    CHECK(failed(NatTypeFailure::Reason::unanswered).tone() == NatReport::Tone::Bad);
    CHECK_EQ(std::string(failed(NatTypeFailure::Reason::unanswered).detailKey()),
             std::string("akira/network/nat_failed_unanswered"));
    CHECK_EQ(std::string(failed(NatTypeFailure::Reason::incomplete).detailKey()),
             std::string("akira/network/nat_failed_incomplete"));
    CHECK_EQ(std::string(failed(NatTypeFailure::Reason::unresolved).detailKey()),
             std::string("akira/network/nat_failed_unresolved"));
    CHECK_EQ(std::string(failed(NatTypeFailure::Reason::network).detailKey()), std::string("akira/network/nat_failed"));
    CHECK_EQ(std::string(failed(NatTypeFailure::Reason::cancelled).detailKey()), std::string("akira/network/nat_failed"));
}
