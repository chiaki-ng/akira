#include "test_util.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace tests {

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> cases;
    return cases;
}

int failures = 0;

} // namespace tests

int main()
{
    int failed = 0;
    int passed = 0;
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    const char* only = std::getenv("TEST_FILTER");

    for (const tests::TestCase& test : tests::registry())
    {
        if (only && std::string_view(test.name).find(only) == std::string_view::npos)
            continue;
        tests::failures = 0;
        test.body();

        if (tests::failures > 0)
        {
            std::printf("FAIL  %s (%d check(s))\n", test.name, tests::failures);
            failed++;
        }
        else
        {
            std::printf("ok    %s\n", test.name);
            passed++;
        }
    }

    std::printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
