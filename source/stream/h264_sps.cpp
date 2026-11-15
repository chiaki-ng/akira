#include "stream/h264_sps.hpp"

#include <algorithm>
#include <array>
#include <bit>

namespace akira::stream {

class H264Sps::BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> bytes) : m_bytes(bytes) {}

    std::optional<std::uint32_t> bits(unsigned count)
    {
        if (count > 32 || m_position + count > m_bytes.size() * 8)
            return std::nullopt;
        std::uint32_t value = 0;
        for (unsigned i = 0; i < count; ++i, ++m_position)
            value = (value << 1) | ((m_bytes[m_position >> 3] >> (7 - (m_position & 7))) & 1U);
        return value;
    }

    std::optional<std::uint32_t> ue()
    {
        unsigned zeros = 0;
        for (;;)
        {
            const auto bit = bits(1);
            if (!bit)
                return std::nullopt;
            if (*bit)
                break;
            if (++zeros > 31)
                return std::nullopt;
        }
        const auto rest = bits(zeros);
        if (!rest)
            return std::nullopt;
        return static_cast<std::uint32_t>((std::uint64_t{1} << zeros) - 1 + *rest);
    }

    std::optional<std::int32_t> se()
    {
        const auto code = ue();
        if (!code)
            return std::nullopt;
        return (*code & 1U) ? static_cast<std::int32_t>((*code + 1) / 2) : -static_cast<std::int32_t>(*code / 2);
    }

    bool skip(unsigned count) { return bits(count).has_value(); }
    bool skipUe() { return ue().has_value(); }
    bool skipSe() { return se().has_value(); }
    std::size_t position() const { return m_position; }

private:
    std::span<const std::uint8_t> m_bytes;
    std::size_t m_position = 0;
};

class H264Sps::BitWriter {
public:
    void put(std::uint64_t value, unsigned count)
    {
        for (unsigned i = count; i-- > 0;)
            bit(static_cast<unsigned>((value >> i) & 1U));
    }

    void ue(std::uint32_t value)
    {
        const std::uint64_t coded = std::uint64_t{value} + 1;
        const auto length = static_cast<unsigned>(std::bit_width(coded));
        put(0, length - 1);
        put(coded, length);
    }

    void trail()
    {
        put(1, 1);
        while (m_bits & 7U)
            put(0, 1);
    }

    const std::vector<std::uint8_t>& bytes() const { return m_bytes; }

private:
    void bit(unsigned value)
    {
        if ((m_bits & 7U) == 0)
            m_bytes.push_back(0);
        if (value)
            m_bytes.back() |= static_cast<std::uint8_t>(0x80U >> (m_bits & 7U));
        ++m_bits;
    }

    std::vector<std::uint8_t> m_bytes;
    std::size_t m_bits = 0;
};

