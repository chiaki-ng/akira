#ifndef AKIRA_PAIR_CRYPTO_HPP
#define AKIRA_PAIR_CRYPTO_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <libchiaking/platform/platform.hpp>

namespace akira::pair {

constexpr std::size_t kPubKeyLen = 64;
constexpr std::size_t kSharedLen = 32;
constexpr std::size_t kSaltNonceLen = 16;
constexpr std::size_t kIvLen = 16;
constexpr std::size_t kMacLen = 32;
constexpr std::size_t kKeyLen = 32;

void hmacSha256(const uint8_t* key, std::size_t keyLen, const uint8_t* data, std::size_t dataLen, uint8_t out[kMacLen]);
void hkdfSha256(const uint8_t* ikm, std::size_t ikmLen, const uint8_t* salt, std::size_t saltLen,
                const uint8_t* info, std::size_t infoLen, uint8_t* out, std::size_t outLen);
void aes256CtrXor(const uint8_t key[kKeyLen], const uint8_t iv[kIvLen], const uint8_t* in, uint8_t* out, std::size_t len);
void deriveKeys(const uint8_t shared[kSharedLen], const uint8_t* saltNonce, std::size_t saltNonceLen,
                const std::string& code, uint8_t keyEnc[kKeyLen], uint8_t keyMac[kKeyLen]);

struct Hello {
    uint8_t switchPub[kPubKeyLen];
    uint8_t saltNonce[kSaltNonceLen];
    std::vector<uint8_t> encode() const;
    static bool decode(const uint8_t* data, std::size_t len, Hello& out);
};

enum class OpenResult { Ok, Malformed, BadKey, BadMac };

class KeyExchange {
public:
    KeyExchange(libchiaking::platform::CryptoProvider& crypto,
                libchiaking::platform::SecureRandomProvider& random) noexcept;

    std::unique_ptr<libchiaking::platform::EcdhKeyPair> generateKeypair(uint8_t pub[kPubKeyLen]);
    static bool ecdhShared(const libchiaking::platform::EcdhKeyPair& key, const uint8_t peerPub[kPubKeyLen],
                           uint8_t shared[kSharedLen]);
    std::unique_ptr<libchiaking::platform::EcdhKeyPair> makeHello(Hello& out);
    OpenResult openSealed(const Hello& hello, const libchiaking::platform::EcdhKeyPair& switchKey,
                          const uint8_t* sealed, std::size_t sealedLen, const std::string& code,
                          std::vector<uint8_t>& plaintext);

private:
    libchiaking::platform::CryptoProvider& crypto_;
    libchiaking::platform::SecureRandomProvider& random_;
};

}

#endif
