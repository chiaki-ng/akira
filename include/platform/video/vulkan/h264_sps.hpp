#ifndef AKIRA_STREAM_H264_SPS_HPP
#define AKIRA_STREAM_H264_SPS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace akira::stream {

class H264Sps {
public:
    static std::optional<H264Sps> parse(std::span<const std::uint8_t> nal);

    std::vector<std::uint8_t> forDecoder(std::uint32_t maxReferences) const;
    std::uint32_t maxNumRefFrames() const { return m_max_num_ref_frames; }
    std::uint32_t levelIdc() const { return m_level_idc; }
    std::optional<std::uint32_t> maxDecFrameBuffering() const;

private:
    struct Restriction {
        std::uint32_t motion_vectors_over_pic_boundaries = 1;
        std::uint32_t max_bytes_per_pic_denom = 2;
        std::uint32_t max_bits_per_mb_denom = 1;
        std::uint32_t log2_max_mv_length_horizontal = 16;
        std::uint32_t log2_max_mv_length_vertical = 16;
        std::uint32_t max_num_reorder_frames = 0;
        std::uint32_t max_dec_frame_buffering = 0;
    };

    class BitReader;
    class BitWriter;

    H264Sps() = default;

    static std::vector<std::uint8_t> unescape(std::span<const std::uint8_t> payload);
    static std::vector<std::uint8_t> escape(std::span<const std::uint8_t> rbsp);
    static bool skipScalingList(BitReader& reader, std::size_t size);
    static bool skipHrd(BitReader& reader);
    static std::optional<Restriction> readRestriction(BitReader& reader);

    std::uint8_t m_header = 0;
    std::uint32_t m_level_idc = 0;
    std::uint32_t m_max_num_ref_frames = 0;
    std::vector<std::uint8_t> m_rbsp;
    std::size_t m_references_begin = 0;
    std::size_t m_references_end = 0;
    std::size_t m_cut = 0;
    bool m_has_vui = false;
    std::optional<Restriction> m_restriction;
};

}

#endif