std::optional<H264Sps> H264Sps::parse(std::span<const std::uint8_t> nal)
{
    static constexpr std::array<std::uint32_t, 13> HighProfiles{100, 110, 122, 244, 44, 83, 86, 118, 128, 138, 139, 134, 135};
    if (nal.size() < 4 || (nal[0] & 0x1F) != 7)
        return std::nullopt;
    H264Sps sps;
    sps.m_header = nal[0];
    sps.m_rbsp = unescape(nal.subspan(1));
    BitReader reader(sps.m_rbsp);

    const auto profile = reader.bits(8);
    if (!profile || !reader.skip(8))
        return std::nullopt;
    const auto level = reader.bits(8);
    if (!level || !reader.skipUe())
        return std::nullopt;
    sps.m_level_idc = *level;

    if (std::ranges::contains(HighProfiles, *profile))
    {
        const auto chroma = reader.ue();
        if (!chroma || (*chroma == 3 && !reader.skip(1)))
            return std::nullopt;
        if (!reader.skipUe() || !reader.skipUe() || !reader.skip(1))
            return std::nullopt;
        const auto matrix = reader.bits(1);
        if (!matrix)
            return std::nullopt;
        const std::size_t lists = *matrix ? (*chroma == 3 ? 12 : 8) : 0;
        for (std::size_t i = 0; i < lists; ++i)
        {
            const auto present = reader.bits(1);
            if (!present || (*present && !skipScalingList(reader, i < 6 ? 16 : 64)))
                return std::nullopt;
        }
    }

    if (!reader.skipUe())
        return std::nullopt;
    const auto pocType = reader.ue();
    if (!pocType)
        return std::nullopt;
    if (*pocType == 0 && !reader.skipUe())
        return std::nullopt;
    if (*pocType == 1)
    {
        if (!reader.skip(1) || !reader.skipSe() || !reader.skipSe())
            return std::nullopt;
        const auto cycle = reader.ue();
        if (!cycle || *cycle > 255)
            return std::nullopt;
        for (std::uint32_t i = 0; i < *cycle; ++i)
        {
            if (!reader.skipSe())
                return std::nullopt;
        }
    }

    sps.m_references_begin = reader.position();
    const auto references = reader.ue();
    sps.m_references_end = reader.position();
    if (!references || !reader.skip(1) || !reader.skipUe() || !reader.skipUe())
        return std::nullopt;
    sps.m_max_num_ref_frames = *references;
    const auto frameMbsOnly = reader.bits(1);
    if (!frameMbsOnly || (!*frameMbsOnly && !reader.skip(1)) || !reader.skip(1))
        return std::nullopt;
    const auto cropping = reader.bits(1);
    if (!cropping)
        return std::nullopt;
    if (*cropping && (!reader.skipUe() || !reader.skipUe() || !reader.skipUe() || !reader.skipUe()))
        return std::nullopt;

    sps.m_cut = reader.position();
    const auto vui = reader.bits(1);
    if (!vui)
        return std::nullopt;
    sps.m_has_vui = *vui != 0;
    if (!sps.m_has_vui)
        return sps;

    const auto aspect = reader.bits(1);
    if (!aspect)
        return std::nullopt;
    if (*aspect)
    {
        const auto idc = reader.bits(8);
        if (!idc || (*idc == 255 && !reader.skip(32)))
            return std::nullopt;
    }
    const auto overscan = reader.bits(1);
    if (!overscan || (*overscan && !reader.skip(1)))
        return std::nullopt;
    const auto signal = reader.bits(1);
    if (!signal)
        return std::nullopt;
    if (*signal)
    {
        if (!reader.skip(4))
            return std::nullopt;
        const auto description = reader.bits(1);
        if (!description || (*description && !reader.skip(24)))
            return std::nullopt;
    }
    const auto chromaLocation = reader.bits(1);
    if (!chromaLocation || (*chromaLocation && (!reader.skipUe() || !reader.skipUe())))
        return std::nullopt;
    const auto timing = reader.bits(1);
    if (!timing || (*timing && (!reader.skip(32) || !reader.skip(32) || !reader.skip(1))))
        return std::nullopt;
    const auto nalHrd = reader.bits(1);
    if (!nalHrd || (*nalHrd && !skipHrd(reader)))
        return std::nullopt;
    const auto vclHrd = reader.bits(1);
    if (!vclHrd || (*vclHrd && !skipHrd(reader)))
        return std::nullopt;
    if ((*nalHrd || *vclHrd) && !reader.skip(1))
        return std::nullopt;
    if (!reader.skip(1))
        return std::nullopt;

    sps.m_cut = reader.position();
    const auto restricted = reader.bits(1);
    if (!restricted)
        return std::nullopt;
    if (*restricted)
    {
        sps.m_restriction = readRestriction(reader);
        if (!sps.m_restriction)
            return std::nullopt;
    }
    return sps;
}

