#include "views/settings_picture_view.hpp"

#include <borealis/core/i18n.hpp>
#include <algorithm>
#include <format>
#include <iterator>

#include "platform/factory.hpp"

using namespace brls::literals;

SettingsPictureView::SettingsPictureView() {
    this->inflateFromXMLRes("xml/settings/picture.xml");

    settings = SettingsManager::getInstance();

    initLocalResolutionSelector();
    initLocalFpsSelector();
    initLocalBitrateSlider();
    initRemoteResolutionSelector();
    initRemoteFpsSelector();
    initRemoteBitrateSlider();
    initVpnResolutionSelector();
    initVpnFpsSelector();
    initVpnBitrateSlider();
    /*
     * Both of these are settings for something legacy cannot reach: cloud needs an npsso,
     * and the remote profile only ever applies to HostType::Remote hosts, which are
     * created solely by a PSN device lookup. A port-forwarded manual host deliberately
     * keeps the local profile, so nothing here is left silently steering it.
     */
    if (settings->isLegacyProfileActive()) {
        if (remoteHeader) remoteHeader->setVisibility(brls::Visibility::GONE);
        if (remoteSection) remoteSection->setVisibility(brls::Visibility::GONE);
        if (cloudHeader) cloudHeader->setVisibility(brls::Visibility::GONE);
        if (cloudSection) cloudSection->setVisibility(brls::Visibility::GONE);
    } else {
#if AKIRA_CLOUD
        initCloudResolutionSelector(true, pscloudResolutionSelector,
            "akira/settings/pscloud_resolution"_i18n);
        initCloudBitrateSlider(true, pscloudBitrateSlider);
        initCloudResolutionSelector(false, psnowResolutionSelector,
            "akira/settings/psnow_resolution"_i18n);
        initCloudBitrateSlider(false, psnowBitrateSlider);
#else
        if (cloudHeader) cloudHeader->setVisibility(brls::Visibility::GONE);
        if (cloudSection) cloudSection->setVisibility(brls::Visibility::GONE);
#endif
    }
    initEnableDitheringToggle();
    initDitheringStrengthSlider();
    initRcasEnabledToggle();
    initRcasSharpnessSlider();
    initZeroCopyToggle();
}

std::vector<SettingsPictureView::ResolutionPreset> SettingsPictureView::resolutionPresets() {
    std::vector<ResolutionPreset> presets = {
        {"akira/settings/res_360p"_i18n, VideoResolution::P360, 0},
        {"akira/settings/res_540p"_i18n, VideoResolution::P540, 0},
        {"akira/settings/res_720p"_i18n, VideoResolution::P720, 0},
        {"akira/settings/res_1080p"_i18n, VideoResolution::P1080, 0},
        {"720p (FSR)", VideoResolution::P540, 720},
        {"1080p (FSR)", VideoResolution::P720, 1080},
    };
    if (akira::platform::PlatformFactory::instance().supplies(akira::platform::PlatformCapability::fsr_4k)) {
        presets.push_back({"1440p (FSR)", VideoResolution::P1080, 1440});
        presets.push_back({"2160p (FSR)", VideoResolution::P1080, 2160});
    }
    return presets;
}

std::vector<std::string> SettingsPictureView::presetLabels(const std::vector<ResolutionPreset>& presets) {
    std::vector<std::string> labels;
    labels.reserve(presets.size());
    std::ranges::transform(presets, std::back_inserter(labels), &ResolutionPreset::label);
    return labels;
}

int SettingsPictureView::presetIndex(const std::vector<ResolutionPreset>& presets, VideoResolution resolution,
    bool fsr, int fsrTarget) {
    const int target = !fsr ? 0
        : fsrTarget > 0 ? fsrTarget
        : resolution == VideoResolution::P540 ? 720
        : resolution == VideoResolution::P720 ? 1080
        : 0;
    const auto matches = [&](int wanted) {
        return std::ranges::find_if(presets, [&](const ResolutionPreset& preset) {
            return preset.resolution == resolution && preset.fsrTarget == wanted;
        });
    };
    auto found = matches(target);
    if (found == presets.end())
        found = matches(0);
    if (found == presets.end())
        return 2;
    return static_cast<int>(std::distance(presets.begin(), found));
}

