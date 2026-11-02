#include "views/players_panel_view.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/host.hpp"
#include "core/settings_manager.hpp"
#include "core/trophy_manager.hpp"
#include "stream/connection.hpp"
#include "stream/input_manager.hpp"
#include "input/pad_path.hpp"
#include "ui/glass.hpp"
#include "ui/theme.hpp"

using akira::input::PadDescription;
using akira::stream::Connection;
using akira::stream::CouchSeat;
using akira::stream::SeatPhase;

namespace {

constexpr int kSecondarySlots = Connection::seatCount - 1;
constexpr float kPanelWidth = 680.0f;
constexpr float kContentWidth = 644.0f;

NVGcolor SlotLed(int player)
{
    switch (player) {
        case 1:  return nvgRGB(0x4a, 0x9e, 0xff);
        case 2:  return nvgRGB(0xff, 0x5a, 0x5a);
        case 3:  return nvgRGB(0x5e, 0xe6, 0xa0);
        case 4:  return nvgRGB(0xf0, 0x72, 0xd0);
        default: return nvgRGB(0x66, 0x66, 0x66);
    }
}

brls::Box* MakeRowShell(bool empty)
{
    const auto& p = akira::ui::active();
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setWidth(kContentWidth);
    row->setHeight(70);
    row->setPaddingLeft(14);
    row->setPaddingRight(14);
    row->setCornerRadius(14);
    row->setMarginBottom(9);
    row->setBackgroundColor(akira::ui::withAlpha(
        empty ? p.backgroundDeep : p.surface, empty ? 0x78 : 0x68));
    row->setBorderThickness(1.0f);
    row->setBorderColor(empty ? akira::ui::withAlpha(p.textDim, 0x32) : p.surfaceLine);
    return row;
}

brls::Box* MakeLed(int player, bool empty)
{
    const auto& p = akira::ui::active();
    auto* led = new brls::Box();
    led->setAxis(brls::Axis::COLUMN);
    led->setJustifyContent(brls::JustifyContent::CENTER);
    led->setAlignItems(brls::AlignItems::CENTER);
    led->setWidth(38);
    led->setHeight(38);
    led->setCornerRadius(11);
    led->setMarginRight(13);
    if (empty) {
        led->setBackgroundColor(akira::ui::withAlpha(p.surface, 0x54));
        led->setBorderThickness(1.0f);
        led->setBorderColor(p.surfaceLine);
    } else {
        led->setBackgroundColor(SlotLed(player));
    }

    auto* n = new brls::Label();
    n->setText(std::to_string(player));
    n->setFontSize(17);
    n->setTextColor(empty ? nvgRGB(0x6c, 0x6c, 0x76) : nvgRGB(0x0b, 0x0d, 0x12));
    led->addView(n);
    return led;
}

brls::Box* MakeBody(const std::string& dev, const std::string& meta, bool dim, brls::Label** outMeta = nullptr)
{
    const auto& p = akira::ui::active();
    auto* body = new brls::Box();
    body->setAxis(brls::Axis::COLUMN);
    body->setJustifyContent(brls::JustifyContent::CENTER);
    body->setGrow(1.0f);

    auto* devLabel = new brls::Label();
    devLabel->setText(dev);
    devLabel->setFontSize(19);
    devLabel->setTextColor(dim ? p.textDim : p.text);
    body->addView(devLabel);

    auto* metaLabel = new brls::Label();
    metaLabel->setText(meta);
    metaLabel->setFontSize(14);
    metaLabel->setTextColor(p.textMuted);
    metaLabel->setMarginTop(3);
    body->addView(metaLabel);

    if (outMeta)
        *outMeta = metaLabel;

    return body;
}

NVGcolor ProfileAccent(std::size_t index)
{
    const auto& p = akira::ui::active();
    switch (index % 4) {
        case 1: return p.media;
        case 2: return p.success;
        case 3: return p.warning;
        default: return p.accent;
    }
}

class ProfileAvatar final : public brls::View
{
public:
    explicit ProfileAvatar(NVGcolor accent) : m_accent(accent) {}

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override
    {
        (void)style;
        (void)ctx;
        const auto& p = akira::ui::active();
        const float r = std::min(width, height) * 0.5f;
        const float cx = x + width * 0.5f;
        const float cy = y + height * 0.5f;

        NVGpaint fill = nvgLinearGradient(vg, x, y, x + width, y + height,
            akira::ui::withAlpha(m_accent, 0xd8),
            akira::ui::withAlpha(p.backgroundDeep, 0xf0));
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgFillPaint(vg, fill);
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgCircle(vg, cx, y + height * 0.36f, height * 0.13f);
        nvgFillColor(vg, akira::ui::withAlpha(p.text, 0xe8));
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + width * 0.27f, y + height * 0.55f,
            width * 0.46f, height * 0.23f, height * 0.115f);
        nvgFillColor(vg, akira::ui::withAlpha(p.text, 0xe8));
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r - 0.5f);
        nvgStrokeColor(vg, akira::ui::withAlpha(p.focusB, 0x70));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }

private:
    NVGcolor m_accent;
};

brls::Button* MakeAction(const std::string& text, NVGcolor color)
{
    auto* btn = new brls::Button();
    btn->setStyle(&brls::BUTTONSTYLE_BORDERLESS);
    btn->setText(text);
    btn->setTextColor(color);
    btn->setHeight(44);
    btn->setCornerRadius(9);
    return btn;
}

NVGcolor MetaColor(SeatPhase phase)
{
    switch (phase) {
        case SeatPhase::Playing:  return nvgRGB(0x8a, 0xd6, 0xa8);
        case SeatPhase::Failed:   return nvgRGB(0xff, 0xa8, 0xa8);
        case SeatPhase::Passcode: return nvgRGB(0xf2, 0xd0, 0x7a);
        default:                  return nvgRGB(0x9a, 0x9a, 0xa4);
    }
}

bool NeedsAnotherAccount(const CouchSeat& seat)
{
    return seat.phase == SeatPhase::Failed && seat.refusal &&
           seat.refusal->kind() == libchiaking::play::SeatRefusal::Kind::account_not_on_console;
}

std::string FailureText(const CouchSeat& seat)
{
    using Kind = libchiaking::play::SeatRefusal::Kind;
    if (seat.dropped)
        return "The console released this pad";
    if (!seat.refusal)
        return "Console refused the join";
    switch (seat.refusal->kind()) {
        case Kind::timed_out:
            return "No answer from the console";
        case Kind::account_not_on_console:
            return "Account is not registered on this PS5 - choose another profile";
        case Kind::passcode_attempts_used:
            return "Too many wrong passcodes";
        case Kind::not_sent:
            return "The join could not be sent";
        case Kind::no_free_seat:
            return "The console has no free seat";
        case Kind::refused_by_console:
        default:
            return "Console refused the join (status " + std::to_string((int)seat.refusal->console_status().value_or(0)) + ")";
    }
}

std::string MetaText(int player, const CouchSeat& seat)
{
    switch (seat.phase) {
        case SeatPhase::Playing:
            return "Player " + std::to_string(player) + " - ready";
        case SeatPhase::Passcode:
            return "Waiting for the console login passcode";
        case SeatPhase::Failed:
            return FailureText(seat);
        case SeatPhase::Joining:
        default:
            return "Joining...";
    }
}

std::string WithAccount(const std::string& text, const Host* host, const Connection* connection, uint8_t slot)
{
    if (!host || !host->isPS5())
        return text;
    std::string account;
    if (slot == 0) {
        if (const Profile* active = SettingsManager::getInstance()->getActiveProfile(); active && !active->accountId.empty())
            account = active->label();
    } else if (connection) {
        account = connection->couchSeat(slot).accountLabel;
    }
    if (account.empty())
        return text + " - no PSN account configured";
    return text + " - PSN: " + SettingsManager::getInstance()->maskAccountName(account);
}

} // namespace

