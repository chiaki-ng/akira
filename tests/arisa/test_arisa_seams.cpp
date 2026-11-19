#include "test_util.hpp"

#include <arisa/components/text.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/runtime/application.hpp>

#include <atomic>
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