void SettingsPictureView::initLocalResolutionSelector() {
    const auto presets = resolutionPresets();
    localResolutionSelector->init(
        "akira/settings/local_resolution"_i18n,
        presetLabels(presets),
        presetIndex(presets, settings->getLocalVideoResolution(), settings->getLocalFsrEnabled(),
            settings->getFsrTarget(SettingsManager::StreamProfile::Local)),
        [](int selected) {},
        [this, presets](int selected) {
            const ResolutionPreset& preset = presets.at(static_cast<std::size_t>(selected));
            settings->setLocalVideoResolution(preset.resolution);
            settings->setLocalFsrEnabled(preset.fsrTarget > 0);
            settings->setFsrTarget(SettingsManager::StreamProfile::Local, preset.fsrTarget);
            updateLocalBitrateSlider();
            settings->writeFile();
            brls::Logger::info("Local resolution set to {}", preset.label);
        }
    );
}

void SettingsPictureView::initLocalFpsSelector() {
    std::vector<std::string> options = {"akira/settings/fps_30"_i18n, "akira/settings/fps_60"_i18n};

    int currentIndex = 1;
    if (settings->getLocalVideoFPS() == VideoFps::Fps30) {
        currentIndex = 0;
    }

    localFpsSelector->init(
        "akira/settings/local_frame_rate"_i18n,
        options,
        currentIndex,
        [](int selected) {},
        [this](int selected) {
            auto preset = selected == 0 ? VideoFps::Fps30 : VideoFps::Fps60;
            settings->setLocalVideoFPS(preset);
            settings->writeFile();
            brls::Logger::info("Local FPS set to {}", SettingsManager::fpsToString(preset));
        }
    );
}

void SettingsPictureView::initLocalBitrateSlider() {
    auto resolution = settings->getLocalVideoResolution();
    int maxBitrate = settings->getMaxBitrateForResolution(resolution);
    int minBitrate = settings->getMinBitrateForResolution(resolution);
    int currentBitrate = settings->getLocalVideoBitrate();

    bool needsSave = false;
    if (currentBitrate < minBitrate) { currentBitrate = minBitrate; needsSave = true; }
    if (currentBitrate > maxBitrate) { currentBitrate = maxBitrate; needsSave = true; }
    if (needsSave) {
        settings->setLocalVideoBitrate(currentBitrate);
        settings->writeFile();
    }

    float normalizedValue = static_cast<float>(currentBitrate - minBitrate) / (maxBitrate - minBitrate);
    normalizedValue = std::max(0.0f, std::min(1.0f, normalizedValue));

    localBitrateSlider->detail->setWidth(100);
    localBitrateSlider->detail->setShrink(0);
    localBitrateSlider->slider->setDiscreteStep(500.0f / static_cast<float>(maxBitrate - minBitrate));
    localBitrateSlider->init(
        "akira/settings/local_bitrate"_i18n,
        normalizedValue,
        [this](float value) {
            auto resolution = settings->getLocalVideoResolution();
            int maxBitrate = settings->getMaxBitrateForResolution(resolution);
            int minBitrate = settings->getMinBitrateForResolution(resolution);
            int bitrate = minBitrate + static_cast<int>(value * (maxBitrate - minBitrate));
            bitrate = ((bitrate + 250) / 500) * 500;
            bitrate = std::clamp(bitrate, minBitrate, maxBitrate);
            settings->setLocalVideoBitrate(bitrate);
            localBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", bitrate));
            settings->writeFile();
            brls::Logger::info("Local bitrate set to {}", bitrate);
        }
    );

    localBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", currentBitrate));
}

void SettingsPictureView::updateLocalBitrateSlider() {
    auto resolution = settings->getLocalVideoResolution();
    int defaultBitrate = SettingsManager::getDefaultBitrateForResolution(resolution);
    int maxBitrate = settings->getMaxBitrateForResolution(resolution);
    int minBitrate = settings->getMinBitrateForResolution(resolution);

    settings->setLocalVideoBitrate(defaultBitrate);

    float normalizedValue = static_cast<float>(defaultBitrate - minBitrate) / (maxBitrate - minBitrate);
    localBitrateSlider->slider->setProgress(normalizedValue);
    localBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", defaultBitrate));
}