PlayersPanelView::PlayersPanelView(Host* host, std::weak_ptr<Connection> connection, InputManager* input,
                                   bool claimImmediately)
    : m_host(host)
    , m_connection(std::move(connection))
    , m_input(input)
{
    setAxis(brls::Axis::COLUMN);
    setJustifyContent(brls::JustifyContent::CENTER);
    setAlignItems(brls::AlignItems::CENTER);
    setGrow(1.0f);
    setWidthPercentage(100.0f);
    setHeightPercentage(100.0f);
    setBackgroundColor(nvgRGBA(0, 0, 0, 0));

    m_panel = new brls::Box();
    m_panel->setAxis(brls::Axis::COLUMN);
    m_panel->setWidth(kPanelWidth);
    m_panel->setPaddingTop(22);
    m_panel->setPaddingBottom(16);
    m_panel->setPaddingLeft(18);
    m_panel->setPaddingRight(18);
    m_panel->setCornerRadius(20);
    m_panel->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
    addView(m_panel);

    auto* header = new brls::Box();
    header->setAxis(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setMarginBottom(2);
    m_panel->addView(header);

    m_title = new brls::Label();
    m_title->setText("Players");
    m_title->setFontSize(26);
    m_title->setTextColor(akira::ui::active().text);
    m_title->setMarginRight(10);
    header->addView(m_title);

    m_count = new brls::Label();
    m_count->setFontSize(15);
    m_count->setTextColor(akira::ui::active().accent);
    header->addView(m_count);

    m_sub = new brls::Label();
    m_sub->setFontSize(14);
    m_sub->setTextColor(akira::ui::active().textMuted);
    m_sub->setMarginBottom(14);
    m_panel->addView(m_sub);

    auto* rule = new brls::Box();
    rule->setWidthPercentage(100.0f);
    rule->setHeight(1);
    rule->setBackgroundColor(akira::ui::active().surfaceLine);
    rule->setMarginBottom(14);
    m_panel->addView(rule);

    m_slots = new brls::Box();
    m_slots->setAxis(brls::Axis::COLUMN);
    m_panel->addView(m_slots);

    auto* footer = new brls::Box();
    footer->setAxis(brls::Axis::ROW);
    footer->setAlignItems(brls::AlignItems::CENTER);
    footer->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    footer->setWidthPercentage(100.0f);
    footer->setMarginTop(8);
    m_foot = new brls::Label();
    m_foot->setFontSize(13);
    m_foot->setTextColor(akira::ui::active().textDim);
    m_foot->setGrow(1.0f);
    footer->addView(m_foot);

    auto* hints = new brls::Hints();
    hints->setHintFontSizes(21.0f, 15.0f);
    footer->addView(hints);
    m_panel->addView(footer);

    if (m_input) {
        auto alive = m_callback_alive;
        m_input->setOnCouchJoined([this, alive](HidNpadIdType npad, uint8_t slot) {
            brls::sync([this, alive, npad, slot]() {
                if (alive->load(std::memory_order_acquire))
                    onJoined(npad, slot);
            });
        });
    }

    registerAction("Back", brls::ControllerButton::BUTTON_B,
        [this](brls::View*) {
            if (m_claiming) {
                stopClaim();
                rebuild(true);
                return true;
            }
            if (m_selectingProfile) {
                m_selectingProfile = false;
                m_profileRetry = false;
                m_profileRejected = false;
                m_profileSlot = 0;
                m_pendingProfileId = 0;
                rebuild(true);
                return true;
            }
            if (m_input)
                m_input->cancelCouchJoin();
            brls::Application::popActivity(brls::TransitionAnimation::NONE);
            return true;
        });

    rebuild(false);

    if (claimImmediately)
        startAddPlayer();
}

PlayersPanelView::~PlayersPanelView()
{
    m_callback_alive->store(false, std::memory_order_release);
    if (m_input) {
        m_input->cancelCouchJoin();
        m_input->setOnCouchJoined(nullptr);
    }

    if (m_on_closed)
        m_on_closed();
}

int PlayersPanelView::rosterCount() const
{
    return m_input ? (int)m_input->couchRoster().size() : 0;
}

const char* PlayersPanelView::deviceLabelFor(HidNpadIdType npad) const
{
    if (!m_input)
        return "Controller";
    for (const auto& d : m_input->describePads()) {
        if (d.npad == npad)
            return d.label;
    }
    return "Controller";
}

void PlayersPanelView::populateRoster()
{
    const char* primaryLabel = "Player 1";
    if (m_input) {
        for (const auto& d : m_input->describePads()) {
            if (m_input->CouchPadIndexForController(d.npad) == 0) {
                primaryLabel = d.label;
                break;
            }
        }
    }

    auto* primary = MakeRowShell(false);
    primary->setBorderColor(nvgRGB(0x35, 0x5a, 0x8a));
    primary->addView(MakeLed(1, false));
    const auto connection = m_connection.lock();
    primary->addView(MakeBody(primaryLabel, WithAccount("Player 1 - ready", m_host, connection.get(), 0), false));
    m_slots->addView(primary);

    std::vector<std::pair<HidNpadIdType, uint8_t>> ordered;
    if (m_input) {
        const auto roster = m_input->couchRoster();
        for (const auto& entry : roster)
            ordered.emplace_back(entry.first, entry.second);
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    for (const auto& entry : ordered) {
        HidNpadIdType npad = entry.first;
        uint8_t slot = entry.second;
        const int player = slot + 1;
        const CouchSeat seat = connection ? connection->couchSeat(slot) : CouchSeat{};

        auto* row = MakeRowShell(false);
        if (seat.phase == SeatPhase::Failed)
            row->setBorderColor(nvgRGB(0x7a, 0x3a, 0x3a));
        row->addView(MakeLed(player, false));

        brls::Label* meta = nullptr;
        row->addView(MakeBody(deviceLabelFor(npad),
                              WithAccount(MetaText(player, seat), m_host, connection.get(), slot),
                              false, &meta));
        if (meta)
            meta->setTextColor(MetaColor(seat.phase));

        brls::View* action = nullptr;
        if (NeedsAnotherAccount(seat)) {
            auto* choose = MakeAction("Choose profile", nvgRGB(0x16, 0xbf, 0xe0));
            choose->registerClickAction([this, slot](brls::View*) {
                startProfileSelection(slot, true);
                return true;
            });
            row->addView(choose);
            action = choose;
        } else if (seat.phase == SeatPhase::Failed) {
            auto* retry = MakeAction("Retry", nvgRGB(0x16, 0xbf, 0xe0));
            retry->registerClickAction([this, slot](brls::View*) {
                retryPlayer(slot);
                return true;
            });
            row->addView(retry);
            action = retry;
        }

        auto* leave = MakeAction("Leave", nvgRGB(0xff, 0xb4, 0xb4));
        leave->registerClickAction([this, npad](brls::View*) {
            leaveSlot(npad);
            return true;
        });
        row->addView(leave);
        if (!action)
            action = leave;

        if (!m_first)
            m_first = action;

        m_rows.push_back({ slot, row, meta, action });
        m_slots->addView(row);
    }

    uint8_t lowestOpen = Connection::seatCount;
    for (uint8_t slot = 1; slot < Connection::seatCount; slot++) {
        const bool used = std::any_of(ordered.begin(), ordered.end(),
            [slot](const auto& entry) { return entry.second == slot; });
        if (!used) {
            lowestOpen = slot;
            break;
        }
    }

    for (uint8_t slot = 1; slot < Connection::seatCount; slot++) {
        const bool used = std::any_of(ordered.begin(), ordered.end(),
            [slot](const auto& entry) { return entry.second == slot; });
        if (used)
            continue;
        const int player = slot + 1;
        auto* row = MakeRowShell(true);
        row->addView(MakeLed(player, true));
        const bool hasProfiles = !m_host || !m_host->isPS5() || (connection && !connection->couchProfileChoices(slot).empty());
        const std::string emptyMeta = hasProfiles
            ? (slot == lowestOpen ? "Invite a player to this slot" : "Waiting for the previous slot")
            : "No other Akira profile has a PSN account ID";
        row->addView(MakeBody("Open slot", emptyMeta, true));

        if (hasProfiles && slot == lowestOpen) {
            auto* add = MakeAction("Add player", nvgRGB(0x16, 0xbf, 0xe0));
            add->registerClickAction([this](brls::View*) {
                startAddPlayer();
                return true;
            });
            if (!m_first)
                m_first = add;
            if (!m_addAction)
                m_addAction = add;
            row->addView(add);
        }

        m_slots->addView(row);
    }
}

void PlayersPanelView::populateProfilePicker()
{
    const auto connection = m_connection.lock();
    const auto choices = connection ? connection->couchProfileChoices(m_profileSlot)
                                    : std::vector<akira::stream::CouchProfileChoice>{};
    if (choices.empty()) {
        auto* row = MakeRowShell(true);
        row->addView(MakeBody("No eligible profiles",
            "Add another PSN profile to Akira first", true));
        m_slots->addView(row);
        return;
    }

    const float cardWidth = choices.size() <= 3 ? 184.0f : 145.0f;
    brls::Box* profileRow = nullptr;
    for (std::size_t i = 0; i < choices.size(); i++) {
        if ((i % 4) == 0) {
            profileRow = new brls::Box();
            profileRow->setAxis(brls::Axis::ROW);
            profileRow->setJustifyContent(brls::JustifyContent::CENTER);
            profileRow->setAlignItems(brls::AlignItems::CENTER);
            profileRow->setWidth(kContentWidth);
            if (i + 4 < choices.size())
                profileRow->setMarginBottom(10);
            m_slots->addView(profileRow);
        }

        const auto& choice = choices[i];
        auto* tile = new brls::Box();
        tile->setAxis(brls::Axis::COLUMN);
        tile->setAlignItems(brls::AlignItems::CENTER);
        tile->setJustifyContent(brls::JustifyContent::CENTER);
        tile->setWidth(cardWidth);
        tile->setHeight(138);
        tile->setMarginLeft(7);
        tile->setMarginRight(7);
        tile->setCornerRadius(18);
        tile->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
        tile->setFocusable(true);
        tile->setHideHighlightBackground(true);
        tile->setHighlightPadding(4);

        tile->registerClickAction([this, profileId = choice.profileId](brls::View*) {
            selectProfile(profileId);
            return true;
        });

        auto* avatarShell = new brls::Box();
        avatarShell->setWidth(68);
        avatarShell->setHeight(68);
        avatarShell->setMarginBottom(9);

        auto* fallback = new ProfileAvatar(ProfileAccent(i));
        fallback->setWidth(68);
        fallback->setHeight(68);
        avatarShell->addView(fallback);

        auto* avatar = new brls::Image();
        avatar->setPositionType(brls::PositionType::ABSOLUTE);
        avatar->setPositionTop(0);
        avatar->setPositionLeft(0);
        avatar->setWidth(68);
        avatar->setHeight(68);
        avatar->setCornerRadius(34);
        avatar->setScalingType(brls::ImageScalingType::FILL);
        avatarShell->addView(avatar);
        tile->addView(avatarShell);
        m_profileAvatars.push_back(avatar);
        loadProfileAvatar(choice.profileId, choice.accountId, choice.avatarUrl, avatar);

        auto* name = new brls::Label();
        name->setText(SettingsManager::getInstance()->maskAccountName(choice.label));
        name->setFontSize(cardWidth < 170.0f ? 16 : 18);
        name->setTextColor(akira::ui::active().text);
        name->setMarginBottom(0);
        tile->addView(name);
        if (!m_first)
            m_first = tile;
        m_profileTiles.push_back(tile);
        profileRow->addView(tile);
    }
}

void PlayersPanelView::loadProfileAvatarUrl(int64_t profileId, const std::string& avatarUrl,
                                            brls::Image* image)
{
    if (avatarUrl.empty() || !image)
        return;

    auto alive = m_callback_alive;
    TrophyManager::getInstance()->fetchIcon(avatarUrl,
        [this, alive, image, profileId](const std::string&, const std::vector<uint8_t>& bytes) {
            if (!alive->load(std::memory_order_acquire) || bytes.empty())
                return;
            if (std::find(m_profileAvatars.begin(), m_profileAvatars.end(), image) == m_profileAvatars.end())
                return;
            image->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));
            brls::Logger::debug("Loaded cached PSN avatar for profile {}", profileId);
        });
}

