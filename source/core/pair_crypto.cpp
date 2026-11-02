#include "core/pair_crypto.hpp"

#include "util/sha256.hpp"

#include <array>
#include <cstring>
#include <memory>
#include <span>
#include <utility>

namespace akira::pair {

namespace {

const char kMagic[4] = {'A', 'K', 'P', 'R'};
constexpr uint8_t kVersion = 1;
const char kInfo[] = "akira-pair-v1";

constexpr std::size_t kHelloLen = 5 + kPubKeyLen + kSaltNonceLen;
constexpr std::size_t kSealedMin = 5 + kPubKeyLen + kIvLen + 4 + kMacLen;

const uint8_t kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

const uint8_t kRcon[8] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};

uint8_t xtime(uint8_t x) {
    return static_cast<uint8_t>((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

void aesKeyExpansion(const uint8_t key[32], uint8_t rk[240]) {
    std::memcpy(rk, key, 32);
    uint8_t t[4];
    for (int i = 8; i < 60; i++) {
        int k = (i - 1) * 4;
        t[0] = rk[k + 0];
        t[1] = rk[k + 1];
        t[2] = rk[k + 2];
        t[3] = rk[k + 3];
        if (i % 8 == 0) {
            uint8_t tmp = t[0];
            t[0] = static_cast<uint8_t>(kSbox[t[1]] ^ kRcon[i / 8]);
            t[1] = kSbox[t[2]];
            t[2] = kSbox[t[3]];
            t[3] = kSbox[tmp];
        } else if (i % 8 == 4) {
            t[0] = kSbox[t[0]];
            t[1] = kSbox[t[1]];
            t[2] = kSbox[t[2]];
            t[3] = kSbox[t[3]];
        }
        int j = i * 4;
        int m = (i - 8) * 4;
        rk[j + 0] = static_cast<uint8_t>(rk[m + 0] ^ t[0]);
        rk[j + 1] = static_cast<uint8_t>(rk[m + 1] ^ t[1]);
        rk[j + 2] = static_cast<uint8_t>(rk[m + 2] ^ t[2]);
        rk[j + 3] = static_cast<uint8_t>(rk[m + 3] ^ t[3]);
    }
}

void aesEncryptBlock(const uint8_t rk[240], const uint8_t in[16], uint8_t out[16]) {
    uint8_t s[16];
    for (int i = 0; i < 16; i++)
        s[i] = static_cast<uint8_t>(in[i] ^ rk[i]);

    for (int round = 1; round <= 14; round++) {
        for (int i = 0; i < 16; i++)
            s[i] = kSbox[s[i]];

        uint8_t r1 = s[1];
        s[1] = s[5];
        s[5] = s[9];
        s[9] = s[13];
        s[13] = r1;
        uint8_t r2a = s[2], r2b = s[6];
        s[2] = s[10];
        s[6] = s[14];
        s[10] = r2a;
        s[14] = r2b;
        uint8_t r3 = s[15];
        s[15] = s[11];
        s[11] = s[7];
        s[7] = s[3];
        s[3] = r3;

        if (round != 14) {
            for (int c = 0; c < 4; c++) {
                uint8_t a0 = s[4 * c + 0], a1 = s[4 * c + 1], a2 = s[4 * c + 2], a3 = s[4 * c + 3];
                s[4 * c + 0] = static_cast<uint8_t>(xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3);
                s[4 * c + 1] = static_cast<uint8_t>(a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3);
                s[4 * c + 2] = static_cast<uint8_t>(a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3));
                s[4 * c + 3] = static_cast<uint8_t>((xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3));
            }
        }

        const uint8_t* k = rk + 16 * round;
        for (int i = 0; i < 16; i++)
            s[i] = static_cast<uint8_t>(s[i] ^ k[i]);
    }

    std::memcpy(out, s, 16);
}

void sha256Once(const uint8_t* d, std::size_t n, uint8_t out[32]) {
    akira::sha256::Ctx c;
    akira::sha256::init(c);
    akira::sha256::update(c, d, n);
    akira::sha256::final(c, out);
}

bool constTimeEq(const uint8_t* a, const uint8_t* b, std::size_t n) {
    uint8_t diff = 0;
    for (std::size_t i = 0; i < n; i++)
        diff = static_cast<uint8_t>(diff | (a[i] ^ b[i]));
    return diff == 0;
}

}

KeyExchange::KeyExchange(libchiaking::platform::CryptoProvider& crypto,
                         libchiaking::platform::SecureRandomProvider& random) noexcept
    : crypto_(crypto), random_(random) {}

void hmacSha256(const uint8_t* key, std::size_t keyLen, const uint8_t* data, std::size_t dataLen, uint8_t out[kMacLen]) {
    uint8_t k[64] = {0};
    if (keyLen > 64) {
        uint8_t hashed[32];
        sha256Once(key, keyLen, hashed);
        std::memcpy(k, hashed, 32);
    } else {
        std::memcpy(k, key, keyLen);
    }

    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; i++) {
        ipad[i] = static_cast<uint8_t>(k[i] ^ 0x36);
        opad[i] = static_cast<uint8_t>(k[i] ^ 0x5c);
    }

    uint8_t inner[32];
    akira::sha256::Ctx c;
    akira::sha256::init(c);
    akira::sha256::update(c, ipad, 64);
    akira::sha256::update(c, data, dataLen);
    akira::sha256::final(c, inner);

    akira::sha256::init(c);
    akira::sha256::update(c, opad, 64);
    akira::sha256::update(c, inner, 32);
    akira::sha256::final(c, out);
}

void hkdfSha256(const uint8_t* ikm, std::size_t ikmLen, const uint8_t* salt, std::size_t saltLen,
                const uint8_t* info, std::size_t infoLen, uint8_t* out, std::size_t outLen) {
    uint8_t prk[32];
    if (saltLen == 0) {
        uint8_t zero[32] = {0};
        hmacSha256(zero, 32, ikm, ikmLen, prk);
    } else {
        hmacSha256(salt, saltLen, ikm, ikmLen, prk);
    }

    uint8_t t[32];
    std::size_t tLen = 0;
    std::size_t done = 0;
    uint8_t counter = 1;
    while (done < outLen) {
        std::vector<uint8_t> buf;
        buf.reserve(tLen + infoLen + 1);
        if (tLen > 0)
            buf.insert(buf.end(), t, t + tLen);
        buf.insert(buf.end(), info, info + infoLen);
        buf.push_back(counter);
        hmacSha256(prk, 32, buf.data(), buf.size(), t);
        tLen = 32;

        std::size_t take = outLen - done;
        if (take > 32)
            take = 32;
        std::memcpy(out + done, t, take);
        done += take;
        counter++;
    }
}

void aes256CtrXor(const uint8_t key[kKeyLen], const uint8_t iv[kIvLen], const uint8_t* in, uint8_t* out, std::size_t len) {
    uint8_t rk[240];
    aesKeyExpansion(key, rk);

    uint8_t counter[16];
    std::memcpy(counter, iv, 16);
    uint8_t stream[16];

    std::size_t off = 0;
    while (off < len) {
        aesEncryptBlock(rk, counter, stream);
        std::size_t block = len - off;
        if (block > 16)
            block = 16;
        for (std::size_t i = 0; i < block; i++)
            out[off + i] = static_cast<uint8_t>(in[off + i] ^ stream[i]);
        off += block;
        for (int i = 15; i >= 0; i--) {
            if (++counter[i] != 0)
                break;
        }
    }
}

bool KeyExchange::ecdhShared(const libchiaking::platform::EcdhKeyPair& key, const uint8_t peerPub[kPubKeyLen],
                             uint8_t shared[kSharedLen]) {
    return key.derive_secret(std::as_bytes(std::span{peerPub, kPubKeyLen}),
                             std::as_writable_bytes(std::span{shared, kSharedLen}))
        .has_value();
}

std::unique_ptr<libchiaking::platform::EcdhKeyPair> KeyExchange::generateKeypair(uint8_t pub[kPubKeyLen]) {
    auto made = crypto_.generate_ecdh(libchiaking::platform::EcdhCurve::secp256r1);
    if (!made)
        return nullptr;
    if (!made.value()->public_coordinates(std::as_writable_bytes(std::span{pub, kPubKeyLen})))
        return nullptr;
    return std::move(made.value());
}

void deriveKeys(const uint8_t shared[kSharedLen], const uint8_t* saltNonce, std::size_t saltNonceLen,
                const std::string& code, uint8_t keyEnc[kKeyLen], uint8_t keyMac[kKeyLen]) {
    std::vector<uint8_t> salt;
    salt.reserve(saltNonceLen + code.size());
    salt.insert(salt.end(), saltNonce, saltNonce + saltNonceLen);
    salt.insert(salt.end(), code.begin(), code.end());

    uint8_t okm[2 * kKeyLen];
    hkdfSha256(shared, kSharedLen, salt.data(), salt.size(),
               reinterpret_cast<const uint8_t*>(kInfo), sizeof(kInfo) - 1, okm, sizeof(okm));
    std::memcpy(keyEnc, okm, kKeyLen);
    std::memcpy(keyMac, okm + kKeyLen, kKeyLen);
}

std::vector<uint8_t> Hello::encode() const {
    std::vector<uint8_t> buf;
    buf.reserve(kHelloLen);
    buf.insert(buf.end(), kMagic, kMagic + 4);
    buf.push_back(kVersion);
    buf.insert(buf.end(), switchPub, switchPub + kPubKeyLen);
    buf.insert(buf.end(), saltNonce, saltNonce + kSaltNonceLen);
    return buf;
}

bool Hello::decode(const uint8_t* data, std::size_t len, Hello& out) {
    if (len != kHelloLen)
        return false;
    if (std::memcmp(data, kMagic, 4) != 0 || data[4] != kVersion)
        return false;
    std::memcpy(out.switchPub, data + 5, kPubKeyLen);
    std::memcpy(out.saltNonce, data + 5 + kPubKeyLen, kSaltNonceLen);
    return true;
}

std::unique_ptr<libchiaking::platform::EcdhKeyPair> KeyExchange::makeHello(Hello& out) {
    auto key = generateKeypair(out.switchPub);
    if (!key)
        return nullptr;
    if (!random_.fill_bytes(std::as_writable_bytes(std::span{out.saltNonce})))
        return nullptr;
    return key;
}

OpenResult KeyExchange::openSealed(const Hello& hello, const libchiaking::platform::EcdhKeyPair& switchKey,
                                   const uint8_t* sealed, std::size_t sealedLen, const std::string& code,
                                   std::vector<uint8_t>& plaintext) {
    if (sealedLen < kSealedMin)
        return OpenResult::Malformed;
    if (std::memcmp(sealed, kMagic, 4) != 0 || sealed[4] != kVersion)
        return OpenResult::Malformed;

    const uint8_t* desktopPub = sealed + 5;
    const uint8_t* iv = sealed + 5 + kPubKeyLen;
    std::size_t lenOff = 5 + kPubKeyLen + kIvLen;
    std::size_t ctLen = (static_cast<std::size_t>(sealed[lenOff]) << 24) |
                        (static_cast<std::size_t>(sealed[lenOff + 1]) << 16) |
                        (static_cast<std::size_t>(sealed[lenOff + 2]) << 8) |
                        static_cast<std::size_t>(sealed[lenOff + 3]);
    std::size_t ctOff = lenOff + 4;
    if (sealedLen != ctOff + ctLen + kMacLen)
        return OpenResult::Malformed;

    const uint8_t* ct = sealed + ctOff;
    const uint8_t* mac = sealed + ctOff + ctLen;

    uint8_t shared[kSharedLen];
    if (!ecdhShared(switchKey, desktopPub, shared))
        return OpenResult::BadKey;

    uint8_t keyEnc[kKeyLen], keyMac[kKeyLen];
    deriveKeys(shared, hello.saltNonce, kSaltNonceLen, code, keyEnc, keyMac);

    uint8_t expected[kMacLen];
    hmacSha256(keyMac, kKeyLen, sealed, sealedLen - kMacLen, expected);
    if (!constTimeEq(expected, mac, kMacLen))
        return OpenResult::BadMac;

    plaintext.resize(ctLen);
    aes256CtrXor(keyEnc, iv, ct, plaintext.data(), ctLen);
    return OpenResult::Ok;
}

}
