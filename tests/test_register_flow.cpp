#include "test_util.hpp"

#include "consoles/register_flow.hpp"

using akira::consoles::Host;
using akira::consoles::RegisterFlow;

TEST(register_flow_names_each_internet_stage)
{
    CHECK(RegisterFlow::stage(1) == RegisterFlow::Stage::Account);
    CHECK(RegisterFlow::stage(2) == RegisterFlow::Stage::Console);
    CHECK(RegisterFlow::stage(3) == RegisterFlow::Stage::Connect);
    CHECK(RegisterFlow::stage(4) == RegisterFlow::Stage::Finish);
    CHECK(RegisterFlow::stage(0) == RegisterFlow::Stage::Finish);
}

TEST(register_flow_pin_outcomes_clear_the_busy_flag)
{
    RegisterFlow flow;
    CHECK(!flow.registering());
    flow.beginPin();
    CHECK(flow.registering());
    CHECK(flow.pinEnded(Host::PairingOutcome::Paired) == RegisterFlow::Notice::Success);
    CHECK(!flow.registering());

    flow.beginPin();
    CHECK(flow.pinEnded(Host::PairingOutcome::Canceled) == RegisterFlow::Notice::Canceled);
    flow.beginPin();
    CHECK(flow.pinEnded(Host::PairingOutcome::Refused) == RegisterFlow::Notice::Failed);
    flow.beginPin();
    CHECK(flow.pinEnded(Host::PairingOutcome::Failed) == RegisterFlow::Notice::Failed);
    CHECK(!flow.registering());
}

TEST(register_flow_pin_refusals_map_each_error)
{
    RegisterFlow flow;
    flow.beginPin();
    CHECK(!flow.pinRefused(akira::consoles::HOST_REGISTER_OK).has_value());
    CHECK(flow.registering());

    CHECK(flow.pinRefused(akira::consoles::HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID) == RegisterFlow::Refusal::AccountId);
    CHECK(!flow.registering());
    CHECK(flow.pinRefused(akira::consoles::HOST_REGISTER_ERROR_SETTING_PSNONLINEID) == RegisterFlow::Refusal::OnlineId);
    CHECK(flow.pinRefused(akira::consoles::HOST_REGISTER_ERROR_UNDEFINED_TARGET) == RegisterFlow::Refusal::ConsoleType);
    CHECK(flow.pinRefused(akira::consoles::HOST_REGISTER_ERROR_HOLEPUNCH) == RegisterFlow::Refusal::Failed);
}

TEST(register_flow_internet_refuses_a_second_start)
{
    RegisterFlow flow;
    CHECK(flow.beginInternet());
    CHECK(flow.registering());
    CHECK(!flow.beginInternet());
    CHECK(flow.internetStarted(akira::consoles::HOST_REGISTER_OK));
    CHECK(flow.registering());
    CHECK(!flow.internetStarted(akira::consoles::HOST_REGISTER_ERROR_HOLEPUNCH));
    CHECK(!flow.registering());
}

TEST(register_flow_internet_falls_back_to_pin_unless_canceled)
{
    RegisterFlow flow;
    REQUIRE(flow.beginInternet());
    CHECK(flow.internetEnded(Host::PairingOutcome::Paired) == RegisterFlow::InternetEnd::Success);
    CHECK(!flow.registering());

    REQUIRE(flow.beginInternet());
    CHECK(flow.internetEnded(Host::PairingOutcome::Failed) == RegisterFlow::InternetEnd::FallBackToPin);

    REQUIRE(flow.beginInternet());
    CHECK(flow.internetEnded(Host::PairingOutcome::Canceled) == RegisterFlow::InternetEnd::Quiet);

    REQUIRE(flow.beginInternet());
    flow.userCanceled();
    flow.stopped();
    CHECK(!flow.registering());
    CHECK(flow.internetEnded(Host::PairingOutcome::Refused) == RegisterFlow::InternetEnd::Quiet);

    REQUIRE(flow.beginInternet());
    CHECK(flow.internetEnded(Host::PairingOutcome::Refused) == RegisterFlow::InternetEnd::FallBackToPin);
}