void PlayersPanelView::loadProfileAvatar(int64_t profileId, const std::string& accountId,
                                         const std::string& avatarUrl, brls::Image* image)
{
    if (!avatarUrl.empty())
    {
        loadProfileAvatarUrl(profileId, avatarUrl, image);
        return;
    }
    if (accountId.empty() || !image)
        return;

    auto alive = m_callback_alive;
    TrophyManager::getInstance()->fetchProfileForAccount(accountId,
        [this, alive, image, profileId](const TrophyManager::Profile& profile) {
            if (!alive->load(std::memory_order_acquire))
                return;
            const std::string url = profile.avatar_url.value_or("");
            if (url.empty())
                return;

            SettingsManager* settings = SettingsManager::getInstance();
            if (Profile* saved = settings->findProfile(profileId); saved && saved->avatarUrl != url)
            {
                saved->avatarUrl = url;
                settings->writeFile();
            }
            loadProfileAvatarUrl(profileId, url, image);
        },
        [](psn::Status, const std::string&) {});
}

void PlayersPanelView::populatePicker()
{
    std::vector<PadDescription> candidates;
    if (m_input) {
        const auto pads = m_input->describePads();
        const auto couchRoster = m_input->couchRoster();
        std::string roster;
        for (const auto& entry : couchRoster)
            roster += " " + std::to_string((int)entry.first) + "->" + std::to_string((int)entry.second);
        brls::Logger::info("Couch picker: {} pad(s) listed, bound npad {}, roster:{}",
                           pads.size(), (int)m_input->boundNpad(),
                           roster.empty() ? " (empty)" : roster);

        for (const auto& d : pads) {
            const bool claimable = m_input->isClaimableNpad(d.npad);
            const bool inRoster = couchRoster.find(d.npad) != couchRoster.end();
            brls::Logger::info("Couch picker: npad {} '{}' claimable={} inRoster={}",
                               (int)d.npad, d.label, claimable, inRoster);
            if (!claimable || inRoster)
                continue;
            candidates.push_back(d);
        }
    }

    if (candidates.empty()) {
        auto* row = MakeRowShell(true);
        row->addView(MakeBody("No controllers waiting", "Connect a pad, then hold ZL + ZR", true));
        m_slots->addView(row);
        return;
    }

    for (const auto& c : candidates) {
        auto* row = MakeRowShell(false);
        row->setBorderColor(nvgRGB(0x16, 0xbf, 0xe0));
        brls::Label* meta = nullptr;
        row->addView(MakeBody(c.label, "Hold ZL + ZR to claim", false, &meta));
        if (!m_claimMeta)
            m_claimMeta = meta;
        m_slots->addView(row);
    }
}

