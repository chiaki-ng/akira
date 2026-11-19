#ifndef AKIRA_SETTINGS_PAD_VIEW_HPP
#define AKIRA_SETTINGS_PAD_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/cells/cell_selector.hpp>
#include <borealis/views/cells/cell_slider.hpp>

#include <cstdint>
#include <memory>
#include <string>

#include "app/view_services.hpp"
#include "platform/pads/pad.hpp"
#include "platform/pad_settings/provider.hpp"

namespace akira::settings::views {

class SettingsPadView : public brls::Box {
public:
    SettingsPadView(const akira::platform::PadDescription& pad);

    ~SettingsPadView() override { *m_alive = false; }

private:
    static brls::Box* makeCard();
    static brls::Label* makeHeading(const std::string& text);
    static void ReopenWhenSettled(const akira::platform::PadDescription& pad, int attempts_left);

    void buildHeader();
    void buildOutputRows();
    void buildLightbarRows();
    void buildSourceRows();
    void buildEnvelopeRows();
    void buildIntensityRow();
    void buildStrengthRows();
    void buildFrequencyRows();
    void buildGyroRow();
    void buildActionRows();

    void store();
    void reopen();
    void previewBuzz();

    brls::Box* content = nullptr;

    app::Services* services = nullptr;
    akira::platform::PadDescription m_pad;
    play::RumbleProfile  m_profile;
    std::unique_ptr<akira::platform::PadSettingsProvider> m_platform;

    std::string                  m_key;

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

}

#endif // AKIRA_SETTINGS_PAD_VIEW_HPP
