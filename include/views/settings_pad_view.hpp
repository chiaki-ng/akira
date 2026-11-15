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

#include "core/settings_manager.hpp"
#include "input/pad_path.hpp"

class SettingsPadView : public brls::Box {
public:
    SettingsPadView(const akira::input::PadDescription& pad);

    ~SettingsPadView() override { *m_alive = false; }

private:
    void buildHeader();
#if AKIRA_PLATFORM_NX
    void buildOutputRows();
#endif
    void buildSourceRows();
    void buildEnvelopeRows();
    void buildIntensityRow();
    void buildStrengthRows();
    void buildFrequencyRows();
#if AKIRA_PLATFORM_NX
    void buildLightbarRows();
#endif
    void buildGyroRow();
    void buildActionRows();

#if AKIRA_PLATFORM_NX
    bool takesDirectOutput() const;

#endif
    void store();
    void reopen();
    void previewBuzz();
#if AKIRA_PLATFORM_NX
    void previewDirectBuzz();
#endif

    brls::Box* content = nullptr;

    SettingsManager*            settings = nullptr;
    akira::input::PadDescription m_pad;
    akira::input::RumbleProfile  m_profile;

#if AKIRA_PLATFORM_NX
    akira::input::PadDriverState m_driver;

    brls::Box* m_lightbar_swatch = nullptr;
#endif
    std::string                  m_key;

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

#endif // AKIRA_SETTINGS_PAD_VIEW_HPP