void PlayersPanelView::refreshStates()
{
    const auto connection = m_connection.lock();
    for (const auto& r : m_rows) {
        if (!r.meta || !m_host || !connection)
            continue;
        const CouchSeat& seat = connection->couchSeat(r.slot);
        const std::string text = WithAccount(MetaText(r.slot + 1, seat), m_host, connection.get(), r.slot);
        if (r.meta->getFullText() != text) {
            r.meta->setText(text);
            r.meta->setTextColor(MetaColor(seat.phase));
        }
    }

    if (m_claiming && m_claimMeta && m_input) {
        const float progress = m_input->couchClaimProgress();
        const std::string text = progress > 0.0f
            ? "Claiming... " + std::to_string((int)(progress * 100.0f)) + "%"
            : std::string("Hold ZL + ZR to claim");
        if (m_claimMeta->getFullText() != text)
            m_claimMeta->setText(text);
    }
}

void PlayersPanelView::rebuild(bool refocus)
{
    bool focusInside = false;
    for (brls::View* v = brls::Application::getCurrentFocus(); v; v = v->getParent()) {
        if (v == m_slots) {
            focusInside = true;
            break;
        }
    }

    if (focusInside) {
        m_focusSlot = -1;
        brls::View* focused = brls::Application::getCurrentFocus();
        for (const auto& r : m_rows) {
            for (brls::View* v = focused; v; v = v->getParent()) {
                if (v == r.row) {
                    m_focusSlot = (int)r.slot;
                    break;
                }
            }
            if (m_focusSlot >= 0)
                break;
        }
    }

    m_slots->clearViews();
    m_first = nullptr;
    m_claimMeta = nullptr;
    m_addAction = nullptr;
    m_rows.clear();
    m_profileTiles.clear();
    m_profileAvatars.clear();

    const int total = 1 + rosterCount();
    m_count->setText(std::to_string(total) + " / " + std::to_string(Connection::seatCount));
    m_count->setVisibility(brls::Visibility::VISIBLE);
    m_title->setText("Players");

    if (m_selectingProfile) {
        m_title->setText("Add player");
        m_count->setVisibility(brls::Visibility::GONE);
        m_sub->setText(m_profileRejected
            ? "That account is not registered on this PS5. Choose another Akira profile registered on the console."
            : "Choose a profile for Player " + std::to_string(m_profileSlot + 1));
        m_foot->setText("The profile must also be signed in on the connected console.");
        populateProfilePicker();
    } else if (m_claiming) {
        m_title->setText("Choose a controller");
        m_count->setVisibility(brls::Visibility::GONE);
        m_sub->setText("Hold ZL + ZR on the controller you want to add");
        m_foot->setText("Press B to cancel. A connected pad does nothing until it claims a slot.");
        populatePicker();
    } else if (rosterCount() >= kSecondarySlots) {
        m_sub->setText("Four players seated - the session is at capacity.");
        m_foot->setText("");
        populateRoster();
    } else {
        m_sub->setText("Everyone shares this screen. Add a pad by acting on it.");
        m_foot->setText("Add player, then the new player holds ZL + ZR on their pad to claim.");
        populateRoster();
    }

    if (!refocus && !focusInside)
        return;

    brls::View* target = nullptr;
    if (m_focusAddNext) {
        m_focusAddNext = false;
        target = m_addAction;
    }
    if (!target && m_focusSlot >= 0) {
        for (const auto& r : m_rows) {
            if ((int)r.slot == m_focusSlot) {
                target = r.action;
                break;
            }
        }
    }
    if (!target)
        target = m_first;
    if (target)
        brls::Application::giveFocus(target);
}

