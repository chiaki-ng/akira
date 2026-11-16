#ifndef AKIRA_SETTINGS_PICTURE_VIEW_HPP
#define AKIRA_SETTINGS_PICTURE_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_selector.hpp>
#include <borealis/views/cells/cell_slider.hpp>

#include "core/settings_manager.hpp"

class SettingsPictureView : public brls::Box {
public:
    SettingsPictureView();

private:
    BRLS_BIND(brls::SelectorCell, localResolutionSelector, "settings/localResolution");
    BRLS_BIND(brls::SelectorCell, localFpsSelector, "settings/localFps");
    BRLS_BIND(brls::SliderCell, localBitrateSlider, "settings/localBitrate");
    BRLS_BIND(brls::Header, remoteHeader, "settings/remoteHeader");
    BRLS_BIND(brls::Box, remoteSection, "settings/remoteSection");
    BRLS_BIND(brls::SelectorCell, remoteResolutionSelector, "settings/remoteResolution");
    BRLS_BIND(brls::SelectorCell, remoteFpsSelector, "settings/remoteFps");
    BRLS_BIND(brls::SliderCell, remoteBitrateSlider, "settings/remoteBitrate");
    BRLS_BIND(brls::SelectorCell, vpnResolutionSelector, "settings/vpnResolution");
    BRLS_BIND(brls::SelectorCell, vpnFpsSelector, "settings/vpnFps");
    BRLS_BIND(brls::SliderCell, vpnBitrateSlider, "settings/vpnBitrate");
    BRLS_BIND(brls::Header, cloudHeader, "settings/cloudHeader");
    BRLS_BIND(brls::Box, cloudSection, "settings/cloudSection");
    BRLS_BIND(brls::SelectorCell, pscloudResolutionSelector, "settings/pscloudResolution");
    BRLS_BIND(brls::SliderCell, pscloudBitrateSlider, "settings/pscloudBitrate");
    BRLS_BIND(brls::SelectorCell, psnowResolutionSelector, "settings/psnowResolution");
    BRLS_BIND(brls::SliderCell, psnowBitrateSlider, "settings/psnowBitrate");
    BRLS_BIND(brls::BooleanCell, enableDitheringToggle, "settings/enableDithering");
    BRLS_BIND(brls::SliderCell, ditheringStrengthSlider, "settings/ditheringStrength");
    BRLS_BIND(brls::BooleanCell, rcasEnabledToggle, "settings/rcasEnabled");
    BRLS_BIND(brls::SliderCell, rcasSharpnessSlider, "settings/rcasSharpness");
    BRLS_BIND(brls::Header, rendererHeader, "settings/rendererHeader");
    BRLS_BIND(brls::Box, rendererSection, "settings/rendererSection");
    BRLS_BIND(brls::SelectorCell, rendererChoice, "settings/rendererChoice");
    BRLS_BIND(brls::Label, rendererName, "settings/rendererName");
    BRLS_BIND(brls::Box, rendererOptions, "settings/rendererOptions");

    struct ResolutionPreset {
        std::string label;
        VideoResolution resolution;
        int fsrTarget;
    };

    SettingsManager* settings = nullptr;

    static std::vector<ResolutionPreset> resolutionPresets();
    static std::vector<std::string> presetLabels(const std::vector<ResolutionPreset>& presets);
    static int presetIndex(const std::vector<ResolutionPreset>& presets, VideoResolution resolution, bool fsr,
        int fsrTarget);

    void initLocalResolutionSelector();
    void initLocalFpsSelector();
    void initLocalBitrateSlider();
    void updateLocalBitrateSlider();
    void initRemoteResolutionSelector();
    void initRemoteFpsSelector();
    void initRemoteBitrateSlider();
    void updateRemoteBitrateSlider();
    void initVpnResolutionSelector();
    void initVpnFpsSelector();
    void initVpnBitrateSlider();
    void updateVpnBitrateSlider();
    void initCloudResolutionSelector(bool pscloud, brls::SelectorCell* selector,
                                     const std::string& title);
    void initCloudBitrateSlider(bool pscloud, brls::SliderCell* slider);
    void updateCloudBitrateSlider(bool pscloud, brls::SliderCell* slider);
    void initEnableDitheringToggle();
    void initDitheringStrengthSlider();
    void initRcasEnabledToggle();
    void initRcasSharpnessSlider();
    void initRendererSection();
};

#endif // AKIRA_SETTINGS_PICTURE_VIEW_HPP
