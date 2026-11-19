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

#include "settings/settings_manager.hpp"
#include "platform/pads/pad.hpp"
#include "platform/pad_settings/provider.hpp"

class SettingsPadView : public brls::Box {
public:
    SettingsPadView(const akira::input::PadDescription& pad);

    ~SettingsPadView() override { *m_alive = false; }

    static brls::Box* makeCard();
    static brls::Label* makeHeading(const std::string& text);

private:
    void buildHeader();
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

    SettingsManager*            settings = nullptr;
    akira::input::PadDescription m_pad;
    akira::input::RumbleProfile  m_profile;
    std::unique_ptr<akira::platform::PadSettingsProvider> m_platform;

    std::string                  m_key;

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

#endif // AKIRA_SETTINGS_PAD_VIEW_HPP
