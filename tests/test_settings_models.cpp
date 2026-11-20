#include "test_util.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>

#include "play/button_board.hpp"
#include "play/combo_capture.hpp"
#include "play/controller_settings.hpp"
#include "play/picture_profile.hpp"
#include "play/remap_target.hpp"
#include "play/stream_settings.hpp"
#include "play/switch_button.hpp"
#include "settings/config_listing.hpp"
#include "settings/settings_store.hpp"

using akira::play::bits;
using akira::play::ButtonBoard;
using akira::play::ComboCapture;
using akira::play::SwitchButton;
using akira::settings::ConfigListing;

namespace {

auto scratch(const char* name) -> std::filesystem::path
{
    return std::filesystem::temp_directory_path()
        / std::format("akira-{}-{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
}

}

TEST(combo_capture_waits_for_release_then_takes_the_peak)
{
    ComboCapture capture;
    CHECK(!capture.feed(bits(SwitchButton::A)).has_value());
    CHECK(!capture.feed(0).has_value());
    CHECK(!capture.feed(bits(SwitchButton::ZL)).has_value());
    CHECK(!capture.feed(bits(SwitchButton::ZL) | bits(SwitchButton::B)).has_value());
    std::optional<std::vector<std::uint64_t>> combo;
    for (int frame = 0; frame < ComboCapture::graceFrames && !combo; ++frame)
        combo = capture.feed(0);
    REQUIRE(combo.has_value());
    CHECK_EQ(combo->size(), std::size_t{ 2 });
    CHECK_EQ(combo->front(), bits(SwitchButton::B));
    CHECK_EQ(combo->back(), bits(SwitchButton::ZL));
}

TEST(button_board_assigns_resets_and_flags_conflicts)
{
    const std::filesystem::path root = scratch("board");
    akira::settings::SettingsStore store(root);
    akira::play::ControllerSettings controllers(store);
    store.finishLoading();
    ButtonBoard board(controllers);
    const std::uint32_t cross = akira::play::remapKey(akira::play::RemapTarget::Cross);
    const std::uint32_t moon = akira::play::remapKey(akira::play::RemapTarget::Moon);
    CHECK(board.isDefault(cross));
    board.assign(cross, board.mapping(moon));
    CHECK(!board.isDefault(cross));
    CHECK(board.conflicts(cross));
    board.reset(cross);
    CHECK(board.isDefault(cross));
    CHECK(!board.conflicts(cross));
    std::filesystem::remove_all(root);
}

TEST(config_listing_masks_secrets_and_colours_tokens)
{
    CHECK_EQ(ConfigListing::censored("refresh_token = 'abcdefghijk'"), std::string("refresh_token = '****ghijk'"));
    CHECK_EQ(ConfigListing::censored("name = 'Living room'"), std::string("name = 'Living room'"));
    const ConfigListing::Line header = ConfigListing::parse("[stream.local]");
    CHECK(header.header);
    CHECK_EQ(header.pieces.size(), std::size_t{ 5 });
    const ConfigListing::Line pair = ConfigListing::parse("fps = 60");
    REQUIRE(pair.pieces.size() == 3);
    CHECK(pair.pieces[2].token == ConfigListing::Token::Number);
}

TEST(vpn_picture_profile_keeps_its_fixed_bitrate_range)
{
    const std::filesystem::path root = scratch("vpn");
    akira::settings::SettingsStore store(root);
    akira::play::StreamSettings stream(store, false);
    store.finishLoading();
    akira::play::PictureProfile vpn(stream, akira::play::StreamProfile::Vpn, false);
    CHECK_EQ(vpn.minBitrate(), akira::play::PictureProfile::vpnMinimumBitrate);
    CHECK_EQ(vpn.maxBitrate(), akira::play::PictureProfile::vpnMaximumBitrate);
    vpn.chooseResolution(akira::play::VideoResolution::P360);
    vpn.chooseResolution(akira::play::VideoResolution::P1080);
    CHECK_EQ(vpn.bitrate(), akira::play::PictureProfile::vpnDefaultBitrate);
    std::filesystem::remove_all(root);
}
