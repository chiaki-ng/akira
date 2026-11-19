#include "test_util.hpp"

#include "fake_strings.hpp"
#include "fake_ui_dispatcher.hpp"

#include <chrono>
#include <string>
#include <vector>

using tests::FakeStrings;
using tests::FakeUiDispatcher;

TEST(strings_format_fills_placeholders_like_borealis)
{
    const FakeStrings strings({{"akira/connection/connecting_to", "Connecting to {}"},
                               {"akira/benchmark/pmull_result", "{:.0f} ops/s | {:.2f} MB/s"}});
    CHECK_EQ(strings.format("akira/connection/connecting_to", std::string("Living Room")),
             std::string("Connecting to Living Room"));
    CHECK_EQ(strings.format("akira/benchmark/pmull_result", 1234.5, 2.0), std::string("1234 ops/s | 2.00 MB/s"));
}

TEST(strings_format_without_arguments_unescapes_like_borealis)
{
    const FakeStrings strings({{"braces", "{{shown}}"}});
    CHECK_EQ(strings.text("braces"), std::string("{{shown}}"));
    CHECK_EQ(strings.format("braces"), std::string("{shown}"));
}

TEST(strings_fall_back_to_the_key)
{
    const FakeStrings strings({{"broken", "missing {} argument {}"}});
    CHECK_EQ(strings.text("akira/absent"), std::string("akira/absent"));
    CHECK_EQ(strings.format("broken", 1), std::string("broken"));
}

TEST(fake_dispatcher_runs_posts_in_order)
{
    FakeUiDispatcher dispatcher;
    std::vector<int> ran;
    dispatcher.post([&ran] { ran.push_back(1); });
    dispatcher.postAfter(std::chrono::milliseconds(50), [&ran] { ran.push_back(2); });
    dispatcher.runOnWorker([&ran] { ran.push_back(3); });
    CHECK(ran == std::vector<int>({1, 2, 3}));
}

TEST(fake_dispatcher_stops_a_dropped_repeat)
{
    FakeUiDispatcher dispatcher;
    int kept = 0;
    int dropped = 0;
    auto keep = dispatcher.repeatEvery(std::chrono::minutes(5), [&kept] { kept++; });
    {
        auto drop = dispatcher.repeatEvery(std::chrono::minutes(5), [&dropped] { dropped++; });
        dispatcher.tickRepeats();
    }
    dispatcher.tickRepeats();
    CHECK_EQ(kept, 2);
    CHECK_EQ(dropped, 1);
}