void SettingsPictureView::initRemoteResolutionSelector() {
    const auto presets = resolutionPresets();
    remoteResolutionSelector->init(
        "akira/settings/remote_resolution"_i18n,
        presetLabels(presets),
        presetIndex(presets, settings->getRemoteVideoResolution(), settings->getRemoteFsrEnabled(),
            settings->getFsrTarget(SettingsManager::StreamProfile::Remote)),
        [](int selected) {},
        [this, presets](int selected) {
            const ResolutionPreset& preset = presets.at(static_cast<std::size_t>(selected));
            settings->setRemoteVideoResolution(preset.resolution);
            settings->setRemoteFsrEnabled(preset.fsrTarget > 0);
            settings->setFsrTarget(SettingsManager::StreamProfile::Remote, preset.fsrTarget);
            updateRemoteBitrateSlider();
            settings->writeFile();
            brls::Logger::info("Remote resolution set to {}", preset.label);
        }
    );
}

void SettingsPictureView::initRemoteFpsSelector() {
    std::vector<std::string> options = {"akira/settings/fps_30"_i18n, "akira/settings/fps_60"_i18n};

    int currentIndex = 1;
    if (settings->getRemoteVideoFPS() == VideoFps::Fps30) {
        currentIndex = 0;
    }

    remoteFpsSelector->init(
        "akira/settings/remote_frame_rate"_i18n,
        options,
        currentIndex,
        [](int selected) {},
        [this](int selected) {
            auto preset = selected == 0 ? VideoFps::Fps30 : VideoFps::Fps60;
            settings->setRemoteVideoFPS(preset);
            settings->writeFile();
            brls::Logger::info("Remote FPS set to {}", SettingsManager::fpsToString(preset));
        }
    );
}

void SettingsPictureView::initRemoteBitrateSlider() {
    auto resolution = settings->getRemoteVideoResolution();
    int maxBitrate = settings->getMaxBitrateForResolution(resolution);
    int minBitrate = settings->getMinBitrateForResolution(resolution);
    int currentBitrate = settings->getRemoteVideoBitrate();

    bool needsSave = false;
    if (currentBitrate < minBitrate) { currentBitrate = minBitrate; needsSave = true; }
    if (currentBitrate > maxBitrate) { currentBitrate = maxBitrate; needsSave = true; }
    if (needsSave) {
        settings->setRemoteVideoBitrate(currentBitrate);
        settings->writeFile();
    }

    float normalizedValue = static_cast<float>(currentBitrate - minBitrate) / (maxBitrate - minBitrate);
    normalizedValue = std::max(0.0f, std::min(1.0f, normalizedValue));

    remoteBitrateSlider->detail->setWidth(100);
    remoteBitrateSlider->detail->setShrink(0);
    remoteBitrateSlider->slider->setDiscreteStep(500.0f / static_cast<float>(maxBitrate - minBitrate));
    remoteBitrateSlider->init(
        "akira/settings/remote_bitrate"_i18n,
        normalizedValue,
        [this](float value) {
            auto resolution = settings->getRemoteVideoResolution();
            int maxBitrate = settings->getMaxBitrateForResolution(resolution);
            int minBitrate = settings->getMinBitrateForResolution(resolution);
            int bitrate = minBitrate + static_cast<int>(value * (maxBitrate - minBitrate));
            bitrate = ((bitrate + 250) / 500) * 500;
            bitrate = std::clamp(bitrate, minBitrate, maxBitrate);
            settings->setRemoteVideoBitrate(bitrate);
            remoteBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", bitrate));
            settings->writeFile();
            brls::Logger::info("Remote bitrate set to {}", bitrate);
        }
    );

    remoteBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", currentBitrate));
}

void SettingsPictureView::updateRemoteBitrateSlider() {
    auto resolution = settings->getRemoteVideoResolution();
    int defaultBitrate = SettingsManager::getDefaultBitrateForResolution(resolution);
    int maxBitrate = settings->getMaxBitrateForResolution(resolution);
    int minBitrate = settings->getMinBitrateForResolution(resolution);

    settings->setRemoteVideoBitrate(defaultBitrate);

    float normalizedValue = static_cast<float>(defaultBitrate - minBitrate) / (maxBitrate - minBitrate);
    remoteBitrateSlider->slider->setProgress(normalizedValue);
    remoteBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", defaultBitrate));
}

