#include "test_util.hpp"

#include "util/semver.hpp"

using akira::semver::compare;
using akira::semver::isValid;

TEST(semver_equal)
{
    CHECK_EQ(compare("0.5.3", "0.5.3"), 0);
    CHECK_EQ(compare("v0.5.3", "0.5.3"), 0);
    CHECK_EQ(compare("1.0.0-rc.1", "1.0.0-rc.1"), 0);
}

TEST(semver_core_ordering)
{
    CHECK(compare("0.5.4", "0.5.3") > 0);
    CHECK(compare("0.5.3", "0.5.4") < 0);
    CHECK(compare("0.6.0", "0.5.99") > 0);
    CHECK(compare("1.0.0", "0.99.99") > 0);
    CHECK(compare("0.10.0", "0.9.0") > 0);
}

TEST(semver_prerelease_precedence)
{
    CHECK(compare("1.0.0", "1.0.0-rc.1") > 0);
    CHECK(compare("1.0.0-rc.1", "1.0.0") < 0);
    CHECK(compare("1.0.0-rc.2", "1.0.0-rc.1") > 0);
    CHECK(compare("1.0.0-rc.10", "1.0.0-rc.2") > 0);
    CHECK(compare("1.0.0-rc.1", "1.0.0-beta.9") > 0);
    CHECK(compare("1.0.0-alpha", "1.0.0-alpha.1") < 0);
}

TEST(semver_build_metadata_ignored)
{
    CHECK_EQ(compare("0.5.3+abc123", "0.5.3"), 0);
    CHECK_EQ(compare("0.5.3+abc123.dirty", "0.5.3+def456"), 0);
    CHECK(compare("0.5.4+aaa", "0.5.3+zzz") > 0);
}

TEST(semver_validity)
{
    CHECK(isValid("0.5.3"));
    CHECK(isValid("v1.2.3-rc.1"));
    CHECK(isValid("1.2.3+meta"));
    CHECK(!isValid(""));
    CHECK(!isValid("1.2"));
    CHECK(!isValid("1.2.x"));
    CHECK(!isValid("nightly-2026-07-26"));
    CHECK(!isValid("1.2.3-"));
}
