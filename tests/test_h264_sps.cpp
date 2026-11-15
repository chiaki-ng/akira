#include "test_util.hpp"

#include "stream/h264_sps.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

using akira::stream::H264Sps;

static std::vector<std::uint8_t> fromHex(std::string_view hex)
{
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        bytes.push_back(static_cast<std::uint8_t>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16)));
    return bytes;
}

static void checkRewrite(std::string_view hex, std::uint32_t level, std::uint32_t references, std::uint32_t limit = 16)
{
    const std::uint32_t expected = std::max<std::uint32_t>(std::min(references, limit), 1);
    const auto original = H264Sps::parse(fromHex(hex));
    CHECK(original.has_value());
    if (!original)
        return;
    CHECK_EQ(original->levelIdc(), level);
    CHECK_EQ(original->maxNumRefFrames(), references);
    const auto rewritten = H264Sps::parse(original->forDecoder(limit));
    CHECK(rewritten.has_value());
    if (!rewritten)
        return;
    CHECK_EQ(rewritten->levelIdc(), level);
    CHECK_EQ(rewritten->maxNumRefFrames(), expected);
    CHECK(rewritten->maxDecFrameBuffering() == expected);
    CHECK(rewritten->forDecoder(limit) == original->forDecoder(limit));
}

TEST(h264_sps_adds_restriction_without_vui)
{
    checkRewrite("2764001fac56805005b9", 31, 1);
}

TEST(h264_sps_adds_restriction_to_vui)
{
    checkRewrite("2764001fac56805005ba6804", 31, 1);
}

TEST(h264_sps_keeps_existing_vui_with_emulation_prevention)
{
    checkRewrite("67640020acb200a00b760220000003002000000f01e30649", 32, 3);
}

TEST(h264_sps_caps_ps5_references)
{
    checkRewrite("674d4028918a014016ec05a808080a00000fa4000753011343c7c540", 40, 9, 5);
}

TEST(h264_sps_rejects_other_nal_units)
{
    CHECK(!H264Sps::parse(fromHex("68ebe3cb22c0")).has_value());
    CHECK(!H264Sps::parse(fromHex("6764")).has_value());
}