void SettingsPictureView::initVpnResolutionSelector() {
    const auto presets = resolutionPresets();
    vpnResolutionSelector->init(
        "akira/settings/vpn_resolution"_i18n,
        presetLabels(presets),
        presetIndex(presets, settings->getVpnVideoResolution(), settings->getVpnFsrEnabled(),
            settings->getFsrTarget(SettingsManager::StreamProfile::Vpn)),
        [](int selected) {},
        [this, presets](int selected) {
            const ResolutionPreset& preset = presets.at(static_cast<std::size_t>(selected));
            settings->setVpnVideoResolution(preset.resolution);
            settings->setVpnFsrEnabled(preset.fsrTarget > 0);
            settings->setFsrTarget(SettingsManager::StreamProfile::Vpn, preset.fsrTarget);
            updateVpnBitrateSlider();
            settings->writeFile();
            brls::Logger::info("Vpn resolution set to {}", preset.label);
        }
    );
}

void SettingsPictureView::initVpnFpsSelector() {
    std::vector<std::string> options = {"akira/settings/fps_30"_i18n, "akira/settings/fps_60"_i18n};

    int currentIndex = 0;
    if (settings->getVpnVideoFPS() == VideoFps::Fps60) {
        currentIndex = 1;
    }

    vpnFpsSelector->init(
        "akira/settings/vpn_frame_rate"_i18n,
        options,
        currentIndex,
        [](int selected) {},
        [this](int selected) {
            auto preset = selected == 0 ? VideoFps::Fps30 : VideoFps::Fps60;
            settings->setVpnVideoFPS(preset);
            settings->writeFile();
            brls::Logger::info("VPN FPS set to {}", SettingsManager::fpsToString(preset));
        }
    );
}

void SettingsPictureView::initVpnBitrateSlider() {
    const int VPN_MIN_BITRATE = 2000;
    const int VPN_MAX_BITRATE = 15000;

    int currentBitrate = settings->getVpnVideoBitrate();

    if (currentBitrate < VPN_MIN_BITRATE) currentBitrate = VPN_MIN_BITRATE;
    if (currentBitrate > VPN_MAX_BITRATE) currentBitrate = VPN_MAX_BITRATE;
    settings->setVpnVideoBitrate(currentBitrate);

    float normalizedValue = static_cast<float>(currentBitrate - VPN_MIN_BITRATE) / (VPN_MAX_BITRATE - VPN_MIN_BITRATE);
    normalizedValue = std::max(0.0f, std::min(1.0f, normalizedValue));

    vpnBitrateSlider->detail->setWidth(100);
    vpnBitrateSlider->detail->setShrink(0);
    vpnBitrateSlider->slider->setDiscreteStep(500.0f / static_cast<float>(VPN_MAX_BITRATE - VPN_MIN_BITRATE));
    vpnBitrateSlider->init(
        "akira/settings/vpn_bitrate"_i18n,
        normalizedValue,
        [this](float value) {
            const int VPN_MIN_BITRATE = 2000;
            const int VPN_MAX_BITRATE = 15000;
            int bitrate = VPN_MIN_BITRATE + static_cast<int>(value * (VPN_MAX_BITRATE - VPN_MIN_BITRATE));
            bitrate = ((bitrate + 250) / 500) * 500;
            bitrate = std::clamp(bitrate, VPN_MIN_BITRATE, VPN_MAX_BITRATE);
            settings->setVpnVideoBitrate(bitrate);
            vpnBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", bitrate));
            settings->writeFile();
            brls::Logger::info("VPN bitrate set to {}", bitrate);
        }
    );

    vpnBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", currentBitrate));
}

void SettingsPictureView::updateVpnBitrateSlider() {
    const int VPN_MIN_BITRATE = 2000;
    const int VPN_MAX_BITRATE = 15000;
    const int VPN_DEFAULT_BITRATE = 5000;

    settings->setVpnVideoBitrate(VPN_DEFAULT_BITRATE);

    float normalizedValue = static_cast<float>(VPN_DEFAULT_BITRATE - VPN_MIN_BITRATE) / (VPN_MAX_BITRATE - VPN_MIN_BITRATE);
    vpnBitrateSlider->slider->setProgress(normalizedValue);
    vpnBitrateSlider->detail->setText(brls::getStr("akira/settings/kbps", VPN_DEFAULT_BITRATE));
}

