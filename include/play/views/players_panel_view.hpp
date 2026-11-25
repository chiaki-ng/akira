#ifndef AKIRA_PLAYERS_PANEL_VIEW_HPP
#define AKIRA_PLAYERS_PANEL_VIEW_HPP

#include <borealis.hpp>
#include <switch.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace akira::consoles {
class Host;
}

namespace akira::play {
class Connection;
class CouchSeats;
struct CouchSeat;
enum class SeatPhase : uint8_t;
}

namespace akira::accounts {
class AccountSettings;
}

namespace akira::app {
class AppSettings;
}

namespace akira::settings {
class SettingsStore;
}

namespace akira::trophies {
class TrophyManager;
}

namespace akira::play::views
{

class PlayersPanelView : public brls::Box
{
public:
    struct Profiles
    {
        accounts::AccountSettings& accounts;
        const app::AppSettings& app;
        settings::SettingsStore& store;
        trophies::TrophyManager& trophies;
    };

    PlayersPanelView(Profiles profiles, consoles::Host* host, std::weak_ptr<play::Connection> connection,
                     CouchSeats* input, bool claimImmediately = false);
    ~PlayersPanelView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override;

    void setOnClosed(std::function<void()> cb) { m_on_closed = std::move(cb); }

    bool isTranslucent() override { return true; }

private:
    class ProfileAvatar;

    static const int kSecondarySlots;
    static constexpr float kPanelWidth = 680.0f;
    static constexpr float kContentWidth = 644.0f;

    static NVGcolor SlotLed(int player);
    static brls::Box* MakeRowShell(bool empty);
    static brls::Box* MakeLed(int player, bool empty);
    static brls::Box* MakeBody(const std::string& dev, const std::string& meta, bool dim,
                               brls::Label** outMeta = nullptr);
    static NVGcolor ProfileAccent(std::size_t index);
    static brls::Button* MakeAction(const std::string& text, NVGcolor color);
    static NVGcolor MetaColor(play::SeatPhase phase);
    static bool NeedsAnotherAccount(const play::CouchSeat& seat);
    static std::string FailureText(const play::CouchSeat& seat);
    static std::string MetaText(int player, const play::CouchSeat& seat);
    static std::string WithAccount(const std::string& text, const consoles::Host* host,
                                   const play::Connection* connection, uint8_t slot,
                                   const accounts::AccountSettings& accounts, const app::AppSettings& app);

    void rebuild(bool refocus);
    void startAddPlayer();
    void startClaim();
    void stopClaim();
    void startProfileSelection(uint8_t slot, bool retry);
    void selectProfile(int64_t profileId);
    void onJoined(HidNpadIdType npad, uint8_t slot);
    void leaveSlot(HidNpadIdType npad);
    void retryPlayer(uint8_t slot);

    void populateRoster();
    void populatePicker();
    void populateProfilePicker();
    void loadProfileAvatar(int64_t profileId, const std::string& accountId,
        const std::string& avatarUrl, brls::Image* image);
    void loadProfileAvatarUrl(int64_t profileId, const std::string& avatarUrl,
        brls::Image* image);
    void refreshStates();
    std::string structureSignature() const;
    const char* deviceLabelFor(HidNpadIdType npad) const;
    int rosterCount() const;

    Profiles                 m_profiles;
    consoles::Host*          m_host    = nullptr;
    std::weak_ptr<play::Connection> m_connection;
    CouchSeats* m_input = nullptr;
    brls::Box*     m_panel   = nullptr;
    brls::Box*     m_slots   = nullptr;
    brls::Label*   m_title   = nullptr;
    brls::Label*   m_count   = nullptr;
    brls::Label*   m_sub     = nullptr;
    brls::Label*   m_foot    = nullptr;
    brls::View*    m_first   = nullptr;
    bool           m_claiming = false;
    bool           m_selectingProfile = false;
    bool           m_profileRetry = false;
    bool           m_profileRejected = false;
    uint8_t        m_profileSlot = 0;
    int64_t        m_pendingProfileId = 0;
    std::string    m_signature;

    struct SlotRow {
        uint8_t      slot   = 0;
        brls::Box*   row    = nullptr;
        brls::Label* meta   = nullptr;
        brls::View*  action = nullptr;
    };
    std::vector<SlotRow> m_rows;
    std::vector<brls::Box*> m_profileTiles;
    std::vector<brls::Image*> m_profileAvatars;
    brls::Label*   m_claimMeta = nullptr;
    brls::View*    m_addAction = nullptr;
    int            m_focusSlot = -1;
    bool           m_focusAddNext = false;
    std::function<void()> m_on_closed;
    std::shared_ptr<std::atomic<bool>> m_callback_alive =
        std::make_shared<std::atomic<bool>>(true);
};


}

#endif // AKIRA_PLAYERS_PANEL_VIEW_HPP