void PlayersPanelView::startAddPlayer()
{
    if (!m_input || rosterCount() >= kSecondarySlots)
        return;
    if (!m_host || !m_host->isPS5()) {
        startClaim();
        return;
    }

    uint8_t slot = 1;
    const auto roster = m_input->couchRoster();
    for (; slot < Connection::seatCount; slot++) {
        const bool used = std::any_of(roster.begin(), roster.end(),
            [slot](const auto& entry) { return entry.second == slot; });
        if (!used)
            break;
    }
    if (slot < Connection::seatCount)
        startProfileSelection(slot, false);
}

void PlayersPanelView::startClaim()
{
    if (m_claiming || !m_input)
        return;
    if (rosterCount() >= kSecondarySlots)
        return;
    m_claiming = true;
    m_input->beginCouchJoin();
    rebuild(true);
}

void PlayersPanelView::stopClaim()
{
    m_claiming = false;
    if (m_input)
        m_input->cancelCouchJoin();
}

void PlayersPanelView::startProfileSelection(uint8_t slot, bool retry)
{
    if (!m_host || !m_host->isPS5() || slot == 0 || slot >= Connection::seatCount)
        return;
    stopClaim();
    m_selectingProfile = true;
    m_profileRetry = retry;
    const auto connection = m_connection.lock();
    m_profileRejected = retry && connection && NeedsAnotherAccount(connection->couchSeat(slot));
    m_profileSlot = slot;
    m_pendingProfileId = 0;
    rebuild(true);
}

