#ifndef AKIRA_CLOUD_MODELS_HPP
#define AKIRA_CLOUD_MODELS_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cloud {

enum class PlusMembership {
    Unknown,
    None,
    SomeTier
};

inline PlusMembership observePlusMembership(std::optional<bool> isPlus)
{
    if (!isPlus)
        return PlusMembership::Unknown;
    return *isPlus ? PlusMembership::SomeTier : PlusMembership::None;
}

enum class StreamabilityStatus {
    Unknown,
    Streamable,
    NotStreamable
};

enum class ReleaseKind {
    Unrestricted,
    Scheduled,
    Unannounced
};

enum class Standing {
    MayStream,
    InLibrary,
    Included,
    Store,
    OwnershipUnknown
};

enum class LaunchBlock {
    None,
    ChooseAGame,
    Refused
};

struct BundledGame {
    std::string productId;
    std::string entitlementId;
    bool launchable = false;
};

struct Game {
    std::string productId;
    std::string name;
    std::string imageUrl;
    std::string landscapeImageUrl;
    std::string conceptId;
    std::string category;
    std::string serviceType;
    std::string platform;
    bool isOwned = false;
    std::string streamServiceType;
    std::string streamIdentifier;
    std::string entitlementId;
    std::string storeProductId;
    std::string conceptUrl;
    bool plusCatalog = false;
    bool needsLibraryAdd = false;
    int64_t availableFromMs = 0;
    ReleaseKind release = ReleaseKind::Unrestricted;
    Standing standing = Standing::MayStream;
    LaunchBlock launchBlock = LaunchBlock::None;
    bool launchLeftOut = false;
    bool canBeAddedToLibrary = false;
    bool needsStorefrontCheck = false;
    bool isTrial = false;
    bool trialEnded = false;
    bool refused = false;
    std::vector<BundledGame> bundle;
    StreamabilityStatus streamabilityStatus = StreamabilityStatus::Unknown;

    bool launchable() const { return !productId.empty() && !streamServiceType.empty(); }
    bool requiresLibraryAdd() const { return needsLibraryAdd; }
    bool streamableNow() const { return launchable() && (isOwned || category != "purchaseable"); }
    bool awaitsRelease(int64_t nowMs) const;
    std::optional<int64_t> unlockAfter(int64_t nowMs) const;
    bool isOwnedPS5() const { return isOwned && platform == "ps5"; }
    bool isInCatalog() const { return platform != "ps5" || streamServiceType == "psnow" || plusCatalog; }
    bool isStoreOnly() const { return !isInCatalog() && !isOwned; }
    bool isBundle() const { return !bundle.empty(); }
    bool opensBundlePicker() const { return isBundle() && launchBlock == LaunchBlock::ChooseAGame; }
    std::string artworkUrl() const;

    static std::string countdown(int64_t seconds);
};

struct GameDetail {
    enum class Art {
        None,
        Layered,
        GameHub
    };

    struct Rating {
        double average = 0;
        std::string averageText;
        int64_t count = 0;
        std::array<int, 5> percentByStars{};
    };

    struct Age {
        std::string name;
        std::string descriptors;
        std::string icon;
    };

    Art art = Art::None;
    std::string background;
    std::string hero;
    std::string logo;
    std::optional<Rating> rating;
    std::optional<Age> age;
    std::string publisher;
    std::optional<int64_t> releasedMs;
    std::vector<std::string> genres;
};

struct Catalog {
    std::vector<Game> games;

    int launchableCount() const;
    const Game* find(const Game& game) const;
};

struct Datacenter {
    std::string name;
    int rttMs = 0;
    std::vector<int> rttSamples;
    int mtuIn = 0;
    int mtuOut = 0;
    int port = 0;
    std::string publicIp;
    int maxBandwidth = 0;
    bool measured = false;
    int64_t measuredAtMs = 0;
};

std::string platformBadge(const Game& game);
std::string categoryBadge(const Game& game);

} // namespace cloud

#endif // AKIRA_CLOUD_MODELS_HPP
