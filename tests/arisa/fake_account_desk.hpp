#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "accounts/account_desk.hpp"

namespace tests {

class FakeAccountDesk final : public akira::accounts::AccountDesk {
public:
    FakeAccountDesk()
        : listed{ Entry{ .id = 1, .name = "kyogre_404", .kind = Kind::Remote, .active = true },
            Entry{ .id = 2, .name = "Couch guest", .kind = Kind::Legacy } }
        , shown{ .accountId = "aGVsbG93b3Js", .onlineId = "kyogre_404", .companionPort = 9295 }
        , card{ .name = "kyogre_404",
            .kind = Kind::Remote,
            .cloudKey = "akira/cloud/chip_ready",
            .plus = true,
            .trophies = Trophies{ .level = 412, .progress = 38, .platinum = 7, .gold = 61, .silver = 180, .bronze = 702 } }
    {
    }

    auto entries() const -> std::vector<Entry> override { return listed; }
    auto identity(bool revealed) const -> Identity override
    {
        Identity made = shown;
        if (!revealed)
            made.accountId = "****Js";
        return made;
    }
    auto credentials(bool revealed) const -> std::vector<Credential> override
    {
        return {
            Credential{ .secret = Secret::AccessToken, .shown = revealed ? "eyJhbGciOiJSUzI1NiJ9" : "****NiJ9" },
            Credential{ .secret = Secret::RefreshToken, .shown = "****a81f" },
            Credential{ .secret = Secret::TokenExpires, .shown = "2026-11-20 22:40:00" },
            Credential{ .secret = Secret::Npsso },
            Credential{ .secret = Secret::SsoAccessToken, .shown = "****77c2" },
            Credential{ .secret = Secret::SsoRefreshToken, .shown = "****0b19" },
            Credential{ .secret = Secret::SsoExpires, .shown = "2026-11-19 08:00:00", .expired = true },
            Credential{ .secret = Secret::Duid, .shown = "****c4e2" },
        };
    }
    void summary(std::function<void(Summary)> deliver) override { deliver(card); }

    void switchTo(std::int64_t id) override
    {
        switched.push_back(id);
        for (Entry& entry : listed)
            entry.active = entry.id == id;
    }
    auto signInExpired() const -> bool override { return expired; }
    void remove(std::int64_t id) override
    {
        std::erase_if(listed, [id](const Entry& entry) { return entry.id == id; });
    }
    auto setAccountId(const std::string& text) -> bool override
    {
        if (text.size() != 12)
            return false;
        shown.accountId = text;
        return true;
    }
    void setOnlineId(const std::string& text) override { shown.onlineId = text; }
    void setTrophies(bool enabled) override { shown.trophies = enabled; }
    auto setCompanionPort(int port) -> bool override
    {
        if (port <= 0 || port > 65535)
            return false;
        shown.companionPort = port;
        return true;
    }

    auto refreshState() const -> Refresh override { return refreshing; }
    void refresh(std::function<void(bool)> done) override
    {
        ++refreshes;
        done(true);
    }
    void clearPsn() override { ++cleared; }

    auto createLegacy(const std::string& accountId, const std::string& name) -> bool override
    {
        if (accountId.size() != 12)
            return false;
        listed.push_back(Entry{ .id = 3, .name = name, .kind = Kind::Legacy, .active = true });
        return true;
    }
    void signInWithQr(bool createProfile, Qr progress) override
    {
        creating = createProfile;
        qr = std::move(progress);
        qr(QrProgress{ .step = QrStep::Preparing });
    }
    void cancelSignIn() override { ++cancelledSignIns; }
    auto openPairing(bool createProfile, Paired step) -> Pairing override
    {
        creating = createProfile;
        paired = std::move(step);
        return renewPairing();
    }
    auto renewPairing() -> Pairing override
    {
        ++codes;
        return Pairing{ .code = codes == 1 ? "4821" : "7310", .address = "192.168.20.31", .port = shown.companionPort };
    }
    void closePairing() override { ++closedPairings; }

    std::vector<Entry> listed;
    Identity shown;
    Summary card;
    Refresh refreshing{ .state = RefreshState::Ready };
    Qr qr;
    Paired paired;
    std::vector<std::int64_t> switched;
    bool expired = false;
    bool creating = false;
    int refreshes = 0;
    int cleared = 0;
    int cancelledSignIns = 0;
    int closedPairings = 0;
    int codes = 0;
};

}
