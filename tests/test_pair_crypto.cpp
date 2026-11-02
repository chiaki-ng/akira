#include "test_util.hpp"

#include "core/pair_crypto.hpp"

#include <libchiaking/platform/crypto/openssl/provider.hpp>
#include <libchiaking/platform/runtime/random_provider.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace akira;

namespace {

std::vector<uint8_t> hexToBytes(const std::string& hex)
{
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        out.push_back(static_cast<uint8_t>((nib(hex[i]) << 4) | nib(hex[i + 1])));
    return out;
}

std::string bytesToHex(const uint8_t* p, std::size_t n)
{
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (std::size_t i = 0; i < n; i++) {
        out.push_back(d[p[i] >> 4]);
        out.push_back(d[p[i] & 0x0f]);
    }
    return out;
}

std::string vecHex(const std::vector<uint8_t>& v)
{
    return bytesToHex(v.data(), v.size());
}

std::vector<uint8_t> buildSealed(const uint8_t* desktopPub, const uint8_t* iv,
                                 const std::vector<uint8_t>& ct, const uint8_t* mac)
{
    std::vector<uint8_t> s;
    const char magic[4] = {'A', 'K', 'P', 'R'};
    s.insert(s.end(), magic, magic + 4);
    s.push_back(1);
    s.insert(s.end(), desktopPub, desktopPub + pair::kPubKeyLen);
    s.insert(s.end(), iv, iv + pair::kIvLen);
    uint32_t n = static_cast<uint32_t>(ct.size());
    s.push_back(static_cast<uint8_t>(n >> 24));
    s.push_back(static_cast<uint8_t>(n >> 16));
    s.push_back(static_cast<uint8_t>(n >> 8));
    s.push_back(static_cast<uint8_t>(n));
    s.insert(s.end(), ct.begin(), ct.end());
    if (mac != nullptr)
        s.insert(s.end(), mac, mac + pair::kMacLen);
    return s;
}

const char* kCode = "4729";
const char* kPayload = "{\"duid\":\"0000000700410080deadbeefdeadbeefdeadbeefdeadbeef\",\"hello\":\"akira\"}";

const char* kDesktopPubHex =
    "d65a93977caa3d1b081852ff57a79e465f1660577304baead505dd3a48589cf350185e895372df6221ea3a137557e473fddb6755f05bd507c3c533fce9c91285";
const char* kSharedHex = "ccfc261f58193c98ca4ad4a53bbac6f0ee29bc4d48438090446908622ca79af6";
const char* kKeyEncHex = "36880b0f7643f30e922a3a7863104225e3f3ef5906959ec080153c9f230ed8a7";
const char* kKeyMacHex = "22855abe398d7b565d85ef55bc9f24219cd27a3cb3a08233052676f5b0e6913e";
const char* kCtHex =
    "8a282fbdd682557a80e1c5fe349db4cbaf421ade29717f0157c0803726a50dbc4d67edd12f8b53b285eaeecf0d42386cd6bfae8c847b274541c5cbf94b4be4f7c6d983ed7c417dc5f4f91b";
const char* kMacHex = "ba5f8878332de6941f9796222c83e4c716a12f114604443b3310e097cb7a1310";

libchiaking::platform::openssl::OpenSslCryptoProvider openssl;
libchiaking::platform::SystemSecureRandom entropy;

class FixedP256Key final : public libchiaking::platform::EcdhKeyPair {
public:
    explicit FixedP256Key(std::vector<uint8_t> scalar) : scalar_(std::move(scalar)) {}

    auto public_coordinates(std::span<std::byte> output) const
        -> libchiaking::Expected<void, libchiaking::platform::CryptoError> override
    {
        using namespace libchiaking::platform;
        std::array<std::byte, p256_point_size> point{};
        if (auto made = openssl.p256_base_multiply(std::as_bytes(std::span{scalar_}), point); !made)
            return made;
        std::memcpy(output.data(), point.data() + sec1_uncompressed_point_prefix, output.size());
        return {};
    }

    auto derive_secret(std::span<const std::byte> peer, std::span<std::byte> secret) const
        -> libchiaking::Expected<void, libchiaking::platform::CryptoError> override
    {
        using namespace libchiaking::platform;
        std::array<std::byte, p256_point_size> encoded{};
        encoded[0] = std::byte{sec1_uncompressed_tag};
        std::memcpy(encoded.data() + sec1_uncompressed_point_prefix, peer.data(), peer.size());
        std::array<std::byte, p256_point_size> product{};
        if (auto made = openssl.p256_multiply(std::as_bytes(std::span{scalar_}), encoded, product); !made)
            return made;
        std::memcpy(secret.data(), product.data() + sec1_uncompressed_point_prefix, secret.size());
        return {};
    }

private:
    std::vector<uint8_t> scalar_;
};

}