std::vector<std::uint8_t> H264Sps::forDecoder(std::uint32_t maxReferences) const
{
    const std::uint32_t references = std::max<std::uint32_t>(std::min(m_max_num_ref_frames, maxReferences), 1);
    BitReader source(m_rbsp);
    BitWriter out;
    for (std::size_t i = 0; i < m_references_begin; ++i)
        out.put(*source.bits(1), 1);
    out.ue(references);
    (void)source.ue();
    for (std::size_t i = m_references_end; i < m_cut; ++i)
        out.put(*source.bits(1), 1);
    if (!m_has_vui)
    {
        out.put(1, 1);
        out.put(0, 8);
    }
    const Restriction restriction = m_restriction.value_or(Restriction{});
    out.put(1, 1);
    out.put(restriction.motion_vectors_over_pic_boundaries, 1);
    out.ue(restriction.max_bytes_per_pic_denom);
    out.ue(restriction.max_bits_per_mb_denom);
    out.ue(restriction.log2_max_mv_length_horizontal);
    out.ue(restriction.log2_max_mv_length_vertical);
    out.ue(0);
    out.ue(references);
    out.trail();

    std::vector<std::uint8_t> nal{m_header};
    std::ranges::copy(escape(out.bytes()), std::back_inserter(nal));
    return nal;
}

std::optional<std::uint32_t> H264Sps::maxDecFrameBuffering() const
{
    if (!m_restriction)
        return std::nullopt;
    return m_restriction->max_dec_frame_buffering;
}

std::vector<std::uint8_t> H264Sps::unescape(std::span<const std::uint8_t> payload)
{
    std::vector<std::uint8_t> rbsp;
    rbsp.reserve(payload.size());
    unsigned zeros = 0;
    for (const std::uint8_t byte : payload)
    {
        if (zeros >= 2 && byte == 0x03)
        {
            zeros = 0;
            continue;
        }
        rbsp.push_back(byte);
        zeros = byte == 0 ? zeros + 1 : 0;
    }
    return rbsp;
}

std::vector<std::uint8_t> H264Sps::escape(std::span<const std::uint8_t> rbsp)
{
    std::vector<std::uint8_t> payload;
    payload.reserve(rbsp.size() + rbsp.size() / 2);
    unsigned zeros = 0;
    for (const std::uint8_t byte : rbsp)
    {
        if (zeros >= 2 && byte <= 0x03)
        {
            payload.push_back(0x03);
            zeros = 0;
        }
        payload.push_back(byte);
        zeros = byte == 0 ? zeros + 1 : 0;
    }
    return payload;
}

bool H264Sps::skipScalingList(BitReader& reader, std::size_t size)
{
    std::int32_t last = 8;
    std::int32_t next = 8;
    for (std::size_t j = 0; j < size; ++j)
    {
        if (next != 0)
        {
            const auto delta = reader.se();
            if (!delta)
                return false;
            next = (last + *delta + 256) % 256;
        }
        last = next == 0 ? last : next;
    }
    return true;
}

bool H264Sps::skipHrd(BitReader& reader)
{
    const auto count = reader.ue();
    if (!count || *count > 31 || !reader.skip(8))
        return false;
    for (std::uint32_t i = 0; i <= *count; ++i)
    {
        if (!reader.skipUe() || !reader.skipUe() || !reader.skip(1))
            return false;
    }
    return reader.skip(20);
}

std::optional<H264Sps::Restriction> H264Sps::readRestriction(BitReader& reader)
{
    Restriction restriction;
    const auto motion = reader.bits(1);
    const auto bytes = motion ? reader.ue() : std::nullopt;
    const auto bits = bytes ? reader.ue() : std::nullopt;
    const auto horizontal = bits ? reader.ue() : std::nullopt;
    const auto vertical = horizontal ? reader.ue() : std::nullopt;
    const auto reorder = vertical ? reader.ue() : std::nullopt;
    const auto buffering = reorder ? reader.ue() : std::nullopt;
    if (!buffering)
        return std::nullopt;
    restriction.motion_vectors_over_pic_boundaries = *motion;
    restriction.max_bytes_per_pic_denom = *bytes;
    restriction.max_bits_per_mb_denom = *bits;
    restriction.log2_max_mv_length_horizontal = *horizontal;
    restriction.log2_max_mv_length_vertical = *vertical;
    restriction.max_num_reorder_frames = *reorder;
    restriction.max_dec_frame_buffering = *buffering;
    return restriction;
}

}