void PlayersPanelView::selectProfile(int64_t profileId)
{
    if (!m_selectingProfile || !m_host)
        return;

    if (m_profileRetry) {
        const auto connection = m_connection.lock();
        if (!connection)
            return;
        connection->removePlayer(m_profileSlot);
        if (!connection->addPlayer(m_profileSlot, profileId))
            return;
        m_selectingProfile = false;
        m_profileRetry = false;
        m_profileRejected = false;
        rebuild(true);
        return;
    }

    m_pendingProfileId = profileId;
    m_selectingProfile = false;
    m_profileRejected = false;
    startClaim();
}

void PlayersPanelView::onJoined(HidNpadIdType npad, uint8_t slot)
{
    m_claiming = false;
    m_focusSlot = -1;
    m_focusAddNext = true;
    const auto connection = m_connection.lock();
    const bool ps5 = m_host && m_host->isPS5();
    const std::optional<int64_t> profile = ps5 ? std::optional<int64_t>(m_pendingProfileId) : std::nullopt;
    if (!connection || !connection->addPlayer(slot, profile)) {
        if (m_input)
            m_input->couchLeave(npad);
        if (ps5)
            startProfileSelection(slot, false);
        else
            rebuild(true);
        return;
    }
    m_pendingProfileId = 0;
    rebuild(true);
}