TEST(pair_crypto_kat_matches_go_vector)
{
    FixedP256Key switchKey{std::vector<uint8_t>(32, 0x11)};
    std::vector<uint8_t> desktopPub = hexToBytes(kDesktopPubHex);
    std::vector<uint8_t> saltNonce(pair::kSaltNonceLen, 0x33);
    std::vector<uint8_t> iv(pair::kIvLen, 0x44);
    std::vector<uint8_t> payload(kPayload, kPayload + std::strlen(kPayload));

    uint8_t shared[pair::kSharedLen];
    CHECK(pair::KeyExchange::ecdhShared(switchKey, desktopPub.data(), shared));
    CHECK_EQ(bytesToHex(shared, pair::kSharedLen), std::string(kSharedHex));

    uint8_t keyEnc[pair::kKeyLen], keyMac[pair::kKeyLen];
    pair::deriveKeys(shared, saltNonce.data(), saltNonce.size(), kCode, keyEnc, keyMac);
    CHECK_EQ(bytesToHex(keyEnc, pair::kKeyLen), std::string(kKeyEncHex));
    CHECK_EQ(bytesToHex(keyMac, pair::kKeyLen), std::string(kKeyMacHex));

    std::vector<uint8_t> ct(payload.size());
    pair::aes256CtrXor(keyEnc, iv.data(), payload.data(), ct.data(), payload.size());
    CHECK_EQ(vecHex(ct), std::string(kCtHex));

    std::vector<uint8_t> prefix = buildSealed(desktopPub.data(), iv.data(), ct, nullptr);
    uint8_t mac[pair::kMacLen];
    pair::hmacSha256(keyMac, pair::kKeyLen, prefix.data(), prefix.size(), mac);
    CHECK_EQ(bytesToHex(mac, pair::kMacLen), std::string(kMacHex));
}

TEST(pair_crypto_open_interops_with_go_sealed)
{
    FixedP256Key switchKey{std::vector<uint8_t>(32, 0x11)};
    std::vector<uint8_t> desktopPub = hexToBytes(kDesktopPubHex);
    std::vector<uint8_t> iv(pair::kIvLen, 0x44);
    std::vector<uint8_t> ct = hexToBytes(kCtHex);
    std::vector<uint8_t> mac = hexToBytes(kMacHex);
    std::string payload(kPayload);

    pair::Hello hello;
    std::memset(hello.switchPub, 0, pair::kPubKeyLen);
    std::memset(hello.saltNonce, 0x33, pair::kSaltNonceLen);

    std::vector<uint8_t> sealed = buildSealed(desktopPub.data(), iv.data(), ct, mac.data());

    pair::KeyExchange exchange{openssl, entropy};
    std::vector<uint8_t> out;
    pair::OpenResult r = exchange.openSealed(hello, switchKey, sealed.data(), sealed.size(), kCode, out);
    CHECK(r == pair::OpenResult::Ok);
    CHECK_EQ(std::string(out.begin(), out.end()), payload);

    std::vector<uint8_t> out2;
    pair::OpenResult wrong = exchange.openSealed(hello, switchKey, sealed.data(), sealed.size(), "0000", out2);
    CHECK(wrong == pair::OpenResult::BadMac);

    std::vector<uint8_t> tampered = sealed;
    tampered[90] ^= 0x01;
    std::vector<uint8_t> out3;
    pair::OpenResult t = exchange.openSealed(hello, switchKey, tampered.data(), tampered.size(), kCode, out3);
    CHECK(t == pair::OpenResult::BadMac);
}

TEST(pair_crypto_generated_keys_round_trip)
{
    pair::KeyExchange exchange{openssl, entropy};
    pair::Hello hello;
    auto switchKey = exchange.makeHello(hello);
    CHECK(switchKey != nullptr);

    uint8_t desktopPub[pair::kPubKeyLen];
    auto desktopKey = exchange.generateKeypair(desktopPub);
    CHECK(desktopKey != nullptr);

    uint8_t shared[pair::kSharedLen];
    CHECK(pair::KeyExchange::ecdhShared(*desktopKey, hello.switchPub, shared));

    uint8_t sharedRev[pair::kSharedLen];
    CHECK(pair::KeyExchange::ecdhShared(*switchKey, desktopPub, sharedRev));
    CHECK_EQ(bytesToHex(shared, pair::kSharedLen), bytesToHex(sharedRev, pair::kSharedLen));

    uint8_t keyEnc[pair::kKeyLen], keyMac[pair::kKeyLen];
    pair::deriveKeys(shared, hello.saltNonce, pair::kSaltNonceLen, "1234", keyEnc, keyMac);

    uint8_t iv[pair::kIvLen];
    CHECK(entropy.fill_bytes(std::as_writable_bytes(std::span{iv})).has_value());
    std::string payload("resume-on-console credentials blob");
    std::vector<uint8_t> ct(payload.size());
    pair::aes256CtrXor(keyEnc, iv, reinterpret_cast<const uint8_t*>(payload.data()), ct.data(), payload.size());

    std::vector<uint8_t> prefix = buildSealed(desktopPub, iv, ct, nullptr);
    uint8_t mac[pair::kMacLen];
    pair::hmacSha256(keyMac, pair::kKeyLen, prefix.data(), prefix.size(), mac);
    std::vector<uint8_t> sealed = buildSealed(desktopPub, iv, ct, mac);

    std::vector<uint8_t> out;
    pair::OpenResult r = exchange.openSealed(hello, *switchKey, sealed.data(), sealed.size(), "1234", out);
    CHECK(r == pair::OpenResult::Ok);
    CHECK_EQ(std::string(out.begin(), out.end()), payload);

    std::vector<uint8_t> out2;
    pair::OpenResult wrong = exchange.openSealed(hello, *switchKey, sealed.data(), sealed.size(), "9999", out2);
    CHECK(wrong == pair::OpenResult::BadMac);
}
