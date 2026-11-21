#include "test_util.hpp"

#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <vector>

#include "app/screenshot.hpp"
#include "play/screenshot_chord.hpp"
#include "play/switch_button.hpp"

using akira::app::Screenshot;
using akira::play::bits;
using akira::play::ScreenshotChord;
using akira::play::SwitchButton;

namespace {

auto bigEndian(const std::vector<std::byte>& bytes, std::size_t at) -> std::uint32_t
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index)
        value = (value << 8) | std::to_integer<std::uint32_t>(bytes[at + index]);
    return value;
}

}

TEST(screenshot_file_name_is_the_local_time)
{
    std::tm local{};
    local.tm_year = 2026 - 1900;
    local.tm_mon = 10;
    local.tm_mday = 20;
    local.tm_hour = 9;
    local.tm_min = 5;
    local.tm_sec = 7;
    CHECK_EQ(Screenshot::fileName(local), std::string("2026-11-20_09-05-07.png"));
}

TEST(screenshot_rejects_a_frame_of_the_wrong_size)
{
    CHECK(!Screenshot::create(4, 4, std::vector<std::byte>(10)).has_value());
    CHECK(!Screenshot::create(0, 4, {}).has_value());
}

TEST(screenshot_encodes_a_png_of_the_frame_size)
{
    std::vector<std::byte> rgba(std::size_t{ 32 } * 18 * Screenshot::channels, std::byte{ 0x40 });
    const std::optional<Screenshot> shot = Screenshot::create(32, 18, std::move(rgba));
    REQUIRE(shot.has_value());
    const std::optional<std::vector<std::byte>> png = shot->png();
    REQUIRE(png.has_value() && png->size() > 33);
    const std::vector<std::byte> signature{ std::byte{ 0x89 }, std::byte{ 'P' }, std::byte{ 'N' }, std::byte{ 'G' } };
    CHECK(std::equal(signature.begin(), signature.end(), png->begin()));
    CHECK_EQ(bigEndian(*png, 16), std::uint32_t{ 32 });
    CHECK_EQ(bigEndian(*png, 20), std::uint32_t{ 18 });
}

TEST(screenshot_saves_into_a_new_folder)
{
    const std::filesystem::path folder = std::filesystem::temp_directory_path()
        / std::format("akira-shots-{}", std::chrono::steady_clock::now().time_since_epoch().count()) / "screenshots";
    const std::optional<Screenshot> shot = Screenshot::create(2, 2, std::vector<std::byte>(16, std::byte{ 0xff }));
    REQUIRE(shot.has_value());
    const auto saved = shot->save(folder, "2026-11-20_09-05-07.png");
    REQUIRE(saved.has_value());
    CHECK(std::filesystem::file_size(*saved) > 0);
    std::filesystem::remove_all(folder.parent_path());
}

TEST(screenshot_chord_fires_once_per_press_and_swallows_the_stick)
{
    ScreenshotChord chord;
    const std::uint64_t triggers = bits(SwitchButton::ZL) | bits(SwitchButton::ZR);
    const std::uint64_t stick = bits(SwitchButton::StickR);
    CHECK(!chord.update(triggers));
    CHECK(!chord.swallowing());
    CHECK(chord.update(triggers | stick));
    CHECK(chord.swallowing());
    CHECK_EQ(chord.filtered(triggers | stick), triggers);
    CHECK(!chord.update(triggers | stick));
    CHECK(!chord.update(stick));
    CHECK(chord.swallowing());
    CHECK(!chord.update(triggers));
    CHECK(!chord.swallowing());
    CHECK(chord.update(triggers | stick));
}

TEST(screenshot_chord_leaves_a_plain_stick_click_alone)
{
    ScreenshotChord chord;
    CHECK(!chord.update(bits(SwitchButton::StickR)));
    CHECK(!chord.swallowing());
    CHECK_EQ(chord.filtered(bits(SwitchButton::StickR)), bits(SwitchButton::StickR));
}
