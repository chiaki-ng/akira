#ifndef AKIRA_CONTROLLER_REMAP_VIEW_HPP
#define AKIRA_CONTROLLER_REMAP_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <switch.h>
#include "input/remap_target.hpp"

class SettingsManager;

struct RemappableButton {
    uint32_t chiakiButton;
    std::string displayName;
    bool isAnalog;
};

class ControllerRemapView : public brls::Box {
public:
    ControllerRemapView();
    ~ControllerRemapView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;


    static brls::View* create();

    bool isTranslucent() override { return translucent; }
    void setTranslucent(bool t) { translucent = t; }
    void setStreamMode(bool enabled);

private:
    bool translucent = false;
    enum class State {
        BROWSING,
        CAPTURING
    };

    State currentState = State::BROWSING;

    brls::Label* titleLabel = nullptr;
    brls::Box* mainContent = nullptr;
    brls::ScrollingFrame* scrollFrame = nullptr;
    brls::Box* listContainer = nullptr;
    brls::Box* captureOverlay = nullptr;
    brls::Label* captureLabel = nullptr;
    brls::Label* captureButtonsLabel = nullptr;
    brls::Image* captureTargetIcon = nullptr;
    brls::Box* captureSwitchBox = nullptr;
    brls::Box* iconRow = nullptr;
    brls::Button* resetAllBtn = nullptr;

    std::map<uint64_t, brls::Image*> captureIcons;

    SettingsManager* settings = nullptr;
    std::vector<RemappableButton> remappableButtons;
    std::vector<brls::DetailCell*> detailCells;
    std::vector<brls::Box*> switchIconBoxes;
    std::map<uint32_t, brls::BooleanCell*> enableToggles;
    std::map<uint32_t, std::vector<uint64_t>> currentMapping;

    uint32_t captureTarget = 0;
    std::set<uint64_t> peakHeldButtons;
    bool captureActive = false;
    bool waitingForRelease = false;
    bool anyButtonWasPressed = false;
    int captureGraceFrames = 0;
    static constexpr int GRACE_FRAME_COUNT = 6;
    static constexpr int LONG_PRESS_FRAMES = 45;
    int xHoldFrames = 0;
    bool xWasHeld = false;

    PadState capturePad;
    bool capturePadInitialized = false;

    void initRemappableButtons();
    void buildUI();
    void loadMappings();
    void saveMappings();
    void updateCellDisplay(int index);
    void updateAllCellDisplays();

    void enterCaptureMode(uint32_t target);
    void exitCaptureMode(bool save);
    void updateCaptureDisplay();
    void pollCaptureInput();

    void resetAllToDefaults();
    void resetSingleToDefault(uint32_t chiakiButton);

    void buildCaptureIcons();

    static std::string formatMapping(const std::vector<uint64_t>& buttons);
    static std::string hidButtonToString(uint64_t button);
    static uint64_t stringToHidButton(const std::string& name);
    static std::string getButtonImagePath(uint64_t button);
    static std::string getPS5ButtonImagePath(uint32_t chiakiButton);
    static std::string chiakiButtonToConfigKey(uint32_t button);
    static uint32_t configKeyToChiakiButton(const std::string& key);

    bool isDefaultMapping(uint32_t chiakiButton) const;
    bool hasConflict(uint32_t chiakiButton) const;
    std::vector<uint64_t> getDefaultForButton(uint32_t chiakiButton) const;
    static bool isToggleableButton(uint32_t chiakiButton);
};

#endif // AKIRA_CONTROLLER_REMAP_VIEW_HPP
