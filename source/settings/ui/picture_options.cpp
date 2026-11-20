#include "settings/ui/picture_options.hpp"

#include <utility>

#include "play/stream_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::settings::ui {

PictureOptions::PictureOptions(play::StreamSettings& stream, SettingsStore& store, bool fsr4k)
    : stream(stream)
    , store(store)
    , state(Values{
          .local = play::PictureProfile(stream, play::StreamProfile::Local, fsr4k),
          .remote = play::PictureProfile(stream, play::StreamProfile::Remote, fsr4k),
          .vpn = play::PictureProfile(stream, play::StreamProfile::Vpn, fsr4k),
          .rcas = stream.rcasEnabled(),
          .rcasStrength = stream.rcasStrength(),
      })
{
}

auto PictureOptions::profile(play::StreamProfile chosen) const -> const play::PictureProfile&
{
    if (chosen == play::StreamProfile::Remote)
        return state.get().remote;
    if (chosen == play::StreamProfile::Vpn)
        return state.get().vpn;
    return state.get().local;
}

template <class Change>
void PictureOptions::change(play::StreamProfile chosen, Change&& edit)
{
    play::PictureProfile edited = profile(chosen);
    edit(edited);
    if (edited == profile(chosen))
        return;
    edited.applyTo(stream);
    state.update([&](Values& values) {
        if (chosen == play::StreamProfile::Remote)
            values.remote = edited;
        else if (chosen == play::StreamProfile::Vpn)
            values.vpn = edited;
        else
            values.local = edited;
    });
    save();
}

void PictureOptions::chooseResolution(play::StreamProfile chosen, play::VideoResolution resolution)
{
    change(chosen, [resolution](play::PictureProfile& edited) { edited.chooseResolution(resolution); });
}

void PictureOptions::chooseUpscale(play::StreamProfile chosen, int target)
{
    change(chosen, [target](play::PictureProfile& edited) { edited.chooseUpscale(target); });
}

void PictureOptions::chooseFps(play::StreamProfile chosen, play::VideoFps fps)
{
    change(chosen, [fps](play::PictureProfile& edited) { edited.chooseFps(fps); });
}

void PictureOptions::chooseBitrate(play::StreamProfile chosen, int bitrate)
{
    change(chosen, [bitrate](play::PictureProfile& edited) { edited.chooseBitrate(bitrate); });
}

void PictureOptions::setRcas(bool enabled)
{
    if (enabled == state.get().rcas)
        return;
    stream.setRcasEnabled(enabled);
    state.write(&Values::rcas, enabled);
    save();
}

void PictureOptions::setRcasStrength(int percent)
{
    stream.setRcasStrength(percent);
    const int stored = stream.rcasStrength();
    if (stored == state.get().rcasStrength)
        return;
    state.write(&Values::rcasStrength, stored);
    save();
}

void PictureOptions::save()
{
    store.save();
}

}
