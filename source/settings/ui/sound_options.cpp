#include "settings/ui/sound_options.hpp"

#include "play/stream_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::settings::ui {

SoundOptions::SoundOptions(play::StreamSettings& stream, SettingsStore& store)
    : stream(stream)
    , store(store)
    , state(Values{ .volume = stream.volume() })
{
}

void SoundOptions::setVolume(play::Volume volume)
{
    if (volume == state.get().volume)
        return;
    stream.setVolume(volume);
    state.write(&Values::volume, volume);
    store.save();
}

}