void SettingsPictureView::initCloudResolutionSelector(bool pscloud,
                                                      brls::SelectorCell* selector,
                                                      const std::string& title) {
    std::vector<std::string> options = {
        "akira/settings/res_720p"_i18n,
        "akira/settings/res_1080p"_i18n,
        "1080p (FSR)",
    };
    const bool hevcTiers = pscloud && akira::platform::PlatformFactory::instance().supplies(akira::platform::PlatformCapability::hevc);
    if (hevcTiers) {
        options.push_back("1440p");
        options.push_back("2160p");
    }

    int currentIndex;
    if (settings->getCloudFsrEnabled(pscloud)) {
        currentIndex = 2;
    } else {
        int currentResolution = settings->getCloudVideoResolution(pscloud);
        if (hevcTiers && currentResolution >= 2160)
            currentIndex = 4;
        else if (hevcTiers && currentResolution >= 1440)
            currentIndex = 3;
        else
            currentIndex = currentResolution <= 720 ? 0 : 1;
    }

    brls::SliderCell* slider = pscloud ? pscloudBitrateSlider : psnowBitrateSlider;

    selector->init(
        title,
        options,
        currentIndex,
        [](int selected) {},
        [this, pscloud, slider](int selected) {
            int resolution;
            bool fsr = false;
            switch (selected) {
                case 0: resolution = 720; break;
                case 1: resolution = 1080; break;
                case 2: resolution = 720; fsr = true; break;
                case 3: resolution = 1440; break;
                case 4: resolution = 2160; break;
                default: resolution = 1080; break;
            }
            settings->setCloudVideoResolution(pscloud, resolution);
            settings->setCloudFsrEnabled(pscloud, fsr);
            updateCloudBitrateSlider(pscloud, slider);
            settings->writeFile();
            brls::Logger::info("{} resolution set to {}p{}",
                pscloud ? "PS5 cloud" : "PS Now", resolution, fsr ? " (FSR)" : "");
        }
    );
}

void SettingsPictureView::initCloudBitrateSlider(bool pscloud, brls::SliderCell* slider) {
    auto resolution = SettingsManager::intToResolution(settings->getCloudVideoResolution(pscloud));
    int maxBitrate = settings->getMaxBitrateForResolution(resolution);
    int minBitrate = settings->getMinBitrateForResolution(resolution);
    int currentBitrate = settings->getCloudVideoBitrate(pscloud);

    bool needsSave = false;
    if (currentBitrate < minBitrate) { currentBitrate = minBitrate; needsSave = true; }
    if (currentBitrate > maxBitrate) { currentBitrate = maxBitrate; needsSave = true; }
    if (needsSave) {
        settings->setCloudVideoBitrate(pscloud, currentBitrate);
        settings->writeFile();
    }

    float normalizedValue = static_cast<float>(currentBitrate - minBitrate) / (maxBitrate - minBitrate);
    normalizedValue = std::max(0.0f, std::min(1.0f, normalizedValue));

    slider->detail->setWidth(100);
    slider->detail->setShrink(0);
    slider->slider->setDiscreteStep(500.0f / static_cast<float>(maxBitrate - minBitrate));
    slider->init(
        pscloud ? "akira/settings/pscloud_bitrate"_i18n : "akira/settings/psnow_bitrate"_i18n,
        normalizedValue,
        [this, pscloud, slider](float value) {
            auto resolution = SettingsManager::intToResolution(settings->getCloudVideoResolution(pscloud));
            int maxBitrate = settings->getMaxBitrateForResolution(resolution);
            int minBitrate = settings->getMinBitrateForResolution(resolution);
            int bitrate = minBitrate + static_cast<int>(value * (maxBitrate - minBitrate));
            bitrate = ((bitrate + 250) / 500) * 500;
            bitrate = std::clamp(bitrate, minBitrate, maxBitrate);
            settings->setCloudVideoBitrate(pscloud, bitrate);
            slider->detail->setText(brls::getStr("akira/settings/kbps", bitrate));
            settings->writeFile();
            brls::Logger::info("{} bitrate set to {}", pscloud ? "PS5 cloud" : "PS Now", bitrate);
        }
    );

    slider->detail->setText(brls::getStr("akira/settings/kbps", currentBitrate));
}

