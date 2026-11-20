#include "test_util.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/build_info.hpp"
#include "app/qr_matrix.hpp"
#include "log/log_excerpt.hpp"
#include "log/log_folder.hpp"

using akira::app::BuildInfo;
using akira::app::QrMatrix;
using akira::log::LogExcerpt;
using akira::log::LogFolder;

TEST(qr_matrix_encodes_a_sign_in_link)
{
    const auto matrix = QrMatrix::encode("https://my.account.sony.com/central/signin/qr?code=482113");
    REQUIRE(matrix.has_value());
    CHECK(matrix->size() >= 21);
    CHECK((matrix->size() - 17) % 4 == 0);
    CHECK(matrix->dark(0, 0));
    CHECK(matrix->dark(6, 6));
    CHECK(!matrix->dark(7, 7));
    CHECK(!matrix->dark(-1, 0));
    CHECK(!QrMatrix::encode(std::string(4000, 'x')).has_value());
}

TEST(build_info_groups_lines_under_headings)
{
    const BuildInfo info = BuildInfo::parse("akira 2.4 (dev)\nCommit: b02a8e5\n\n=== Core ===\nlibchiaki-core: 2c84cee\r\n\n=== Toolchain ===\nGCC: 16.1.0\n");
    REQUIRE(info.sections().size() == 3);
    CHECK(info.sections()[0].title.empty());
    CHECK(info.sections()[0].lines == std::vector<std::string>({ "akira 2.4 (dev)", "Commit: b02a8e5" }));
    CHECK(info.sections()[1].title == "Core");
    CHECK(info.sections()[1].lines == std::vector<std::string>({ "libchiaki-core: 2c84cee" }));
    CHECK(info.sections()[2].title == "Toolchain");
    CHECK(!BuildInfo::read("/nonexistent/build_info.txt").has_value());
}

TEST(log_excerpt_strips_colour_and_groups_levels)
{
    CHECK(LogExcerpt::sanitize("\x1b[31mred\x1b[0m\r\nok\x01") == "red\nok");
    CHECK(LogExcerpt::levelOf("12:00 [WARN] slow") == LogExcerpt::Level::Warning);
    CHECK(!LogExcerpt::levelOf("plain").has_value());
    const std::vector<LogExcerpt::Block> blocks = LogExcerpt::blocksOf("[INFO] a\n[ERROR] b\n  at c\n[INFO] d\n");
    REQUIRE(blocks.size() == 3);
    CHECK(blocks[1].level == LogExcerpt::Level::Error);
    CHECK(blocks[1].text == "[ERROR] b\n  at c");
}

TEST(log_excerpt_keeps_the_newest_lines)
{
    std::string raw = "partial line\n";
    for (std::size_t line = 0; line < LogExcerpt::maxLines + 10; ++line)
        raw += "[INFO] line " + std::to_string(line) + "\n";
    const LogExcerpt excerpt = LogExcerpt::of(raw, 1 << 20, true);
    CHECK(excerpt.truncated());
    CHECK(excerpt.lines() == LogExcerpt::maxLines);
    REQUIRE(!excerpt.blocks().empty());
    CHECK(excerpt.blocks().front().text.find("partial") == std::string::npos);
    CHECK(excerpt.totalBytes() == (1u << 20));
}

TEST(log_folder_lists_folders_first_and_newest_files_next)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "akira-log-folder";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "sessions");
    std::ofstream(root / "older.log") << "a";
    std::ofstream(root / "newer.log") << "bb";
    std::filesystem::last_write_time(root / "older.log", std::filesystem::file_time_type::clock::now() - std::chrono::hours(2));
    const auto listed = LogFolder::list(root);
    REQUIRE(listed.has_value());
    REQUIRE(listed->size() == 3);
    CHECK((*listed)[0].name == "sessions");
    CHECK((*listed)[1].name == "newer.log");
    CHECK((*listed)[2].name == "older.log");
    CHECK(!LogFolder::list(root / "missing").has_value());
    CHECK(LogFolder::sizeText(512) == "512 B");
    CHECK(LogFolder::sizeText(4096) == "4 KB");
    CHECK(LogFolder::sizeText(3 * 1024 * 1024) == "3.0 MB");
    std::filesystem::remove_all(root);
}
