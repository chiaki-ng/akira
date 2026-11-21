#include "play/lazy_session.hpp"

#include "play/session.hpp"

namespace akira::play {

LazySession::LazySession(platform::PlatformFactory& platform, StreamState& streamState,
                         const ControllerSettings& controllers, const settings::DeveloperSettings& developer,
                         app::UiWindow& window, libchiaking::platform::ThreadingProvider& threading,
                         AudioOutput& audio)
    : platform(platform), streamState(streamState), controllers(controllers), developer(developer), window(window),
      threading(threading), audio(audio)
{
}

LazySession::~LazySession() = default;

Session& LazySession::get()
{
    if (!session)
        session = std::make_unique<Session>(platform, streamState, controllers, developer, window, threading, audio);
    return *session;
}

void LazySession::abandon()
{
    static_cast<void>(session.release());
}

}
