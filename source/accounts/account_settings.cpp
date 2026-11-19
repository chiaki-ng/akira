#include "accounts/account_settings.hpp"
#include "settings/settings_store.hpp"
#include "log/log.hpp"

#include <utility>

namespace akira::accounts {

AccountSettings::AccountSettings(settings::SettingsStore& store, LegacyGate legacyGate)
    : store(store), legacyGate(std::move(legacyGate)), signInStore(*this, store)
{
    read(store.loaded());
    store.attach(*this);
}

AccountSettings::~AccountSettings()
{
    store.detach(*this);
}

void AccountSettings::read(const toml::table& config)
{
    if (const auto* rows = config["profiles"].as_array()) {
        for (const auto& element : *rows) {
            const auto* row = element.as_table();
            if (!row) continue;

            Profile profile;
            profile.id = (*row)["profile_id"].value<int64_t>().value_or(0);
            if (profile.id <= 0) continue;

            profile.onlineId = (*row)["online_id"].value<std::string>().value_or("");
            profile.accountId = (*row)["account_id"].value<std::string>().value_or("");
            profile.avatarUrl = (*row)["avatar_url"].value<std::string>().value_or("");
            profile.trophiesEnabled = (*row)["trophies_enabled"].value<bool>().value_or(true);
            profile.legacy = (*row)["legacy"].value<bool>().value_or(false);
            profile.cloudShortcuts = readShortcuts((*row)["cloud_shortcuts"].as_array());

            entries.push_back(profile);
            if (profile.id >= nextId)
                nextId = profile.id + 1;
        }
    }

    if (auto val = config["active_profile_id"].value<int64_t>())
        activeId = *val;

    log::info("Loaded {} profile(s) from TOML config", entries.size());
    logProfileCensus("load");
    refreshLegacyGate();
}

void AccountSettings::write(toml::table& config) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    toml::array rows;
    for (const Profile& p : entries) {
        toml::table row;
        row.insert("profile_id", p.id);
        if (!p.onlineId.empty()) row.insert("online_id", p.onlineId);
        if (!p.accountId.empty()) row.insert("account_id", p.accountId);
        if (!p.avatarUrl.empty()) row.insert("avatar_url", p.avatarUrl);
        if (!p.trophiesEnabled) row.insert("trophies_enabled", false);
        if (p.legacy) row.insert("legacy", true);
        if (!p.cloudShortcuts.empty()) row.insert("cloud_shortcuts", shortcutRows(p.cloudShortcuts));
        rows.push_back(std::move(row));
    }
    if (!rows.empty())
        config.insert("profiles", std::move(rows));

    if (activeId > 0)
        config.insert("active_profile_id", activeId);
}

std::vector<Profile> AccountSettings::profilesSnapshot() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return entries;
}

Profile* AccountSettings::findProfile(int64_t id)
{
    for (Profile& p : entries)
        if (p.id == id) return &p;
    return nullptr;
}

const Profile* AccountSettings::findProfile(int64_t id) const
{
    for (const Profile& p : entries)
        if (p.id == id) return &p;
    return nullptr;
}

Profile* AccountSettings::activeProfile()
{
    return findProfile(activeId);
}

const Profile* AccountSettings::activeProfile() const
{
    return findProfile(activeId);
}

Profile* AccountSettings::ensureActiveProfile()
{
    if (Profile* p = activeProfile())
        return p;
    const int64_t id = addProfile(Profile{});
    activeId = id;
    return findProfile(id);
}

void AccountSettings::setActiveProfileId(int64_t id)
{
    if (activeId != id) {
        const Profile* from = findProfile(activeId);
        const Profile* to = findProfile(id);
        log::info("Active profile {} ({}) -> {} ({})",
            activeId, from ? from->label() : std::string("<none>"),
            id, to ? to->label() : std::string("<none>"));
    }
    activeId = id;
    refreshLegacyGate();
}

void AccountSettings::logProfileCensus(const char* stage) const
{
    log::info("Profile census [{}]: active={} count={}", stage, activeId, entries.size());
    for (const Profile& p : entries) {
        log::info("  profile id={} label='{}' legacy={} account={} shortcuts={}",
            p.id, p.label(), p.legacy, !p.accountId.empty(), p.cloudShortcuts.size());
    }
}

bool AccountSettings::readProfile(int64_t id, const std::function<void(const Profile&)>& fn) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const Profile* p = findProfile(id);
    if (!p)
        return false;
    fn(*p);
    return true;
}

bool AccountSettings::updateProfile(int64_t id, const std::function<void(Profile&)>& fn)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    Profile* p = findProfile(id);
    if (!p)
        return false;
    fn(*p);
    return true;
}

bool AccountSettings::activeProfileTrophiesEnabled() const
{
    const Profile* p = activeProfile();
    return p ? p->trophiesEnabled : true;
}

