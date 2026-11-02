#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <libchiaking/play.hpp>

class Host;

namespace cloud {
class CloudLaunch;
}
class Session;
class SettingsManager;
enum class TakionTarget : uint8_t;

namespace akira::stream {

enum class SeatPhase : uint8_t {
    Empty,
    Joining,
    Passcode,
    Playing,
    Failed
};

struct CouchSeat {
    SeatPhase phase = SeatPhase::Empty;
    std::optional<libchiaking::play::SeatRefusal> refusal;
    bool dropped = false;
    std::string accountLabel;
    int64_t profileId = 0;
};

struct CouchProfileChoice {
    int64_t profileId = 0;
    std::string label;
    std::string accountId;
    std::string avatarUrl;
};

class ConnectionListener {
public:
    virtual ~ConnectionListener() = default;

    virtual void onConnectProgress(const libchiaking::play::ConnectProgress& progress) = 0;
    virtual void onLaunchProgress(const libchiaking::play::LaunchProgress& progress) = 0;
    virtual void onPlaying() = 0;
    virtual void onEnded(const libchiaking::play::Outcome& outcome) = 0;
    virtual void onRefused(const libchiaking::play::StartRefusal& refusal) = 0;
    virtual void onLoginPinNeeded(bool previousWasWrong) = 0;
    virtual void onSeatPasscodeNeeded(uint8_t slot, bool retry) = 0;
    virtual void onSeatRefused(uint8_t slot, const libchiaking::play::SeatRefusal& refusal) = 0;
    virtual void onSeatDropped(uint8_t slot) = 0;
};

class Connection final : private libchiaking::media::MediaSink, private libchiaking::play::SessionEvents {
public:
    using Started = libchiaking::Expected<std::shared_ptr<libchiaking::play::Session>, libchiaking::play::StartRefusal>;
    using Starter = std::function<Started(libchiaking::media::MediaSink&, libchiaking::play::SessionEvents&,
                                          const libchiaking::platform::StopToken&)>;

    static constexpr uint8_t seatCount = libchiaking::controllers::SeatNumber::count;

    Connection(Session& media, std::weak_ptr<ConnectionListener> listener, bool ps5);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    static auto preferencesFor(Host& host, SettingsManager& settings) -> std::optional<libchiaking::play::Preferences>;
    static auto cloudPreferencesFor(const cloud::CloudLaunch& launch, SettingsManager& settings)
        -> std::optional<libchiaking::play::Preferences>;
    static auto consoleStarter(std::shared_ptr<libchiaking::consoles::Console> console,
                               std::shared_ptr<libchiaking::users::PsUser> player,
                               libchiaking::play::Preferences preferences,
                               libchiaking::consoles::Route route) -> Starter;

    [[nodiscard]] bool open(Starter starter);

    auto state() const -> std::optional<libchiaking::play::SessionState>;
    bool submitLoginPin(std::string_view pin);
    void leave(bool restConsole);
    void setInputSuppressed(bool suppressed);

    bool addPlayer(uint8_t slot, std::optional<int64_t> profileId);
    void removePlayer(uint8_t slot);
    void sendSeatPasscode(uint8_t slot, std::string_view passcode);
    auto couchSeat(uint8_t slot) const -> const CouchSeat&;
    bool couchSlotAvailable() const;
    bool seatPlaying(uint8_t slot) const;
    auto couchProfileChoices(uint8_t slot) const -> std::vector<CouchProfileChoice>;

private:
    void run(const libchiaking::platform::StopToken& stop);
    void feed(libchiaking::play::Session& active, const libchiaking::platform::StopToken& stop);
    void sendInput(libchiaking::play::Session& active);
    static void drainJournal(const libchiaking::play::Session& active);
    static constexpr std::chrono::microseconds journalDrainPeriod{250000};
    auto session() const -> std::shared_ptr<libchiaking::play::Session>;
    void post(std::function<void(Connection&, ConnectionListener&)> work);
    void routeCouchFeedback(const libchiaking::controllers::ControllerFeedback& feedback);
    static auto tuned(libchiaking::play::PictureRequest picture, TakionTarget target, SettingsManager& settings)
        -> std::optional<libchiaking::play::Preferences>;

    void on_stream_description(const libchiaking::media::StreamDescription& description) override;
    auto on_video(const libchiaking::media::VideoUnit& unit) -> libchiaking::media::VideoResult override;
    void on_video_gap() override;
    auto on_audio(const libchiaking::media::AudioUnit& unit) -> libchiaking::media::AudioResult override;
    void on_feedback(const libchiaking::controllers::ControllerFeedback& feedback) override;
    auto decoder_name() const -> std::string_view override;

    void on_state(libchiaking::play::SessionState state) override;
    void on_ended(const libchiaking::play::Outcome& outcome) override;
    void on_connect_progress(const libchiaking::play::ConnectProgress& progress) override;
    void on_launch_progress(const libchiaking::play::LaunchProgress& progress) override;
    void on_login_pin_needed(bool previousWasWrong) override;
    void on_seat_passcode_needed(libchiaking::controllers::SeatNumber seat, bool retry) override;
    void on_seat_joined(libchiaking::controllers::SeatNumber seat, libchiaking::controllers::SeatLight light) override;
    void on_seat_left(libchiaking::controllers::SeatNumber seat) override;
    void on_seat_refused(const libchiaking::play::SeatRefusal& refusal) override;

    Session& media;
    std::weak_ptr<ConnectionListener> listener;
    std::shared_ptr<const bool> alive = std::make_shared<const bool>(true);
    const std::weak_ptr<const bool> aliveToken = alive;
    const bool ps5;

    mutable std::mutex sessionMutex;
    std::shared_ptr<libchiaking::play::Session> active;
    std::atomic<bool> inputSuppressed{false};

    std::array<libchiaking::controllers::ControllerInput, seatCount> inputs{};
    std::map<uint32_t, int8_t> fingerIdTouchId;
    uint8_t couchPadCount = 1;
    uint8_t hapticIntensity = 1;
    uint8_t triggerIntensity = 1;

    std::array<CouchSeat, seatCount> seats{};

    libchiaking::platform::Thread worker;
};

}