void PlayersPanelView::leaveSlot(HidNpadIdType npad)
{
    if (!m_input)
        return;

    const uint8_t slot = m_input->CouchPadIndexForController(npad);

    if (slot != InputManager::kNoCouchSlot && slot != 0) {
        uint8_t addr[6];
        if (m_input->couchSlotOutputTarget(slot, addr, nullptr, nullptr))
            m_input->extendedInput().unregisterCouchOutput(addr);
    }

    m_input->couchLeave(npad);
    const auto connection = m_connection.lock();
    if (connection && slot != InputManager::kNoCouchSlot && slot != 0)
        connection->removePlayer(slot);

    rebuild(true);
}

void PlayersPanelView::retryPlayer(uint8_t slot)
{
    const auto connection = m_connection.lock();
    if (!connection)
        return;
    const int64_t profileId = connection->couchSeat(slot).profileId;
    connection->removePlayer(slot);
    const bool ps5 = m_host && m_host->isPS5();
    if (!connection->addPlayer(slot, ps5 ? std::optional<int64_t>(profileId) : std::nullopt) && ps5)
        startProfileSelection(slot, true);
    rebuild(true);
}

std::string PlayersPanelView::structureSignature() const
{
    std::string sig = m_selectingProfile ? "p" : (m_claiming ? "c" : "r");
    if (!m_input)
        return sig;

    std::vector<std::pair<HidNpadIdType, uint8_t>> ordered;
    const auto roster = m_input->couchRoster();
    for (const auto& entry : roster)
        ordered.emplace_back(entry.first, entry.second);
    std::sort(ordered.begin(), ordered.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    const auto connection = m_connection.lock();
    for (const auto& entry : ordered) {
        sig += ":" + std::to_string((int)entry.first) + "-" + std::to_string(entry.second);
        if (connection) {
            const CouchSeat& seat = connection->couchSeat(entry.second);
            if (NeedsAnotherAccount(seat))
                sig += "N";
            else if (seat.phase == SeatPhase::Failed)
                sig += "F";
        }
    }

    if (m_claiming) {
        const auto couchRoster = m_input->couchRoster();
        for (const auto& d : m_input->describePads()) {
            if (!m_input->isClaimableNpad(d.npad))
                continue;
            if (couchRoster.find(d.npad) != couchRoster.end())
                continue;
            sig += "/" + std::to_string((int)d.npad);
        }
    }
    return sig;
}

void PlayersPanelView::draw(NVGcontext* vg, float x, float y, float width, float height,
                            brls::Style style, brls::FrameContext* ctx)
{
    if (m_claiming && m_input)
        m_input->tickCouchClaim();

    const std::string sig = structureSignature();
    if (sig != m_signature) {
        m_signature = sig;
        rebuild(false);
    }

    refreshStates();

    const auto& p = akira::ui::active();
    NVGpaint scrim = nvgLinearGradient(vg, x, y, x, y + height,
        akira::ui::withAlpha(p.backgroundDeep, 0x70),
        akira::ui::withAlpha(p.gradientBottom, 0xc8));
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillPaint(vg, scrim);
    nvgFill(vg);

    if (m_panel && m_panel->getWidth() > 0.0f && m_panel->getHeight() > 0.0f)
        akira::ui::drawGlassSurface(vg, m_panel->getX(), m_panel->getY(),
            m_panel->getWidth(), m_panel->getHeight(), 20.0f, 1.0f, false, p);

    for (auto* tile : m_profileTiles) {
        if (!tile || tile->getWidth() <= 0.0f || tile->getHeight() <= 0.0f)
            continue;

        bool focused = false;
        for (brls::View* view = brls::Application::getCurrentFocus(); view; view = view->getParent()) {
            if (view == tile) {
                focused = true;
                break;
            }
        }
        akira::ui::drawGlassSurface(vg, tile->getX(), tile->getY(),
            tile->getWidth(), tile->getHeight(), 18.0f, 0.94f, focused, p);
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

brls::View* PlayersPanelView::getDefaultFocus()
{
    return m_first ? m_first : this;
}
