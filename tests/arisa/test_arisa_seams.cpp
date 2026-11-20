#include "test_util.hpp"

#include <arisa/components/text.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/runtime/application.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <thread>

#include "app/arisa/dispatcher.hpp"
#include "app/arisa/string_catalog.hpp"

namespace {

using akira::app::arisa::Dispatcher;
using akira::app::arisa::StringCatalog;

auto headlessDispatcher(arisa::runtime::Application& application) -> Dispatcher
{
    return Dispatcher(application.root().dispatcher(), application.root().scheduler(),
        application.executors().background.get());
}

}

TEST(arisa_dispatcher_runs_a_post_on_the_next_frame)
{
    arisa::provider::headless::HeadlessProviderFactory providers;
    auto application = arisa::runtime::Application::create(providers, arisa::components::Text("akira"));
    REQUIRE(application.has_value());
    Dispatcher dispatcher = headlessDispatcher(*application);
    int ran = 0;
    dispatcher.post([&ran] { ++ran; });
    CHECK(ran == 0);
    REQUIRE(application->frame().has_value());
    CHECK(ran == 1);
}

TEST(arisa_dispatcher_runs_worker_jobs_off_the_root)
{
    using namespace std::chrono_literals;
    arisa::provider::headless::HeadlessProviderFactory providers;
    auto application = arisa::runtime::Application::create(providers, arisa::components::Text("akira"));
    REQUIRE(application.has_value());
    Dispatcher dispatcher = headlessDispatcher(*application);
    std::atomic<bool> done{false};
    dispatcher.runOnWorker([&done] { done.store(true); });
    for (int attempt = 0; attempt < 400 && !done.load(); ++attempt)
    {
        static_cast<void>(application->frame());
        std::this_thread::sleep_for(5ms);
    }
    CHECK(done.load());
}

TEST(arisa_strings_read_akira_catalog_and_fall_back_to_the_key)
{
    const arisa::provider::standard::DirectoryAssets assets(AKIRA_RESOURCES_DIR);
    const StringCatalog strings(assets, "en");
    CHECK(strings.loaded());
    CHECK(strings.text("akira/common/cancel") == "Cancel");
    CHECK(strings.text("akira/does/not/exist") == "akira/does/not/exist");
}

TEST(arisa_strings_pick_the_plural_form_for_the_count)
{
    const arisa::provider::standard::DirectoryAssets assets(AKIRA_RESOURCES_DIR);
    const StringCatalog english(assets, "en");
    CHECK(english.counted("akira/hosts/recent_weeks", 1) == "1 week ago");
    CHECK(english.counted("akira/hosts/recent_weeks", 2) == "2 weeks ago");
    CHECK(english.counted("akira/cloud/rating_count", std::int64_t{ 1 }) == "1 rating");
    CHECK(english.counted("akira/cloud/rating_count", std::int64_t{ 1200 }) == "1200 ratings");
    CHECK(english.counted("akira/cloud/count_all", std::size_t{ 1 }) == "1 game");
    CHECK(english.counted("akira/connection/step_of", 3, 6) == "Step 3 of 6");
    const StringCatalog chinese(assets, "zh-Hans");
    CHECK(chinese.counted("akira/cloud/count_all", std::size_t{ 1 }) == "1 款游戏");
}