void AccountSettings::setActiveProfileTrophiesEnabled(bool enabled)
{
    if (Profile* p = activeProfile())
        p->trophiesEnabled = enabled;
}

int64_t AccountSettings::addProfile(const Profile& profile)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    Profile copy = profile;
    copy.id = nextId++;
    entries.push_back(copy);
    return copy.id;
}

void AccountSettings::removeProfile(int64_t id)
{
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        std::erase_if(entries, [id](const Profile& p) { return p.id == id; });
        if (activeId == id)
            activeId = entries.empty() ? 0 : entries.front().id;
    }
    refreshLegacyGate();
}

bool AccountSettings::isLegacyProfileActive() const
{
    const Profile* p = activeProfile();
    return p && p->legacy;
}

void AccountSettings::refreshLegacyGate() const
{
    if (legacyGate)
        legacyGate(isLegacyProfileActive());
}

std::string AccountSettings::psnOnlineId() const
{
    const Profile* p = activeProfile();
    return p ? p->onlineId : "";
}

void AccountSettings::setPsnOnlineId(const std::string& id)
{
    ensureActiveProfile()->onlineId = id;
}

std::string AccountSettings::psnAccountId() const
{
    const Profile* p = activeProfile();
    return p ? p->accountId : "";
}

void AccountSettings::setPsnAccountId(const std::string& id)
{
    ensureActiveProfile()->accountId = id;
}

const std::vector<cloud::Game>& AccountSettings::cloudShortcuts() const
{
    static const std::vector<cloud::Game> none;
    const Profile* p = activeProfile();
    return p ? p->cloudShortcuts : none;
}

void AccountSettings::setCloudShortcuts(std::vector<cloud::Game> shortcuts)
{
    ensureActiveProfile()->cloudShortcuts = std::move(shortcuts);
}

std::vector<cloud::Game> AccountSettings::readShortcuts(const toml::array* rows)
{
    std::vector<cloud::Game> out;
    if (!rows)
        return out;

    for (const auto& node : *rows) {
        const auto* t = node.as_table();
        if (!t) continue;

        cloud::Game g;
        g.productId = (*t)["product_id"].value<std::string>().value_or("");
        g.name = (*t)["name"].value<std::string>().value_or("");
        if (g.productId.empty() || g.name.empty()) continue;

        g.imageUrl = (*t)["image_url"].value<std::string>().value_or("");
        g.landscapeImageUrl = (*t)["landscape_image_url"].value<std::string>().value_or("");
        g.conceptId = (*t)["concept_id"].value<std::string>().value_or("");
        g.category = (*t)["category"].value<std::string>().value_or("");
        g.serviceType = (*t)["service_type"].value<std::string>().value_or("");
        g.platform = (*t)["platform"].value<std::string>().value_or("");
        g.isOwned = (*t)["is_owned"].value<bool>().value_or(false);
        g.streamServiceType = (*t)["stream_service_type"].value<std::string>().value_or("");
        g.streamIdentifier = (*t)["stream_identifier"].value<std::string>().value_or("");
        g.entitlementId = (*t)["entitlement_id"].value<std::string>().value_or("");
        g.storeProductId = (*t)["store_product_id"].value<std::string>().value_or("");
        g.conceptUrl = (*t)["concept_url"].value<std::string>().value_or("");
        g.plusCatalog = (*t)["plus_catalog"].value<bool>().value_or(false);
        out.push_back(std::move(g));
    }
    return out;
}

toml::array AccountSettings::shortcutRows(const std::vector<cloud::Game>& shortcuts)
{
    toml::array rows;
    for (const cloud::Game& g : shortcuts) {
        toml::table t;
        t.insert("product_id", g.productId);
        t.insert("name", g.name);
        if (!g.imageUrl.empty()) t.insert("image_url", g.imageUrl);
        if (!g.landscapeImageUrl.empty()) t.insert("landscape_image_url", g.landscapeImageUrl);
        if (!g.conceptId.empty()) t.insert("concept_id", g.conceptId);
        if (!g.category.empty()) t.insert("category", g.category);
        if (!g.serviceType.empty()) t.insert("service_type", g.serviceType);
        if (!g.platform.empty()) t.insert("platform", g.platform);
        if (g.isOwned) t.insert("is_owned", true);
        if (!g.streamServiceType.empty()) t.insert("stream_service_type", g.streamServiceType);
        if (!g.streamIdentifier.empty()) t.insert("stream_identifier", g.streamIdentifier);
        if (!g.entitlementId.empty()) t.insert("entitlement_id", g.entitlementId);
        if (!g.storeProductId.empty()) t.insert("store_product_id", g.storeProductId);
        if (!g.conceptUrl.empty()) t.insert("concept_url", g.conceptUrl);
        if (g.plusCatalog) t.insert("plus_catalog", true);
        rows.push_back(std::move(t));
    }
    return rows;
}

}