void SettingsPictureView::updateCloudBitrateSlider(bool pscloud, brls::SliderCell* slider) {
    auto resolution = SettingsManager::intToResolution(settings->getCloudVideoResolution(pscloud));
    int defaultBitrate = SettingsManager::getDefaultBitrateForResolution(resolution);
    int maxBitrate = settings->getMaxBitrateForResolution(resolution);
    int minBitrate = settings->getMinBitrateForResolution(resolution);

    settings->setCloudVideoBitrate(pscloud, defaultBitrate);

    float normalizedValue = static_cast<float>(defaultBitrate - minBitrate) / (maxBitrate - minBitrate);
    slider->slider->setProgress(normalizedValue);
    slider->detail->setText(brls::getStr("akira/settings/kbps", defaultBitrate));
}

void SettingsPictureView::initEnableDitheringToggle() {
    bool currentValue = settings->getEnableDithering();

    enableDitheringToggle->init(
        "akira/settings/enable_dithering"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setEnableDithering(isOn);
            settings->writeFile();
        }
    );
}

void SettingsPictureView::initDitheringStrengthSlider() {
    constexpr float MIN_STRENGTH = 1.0f;
    constexpr float MAX_STRENGTH = 10.0f;

    float current = settings->getDitheringStrength();
    float normalized = (current - MIN_STRENGTH) / (MAX_STRENGTH - MIN_STRENGTH);
    normalized = std::max(0.0f, std::min(1.0f, normalized));

    ditheringStrengthSlider->detail->setWidth(100);
    ditheringStrengthSlider->detail->setShrink(0);
    ditheringStrengthSlider->slider->setDiscreteStep(1.0f / (MAX_STRENGTH - MIN_STRENGTH));
    ditheringStrengthSlider->init(
        "akira/settings/dithering_strength"_i18n,
        normalized,
        [this](float value) {
            constexpr float MIN_STRENGTH = 1.0f;
            constexpr float MAX_STRENGTH = 10.0f;
            float strength = MIN_STRENGTH + value * (MAX_STRENGTH - MIN_STRENGTH);
            strength = static_cast<float>(static_cast<int>(strength));
            settings->setDitheringStrength(strength);
            ditheringStrengthSlider->detail->setText(std::format("{}", static_cast<int>(strength)));
            settings->writeFile();
        }
    );

    ditheringStrengthSlider->detail->setText(std::format("{}", static_cast<int>(current)));
}

void SettingsPictureView::initRcasEnabledToggle() {
    bool currentValue = settings->getRcasEnabled();

    rcasEnabledToggle->init(
        "RCAS (Sharpening)",
        currentValue,
        [this](bool isOn) {
            settings->setRcasEnabled(isOn);
            settings->writeFile();
        }
    );
}

void SettingsPictureView::initRcasSharpnessSlider() {
    float current = settings->getRcasSharpness();
    float normalized = 1.0f - (current / 2.0f);

    rcasSharpnessSlider->detail->setWidth(100);
    rcasSharpnessSlider->detail->setShrink(0);
    rcasSharpnessSlider->slider->setDiscreteStep(0.05f);
    rcasSharpnessSlider->init(
        "RCAS Strength",
        normalized,
        [this](float value) {
            float sharpness = (1.0f - value) * 2.0f;
            settings->setRcasSharpness(sharpness);
            int percent = static_cast<int>(value * 100.0f);
            rcasSharpnessSlider->detail->setText(std::format("{}%", percent));
            settings->writeFile();
        }
    );

    int percent = static_cast<int>(normalized * 100.0f);
    rcasSharpnessSlider->detail->setText(std::format("{}%", percent));
}

void SettingsPictureView::initZeroCopyToggle() {
    if (!akira::platform::PlatformFactory::instance().supplies(akira::platform::PlatformCapability::zero_copy)) {
        videoHeader->setVisibility(brls::Visibility::GONE);
        videoSection->setVisibility(brls::Visibility::GONE);
        return;
    }
    zeroCopyToggle->init(
        "akira/settings/zero_copy"_i18n,
        settings->getZeroCopyVideo(),
        [this](bool isOn) {
            settings->setZeroCopyVideo(isOn);
            settings->writeFile();
            brls::Logger::info("Zero-copy video set to {}", isOn ? "on" : "off");
        });
}
