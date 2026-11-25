#include "test_util.hpp"

#include "log/log.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::string contents(std::FILE* file)
{
    std::fflush(file);
    std::rewind(file);
    std::string text;
    char buffer[256];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof buffer, file)) > 0)
        text.append(buffer, read);
    return text;
}

static std::string slurp(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

static std::filesystem::path scratch(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

TEST(log_line_matches_the_borealis_format)
{
    std::tm local{};
    local.tm_year = 126;
    local.tm_mon = 10;
    local.tm_mday = 19;
    local.tm_hour = 7;
    local.tm_min = 5;
    local.tm_sec = 9;
    local.tm_isdst = -1;
    const auto time = akira::log::Clock::from_time_t(std::mktime(&local)) + std::chrono::milliseconds(42);

    CHECK_EQ(akira::log::Log::formatLine(time, akira::log::Level::info, "hello 3"),
             std::string("07:05:09.042\033[0;34m[INFO]\033[0m hello 3\n"));
    CHECK_EQ(akira::log::Log::formatLine(time, akira::log::Level::error, "x"),
             std::string("07:05:09.042\033[0;31m[ERROR]\033[0m x\n"));
    CHECK_EQ(akira::log::Log::formatLine(time, akira::log::Level::warning, "x"),
             std::string("07:05:09.042\033[0;33m[WARNING]\033[0m x\n"));
    CHECK_EQ(akira::log::Log::formatLine(time, akira::log::Level::debug, "x"),
             std::string("07:05:09.042\033[0;32m[DEBUG]\033[0m x\n"));
    CHECK_EQ(akira::log::Log::formatLine(time, akira::log::Level::verbose, "x"),
             std::string("07:05:09.042\033[0;37m[VERBOSE]\033[0m x\n"));
}

TEST(log_level_filters_and_silences)
{
    akira::log::Log log{std::tmpfile()};
    CHECK(log.enabled(akira::log::Level::error));
    CHECK(log.enabled(akira::log::Level::info));
    CHECK(!log.enabled(akira::log::Level::debug));

    log.setLevel(akira::log::Level::verbose);
    CHECK(log.enabled(akira::log::Level::verbose));

    log.setLevel(akira::log::Level::warning);
    CHECK(log.enabled(akira::log::Level::warning));
    CHECK(!log.enabled(akira::log::Level::info));

    log.silence();
    CHECK(!log.enabled(akira::log::Level::error));
}

TEST(log_writes_sync_and_async_lines)
{
    std::FILE* console = std::tmpfile();
    REQUIRE(console != nullptr);
    {
        akira::log::Log log{console};
        log.write(akira::log::Level::info, "first");
        log.setAsync(true);
        log.write(akira::log::Level::error, "second");
        CHECK(contents(console).find("second") == std::string::npos);
        log.flush();
    }
    const std::string text = contents(console);
    CHECK(text.find("[INFO]\033[0m first\n") != std::string::npos);
    CHECK(text.find("[ERROR]\033[0m second\n") != std::string::npos);
    std::fclose(console);
}

TEST(log_redirect_restores_the_previous_target)
{
    std::FILE* console = std::tmpfile();
    REQUIRE(console != nullptr);
    const auto outer = scratch("akira_log_outer.log");
    const auto inner = scratch("akira_log_inner.log");
    {
        akira::log::Log log{console};
        log.setAsync(true);
        log.write(akira::log::Level::info, "before");
        {
            auto outerRedirect = log.redirect(outer.string());
            REQUIRE(outerRedirect.has_value());
            log.write(akira::log::Level::info, "outer");
            {
                auto innerRedirect = log.redirect(inner.string(), akira::log::Buffering::line);
                REQUIRE(innerRedirect.has_value());
                log.write(akira::log::Level::info, "inner");
            }
            log.write(akira::log::Level::info, "outer again");
        }
        log.write(akira::log::Level::info, "after");
        log.flush();
        CHECK(!log.redirect("/nonexistent-dir/akira.log").has_value());
    }

    const std::string consoleText = contents(console);
    const std::string outerText = slurp(outer);
    const std::string innerText = slurp(inner);
    CHECK(consoleText.find(" before\n") != std::string::npos);
    CHECK(consoleText.find(" after\n") != std::string::npos);
    CHECK(consoleText.find("outer") == std::string::npos);
    CHECK(outerText.find(" outer\n") != std::string::npos);
    CHECK(outerText.find(" outer again\n") != std::string::npos);
    CHECK(outerText.find("inner") == std::string::npos);
    CHECK(innerText.find(" inner\n") != std::string::npos);
    CHECK(innerText.find("outer") == std::string::npos);

    std::fclose(console);
    std::filesystem::remove(outer);
    std::filesystem::remove(inner);
}

TEST(log_redirect_released_out_of_order)
{
    std::FILE* console = std::tmpfile();
    REQUIRE(console != nullptr);
    const auto first = scratch("akira_log_first.log");
    const auto second = scratch("akira_log_second.log");
    {
        akira::log::Log log{console};
        auto firstRedirect = log.redirect(first.string());
        auto secondRedirect = log.redirect(second.string());
        REQUIRE(firstRedirect.has_value() && secondRedirect.has_value());
        firstRedirect.reset();
        log.write(akira::log::Level::info, "still second");
        secondRedirect.reset();
        log.write(akira::log::Level::info, "back on console");
    }
    CHECK(slurp(second).find(" still second\n") != std::string::npos);
    CHECK(contents(console).find(" back on console\n") != std::string::npos);
    std::fclose(console);
    std::filesystem::remove(first);
    std::filesystem::remove(second);
}

TEST(log_subscription_unsubscribes_on_destruction)
{
    akira::log::Log log{std::tmpfile()};
    std::vector<std::string> seen;
    std::vector<akira::log::Level> levels;
    {
        auto subscription = log.subscribe([&](const akira::log::Entry& entry) {
            seen.emplace_back(entry.message);
            levels.push_back(entry.level);
        });
        log.write(akira::log::Level::warning, "one");
        auto moved = std::move(subscription);
        log.write(akira::log::Level::info, "two");
    }
    log.write(akira::log::Level::info, "three");

    REQUIRE(seen.size() == 2);
    CHECK_EQ(seen[0], std::string("one"));
    CHECK_EQ(seen[1], std::string("two"));
    CHECK(levels[0] == akira::log::Level::warning);
}
