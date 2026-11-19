#include "play/stream_launcher.hpp"

#include "accounts/account_settings.hpp"
#include "consoles/host.hpp"
#include "log/log.hpp"
#include "platform/factory.hpp"
#include "play/chiaki_core.hpp"
#include "play/rumble_profile.hpp"
#include "play/session.hpp"
#include "play/stream_preferences.hpp"

namespace akira::play {

StreamLauncher::StreamLauncher(Session& session, platform::PlatformFactory& platform,
    const StreamPreferences& preferences, const accounts::AccountSettings& accounts, app::UiDispatcher& dispatcher,
    ChiakiCore& core)
    : session(session)
    , platform(platform)
    , preferences(preferences)
    , accounts(accounts)
    , dispatcher(dispatcher)
    , core(core)
{
}

auto StreamLauncher::pictureOf(const libchiaking::play::Picture& picture) -> Picture
{
    return Picture{
        .width = static_cast<int>(picture.width()),
        .height = static_cast<int>(picture.height()),
        .fps = static_cast<int>(picture.fps()),
        .bitrate = static_cast<int>(picture.bitrate_kbps()),
        .hevc = picture.codec() != libchiaking::media::VideoCodec::h264,
    };
}

void StreamLauncher::applyPadProfile()
{
    const RumbleProfile padProfile = platform.pads().primaryRumbleProfile();
    session.setRumbleStrength(padProfile.rumble_source == RumbleSource::Off ? 0.0f : padProfile.strength);
    session.setRumbleSource(padProfile.rumble_source);
    session.setHapticIntensity(padProfile.haptic_intensity);
    session.setHapticBase(HapticBaseForIntensity(padProfile.haptic_intensity));
    session.setRumbleFreqs(padProfile.freq_low, padProfile.freq_high);
    session.setEnvelopeAttack(padProfile.envelope_attack);
    session.setEnvelopeDecay(padProfile.envelope_decay);
    session.setRumbleCeiling(padProfile.ceiling);
}

auto StreamLauncher::open(consoles::Host& host, std::shared_ptr<consoles::Host> owner,
    std::weak_ptr<ConnectionListener> listener, libchiaking::play::ConsoleHandover handover,
    const std::string& context) -> std::expected<std::shared_ptr<Connection>, Failure>
{
    namespace play = libchiaking::play;

    auto* signedIn = core.accounts();
    auto player = signedIn ? signedIn->activeUser() : nullptr;
    const auto& console = host.getConsole();
    Connection::Starter starter;
    Picture picture;
    if (const auto* launch = host.cloudLaunch()) {
        auto chosen = preferences.forCloud(*launch);
        if (!chosen)
            return std::unexpected(Failure::NoPreferences);
        picture = pictureOf(chosen->picture.on_cloud(launch->service()));
        starter = [owner = std::move(owner), launch, chosen = *chosen, &core = core](
                      libchiaking::media::MediaSink& sink, play::SessionEvents& events,
                      const libchiaking::platform::StopToken&) -> Connection::Started {
            return launch->start(core, chosen, sink, events);
        };
    } else {
        if (!player || !console)
            return std::unexpected(Failure::NotRegistered);
        auto chosen = preferences.forConsole(host);
        if (!chosen)
            return std::unexpected(Failure::NoPreferences);
        chosen->handover = handover;
        picture = pictureOf(chosen->picture.on_console(console->generation()));
        starter = Connection::consoleStarter(console, player, *chosen,
            host.isRemote() ? libchiaking::consoles::Route::internet : libchiaking::consoles::Route::home);
    }

    log::info("StreamView: requesting {}x{} at {} fps, {} kbps, {}", picture.width, picture.height, picture.fps,
        picture.bitrate, picture.hevc ? "HEVC" : "H.264");
    session.setRequestedProfile(picture.width, picture.height, picture.fps, picture.bitrate, picture.hevc);

    applyPadProfile();

    if (!session.InitAVCodec(picture.hevc, host.isPS5(), picture.width, picture.height))
        return std::unexpected(Failure::Codec);
    if (!session.InitVideo(picture.width, picture.height))
        return std::unexpected(Failure::Video);

    session.setConnectionContext(context);

    auto connection = std::make_shared<Connection>(session, std::move(listener), host.isPS5(), platform, accounts,
        dispatcher, core);
    if (!connection->open(std::move(starter)))
        return std::unexpected(Failure::Worker);
    return connection;
}

}
