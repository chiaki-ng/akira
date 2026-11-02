#ifndef AKIRA_PLAYERS_PANEL_VIEW_HPP
#define AKIRA_PLAYERS_PANEL_VIEW_HPP

#include <borealis.hpp>
#include <switch.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Host;
class InputManager;

namespace akira::stream {
class Connection;
}

class PlayersPanelView : public brls::Box
{
public:
    PlayersPanelView(Host* host, std::weak_ptr<akira::stream::Connection> connection, InputManager* input,
                     bool claimImmediately = false);
    ~PlayersPanelView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override;

    void setOnClosed(std::function<void()> cb) { m_on_closed = std::move(cb); }

    bool isTranslucent() override { return true; }

private:
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

    Host*          m_host    = nullptr;
    std::weak_ptr<akira::stream::Connection> m_connection;
    InputManager*  m_input   = nullptr;
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

#endif // AKIRA_PLAYERS_PANEL_VIEW_HPP
