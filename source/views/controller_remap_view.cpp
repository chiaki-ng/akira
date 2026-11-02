#include "views/controller_remap_view.hpp"
#include "ui/theme.hpp"
#include "core/settings_manager.hpp"
#include "core/swipe_direction.hpp"

#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/i18n.hpp>
using namespace brls::literals;
using akira::input::RemapTarget;
using akira::input::remapKey;

#include <algorithm>

ControllerRemapView::ControllerRemapView()
{
    this->inflateFromXMLRes("xml/views/controller_remap.xml");

    titleLabel = (brls::Label*)this->getView("remap/title");
    mainContent = (brls::Box*)this->getView("remap/mainContent");
    auto* controllerImg = (brls::Image*)this->getView("remap/controllerImage");
    controllerImg->setImageFromRes("img/ps5/controller.png");
    scrollFrame = (brls::ScrollingFrame*)this->getView("remap/scroll");
    scrollFrame->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    this->setDefaultFocusedIndex(1);
    listContainer = (brls::Box*)this->getView("remap/list");
    captureOverlay = (brls::Box*)this->getView("remap/captureOverlay");
    captureLabel = (brls::Label*)this->getView("remap/captureLabel");
    captureButtonsLabel = (brls::Label*)this->getView("remap/captureButtons");

    settings = SettingsManager::getInstance();

    initRemappableButtons();
    loadMappings();
    buildUI();
    buildCaptureIcons();

    captureLabel->setVisibility(brls::Visibility::GONE);

    captureTargetIcon = new brls::Image();
    captureTargetIcon->setWidth(80);
    captureTargetIcon->setHeight(80);
    captureTargetIcon->setMarginBottom(20);
    captureOverlay->addView(captureTargetIcon, 1);

    captureSwitchBox = new brls::Box();
    captureSwitchBox->setAxis(brls::Axis::ROW);
    captureSwitchBox->setAlignItems(brls::AlignItems::CENTER);
    captureSwitchBox->setJustifyContent(brls::JustifyContent::CENTER);
    captureSwitchBox->setMarginBottom(20);
    captureOverlay->addView(captureSwitchBox, 2);

    captureButtonsLabel->setVisibility(brls::Visibility::GONE);

    padInitializeDefault(&capturePad);
    capturePadInitialized = true;

    this->registerAction("Back", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        if (currentState == State::CAPTURING) {
            return true;
        }
        brls::Application::popActivity();
        return true;
    }, true);
}

ControllerRemapView::~ControllerRemapView()
{
}

brls::View* ControllerRemapView::create()
{
    return new ControllerRemapView();
}

void ControllerRemapView::initRemappableButtons()
{
    remappableButtons = {
        {remapKey(RemapTarget::Cross),    "akira/remap/cross"_i18n,    false},
        {remapKey(RemapTarget::Moon),     "akira/remap/circle"_i18n,   false},
        {remapKey(RemapTarget::Box),      "akira/remap/square"_i18n,   false},
        {remapKey(RemapTarget::Pyramid),  "akira/remap/triangle"_i18n, false},
        {remapKey(RemapTarget::L1),       "akira/remap/l1"_i18n,       false},
        {remapKey(RemapTarget::R1),       "akira/remap/r1"_i18n,       false},
        {remapKey(RemapTarget::L2), "akira/remap/l2"_i18n,      true},
        {remapKey(RemapTarget::R2), "akira/remap/r2"_i18n,      true},
        {remapKey(RemapTarget::L3),       "akira/remap/l3"_i18n,       false},
        {remapKey(RemapTarget::R3),       "akira/remap/r3"_i18n,       false},
        {remapKey(RemapTarget::Options),  "akira/remap/options"_i18n,  false},
        {remapKey(RemapTarget::Share),    "akira/remap/share"_i18n,    false},
        {remapKey(RemapTarget::Touchpad), "akira/remap/touchpad"_i18n, false},
        {remapKey(RemapTarget::Ps),       "akira/remap/ps"_i18n,       false},
        {SWIPE_TOUCHPAD_UP,                 "akira/remap/swipe_up"_i18n,    false},
        {SWIPE_TOUCHPAD_DOWN,               "akira/remap/swipe_down"_i18n,  false},
        {SWIPE_TOUCHPAD_LEFT,               "akira/remap/swipe_left"_i18n,  false},
        {SWIPE_TOUCHPAD_RIGHT,              "akira/remap/swipe_right"_i18n, false},
    };
}

void ControllerRemapView::buildUI()
{
    detailCells.clear();
    switchIconBoxes.clear();

    for (size_t i = 0; i < remappableButtons.size(); i++) {
        auto* cell = new brls::DetailCell();
        cell->setText("");
        cell->setDetailText("");

        uint32_t target = remappableButtons[i].chiakiButton;

        auto* titleLbl = (brls::Label*)cell->getChildren()[0];
        titleLbl->setText("");
        titleLbl->setGrow(1.0f);

        auto* detailLbl = (brls::Label*)cell->getChildren()[1];
        detailLbl->setVisibility(brls::Visibility::GONE);

        std::string ps5Path = getPS5ButtonImagePath(target);
        if (!ps5Path.empty()) {
            auto* ps5Icon = new brls::Image();
            ps5Icon->setImageFromRes(ps5Path);
            ps5Icon->setWidth(50);
            ps5Icon->setHeight(50);
            cell->addView(ps5Icon, 0);
        }

        auto* switchBox = new brls::Box();
        switchBox->setAxis(brls::Axis::ROW);
        switchBox->setAlignItems(brls::AlignItems::CENTER);
        cell->addView(switchBox);
        switchIconBoxes.push_back(switchBox);

        cell->registerClickAction([this, target](brls::View*) {
            if (currentState == State::CAPTURING)
                return true;
            enterCaptureMode(target);
            return true;
        });

        cell->setFocusable(true);
        cell->addGestureRecognizer(new brls::TapGestureRecognizer(cell));
        listContainer->addView(cell);
        detailCells.push_back(cell);

        if (isToggleableButton(target)) {
            auto* toggle = new brls::BooleanCell();
            toggle->init(
                remappableButtons[i].displayName + "akira/remap/enabled_suffix"_i18n,
                settings->isButtonEnabled(target),
                [this, target](bool isOn) {
                    switch (target) {
                        case remapKey(RemapTarget::Touchpad):
                            settings->setTouchpadEnabled(isOn); break;
                        case SWIPE_TOUCHPAD_UP:
                            settings->setSwipeUpEnabled(isOn); break;
                        case SWIPE_TOUCHPAD_DOWN:
                            settings->setSwipeDownEnabled(isOn); break;
                        case SWIPE_TOUCHPAD_LEFT:
                            settings->setSwipeLeftEnabled(isOn); break;
                        case SWIPE_TOUCHPAD_RIGHT:
                            settings->setSwipeRightEnabled(isOn); break;
                    }
                    settings->writeFile();
                }
            );
            listContainer->addView(toggle);
            enableToggles[target] = toggle;
        }
    }

    resetAllBtn = new brls::Button();
    resetAllBtn->setStyle(&brls::BUTTONSTYLE_BORDERED);
    resetAllBtn->setText("akira/remap/reset_all"_i18n);
    resetAllBtn->setMargins(20, 0, 0, 0);
    resetAllBtn->setFocusable(true);
    resetAllBtn->registerClickAction([this](brls::View*) {
        resetAllToDefaults();
        return true;
    });
    listContainer->addView(resetAllBtn);

    updateAllCellDisplays();
}

void ControllerRemapView::loadMappings()
{
    currentMapping = settings->getButtonMapping();
}

void ControllerRemapView::saveMappings()
{
    settings->setButtonMapping(currentMapping);
    settings->writeFile();
}

void ControllerRemapView::updateCellDisplay(int index)
{
    if (index < 0 || index >= (int)detailCells.size()) return;
    if (index >= (int)switchIconBoxes.size()) return;

    uint32_t button = remappableButtons[index].chiakiButton;
    auto it = currentMapping.find(button);

    auto* switchBox = switchIconBoxes[index];
    switchBox->clearViews();

    if (it != currentMapping.end() && !it->second.empty()) {
        for (size_t j = 0; j < it->second.size(); j++) {
            if (j > 0) {
                auto* plus = new brls::Label();
                plus->setText("+");
                plus->setFontSize(14);
                plus->setMargins(0, 4, 0, 4);
                switchBox->addView(plus);
            }
            std::string path = getButtonImagePath(it->second[j]);
            if (!path.empty()) {
                auto* icon = new brls::Image();
                icon->setImageFromRes(path);
                icon->setWidth(45);
                icon->setHeight(45);
                switchBox->addView(icon);
            } else {
                auto* lbl = new brls::Label();
                lbl->setText(hidButtonToString(it->second[j]));
                lbl->setFontSize(14);
                switchBox->addView(lbl);
            }
        }
    } else {
        auto* lbl = new brls::Label();
        lbl->setText("akira/remap/not_mapped"_i18n);
        lbl->setFontSize(14);
        switchBox->addView(lbl);
    }

    if (isDefaultMapping(button)) {
        auto* statusLbl = new brls::Label();
        statusLbl->setText("akira/remap/default"_i18n);
        statusLbl->setFontSize(14);
        statusLbl->setMarginLeft(8);
        statusLbl->setTextColor(akira::ui::active().textDim);
        switchBox->addView(statusLbl);
    }

    if (hasConflict(button)) {
        detailCells[index]->setBackgroundColor(akira::ui::withAlpha(akira::ui::active().danger, 80));
    } else {
        detailCells[index]->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
    }
}

void ControllerRemapView::updateAllCellDisplays()
{
    for (size_t i = 0; i < detailCells.size(); i++) {
        updateCellDisplay(i);
    }
}

void ControllerRemapView::buildCaptureIcons()
{
    iconRow = new brls::Box();
    iconRow->setAxis(brls::Axis::ROW);
    iconRow->setJustifyContent(brls::JustifyContent::CENTER);
    iconRow->setAlignItems(brls::AlignItems::CENTER);
    iconRow->setMarginBottom(20);

    static const uint64_t allButtons[] = {
        HidNpadButton_A, HidNpadButton_B, HidNpadButton_X, HidNpadButton_Y,
        HidNpadButton_L, HidNpadButton_R, HidNpadButton_ZL, HidNpadButton_ZR,
        HidNpadButton_Plus, HidNpadButton_Minus,
        HidNpadButton_StickL, HidNpadButton_StickR,
        HidNpadButton_LeftSL, HidNpadButton_LeftSR,
        HidNpadButton_Up, HidNpadButton_Down,
        HidNpadButton_Left, HidNpadButton_Right,
    };

    for (uint64_t btn : allButtons) {
        std::string path = getButtonImagePath(btn);
        if (path.empty()) continue;

        auto* img = new brls::Image();
        img->setImageFromRes(path);
        img->setWidth(40);
        img->setHeight(40);
        img->setMargins(0, 4, 0, 4);
        img->setAlpha(0.2f);
        iconRow->addView(img);
        captureIcons[btn] = img;
    }

    captureOverlay->addView(iconRow, 1);
}

std::string ControllerRemapView::getButtonImagePath(uint64_t button)
{
    switch (button) {
        case HidNpadButton_A: return "img/buttons/a.png";
        case HidNpadButton_B: return "img/buttons/b.png";
        case HidNpadButton_X: return "img/buttons/x.png";
        case HidNpadButton_Y: return "img/buttons/y.png";
        case HidNpadButton_L: return "img/buttons/l.png";
        case HidNpadButton_R: return "img/buttons/r.png";
        case HidNpadButton_ZL: return "img/buttons/zl.png";
        case HidNpadButton_ZR: return "img/buttons/zr.png";
        case HidNpadButton_Plus: return "img/buttons/plus.png";
        case HidNpadButton_Minus: return "img/buttons/minus.png";
        case HidNpadButton_StickL: return "img/buttons/lstick_click.png";
        case HidNpadButton_StickR: return "img/buttons/rstick_click.png";
        case HidNpadButton_LeftSL: return "img/buttons/sl.png";
        case HidNpadButton_LeftSR: return "img/buttons/sr.png";
        case HidNpadButton_RightSL: return "img/buttons/sl.png";
        case HidNpadButton_RightSR: return "img/buttons/sr.png";
        case HidNpadButton_Up: return "img/buttons/dpad_up.png";
        case HidNpadButton_Down: return "img/buttons/dpad_down.png";
        case HidNpadButton_Left: return "img/buttons/dpad_left.png";
        case HidNpadButton_Right: return "img/buttons/dpad_right.png";
        case HidNpadButton_StickLUp: return "img/buttons/lstick_up.png";
        case HidNpadButton_StickLDown: return "img/buttons/lstick_down.png";
        case HidNpadButton_StickLLeft: return "img/buttons/lstick_left.png";
        case HidNpadButton_StickLRight: return "img/buttons/lstick_right.png";
        case HidNpadButton_StickRUp: return "img/buttons/rstick_up.png";
        case HidNpadButton_StickRDown: return "img/buttons/rstick_down.png";
        case HidNpadButton_StickRLeft: return "img/buttons/rstick_left.png";
        case HidNpadButton_StickRRight: return "img/buttons/rstick_right.png";
        default: return "";
    }
}

std::string ControllerRemapView::getPS5ButtonImagePath(uint32_t chiakiButton)
{
    switch (chiakiButton) {
        case remapKey(RemapTarget::Cross):    return "img/ps5/cross.png";
        case remapKey(RemapTarget::Moon):     return "img/ps5/circle.png";
        case remapKey(RemapTarget::Box):      return "img/ps5/square.png";
        case remapKey(RemapTarget::Pyramid):  return "img/ps5/triangle.png";
        case remapKey(RemapTarget::L1):       return "img/ps5/l1.png";
        case remapKey(RemapTarget::R1):       return "img/ps5/r1.png";
        case remapKey(RemapTarget::L2): return "img/ps5/l2.png";
        case remapKey(RemapTarget::R2): return "img/ps5/r2.png";
        case remapKey(RemapTarget::L3):       return "img/ps5/l3.png";
        case remapKey(RemapTarget::R3):       return "img/ps5/r3.png";
        case remapKey(RemapTarget::Options):  return "img/ps5/options.png";
        case remapKey(RemapTarget::Share):    return "img/ps5/share.png";
        case remapKey(RemapTarget::Touchpad): return "img/ps5/touchpad.png";
        case remapKey(RemapTarget::Ps):       return "img/ps5/ps.png";
        case SWIPE_TOUCHPAD_UP:                 return "img/ps5/swipe_up.png";
        case SWIPE_TOUCHPAD_DOWN:               return "img/ps5/swipe_down.png";
        case SWIPE_TOUCHPAD_LEFT:               return "img/ps5/swipe_left.png";
        case SWIPE_TOUCHPAD_RIGHT:              return "img/ps5/swipe_right.png";
        default: return "";
    }
}

void ControllerRemapView::enterCaptureMode(uint32_t target)
{
    currentState = State::CAPTURING;
    captureTarget = target;
    peakHeldButtons.clear();
    captureActive = true;
    waitingForRelease = true;
    anyButtonWasPressed = false;
    captureGraceFrames = 0;

    for (auto& [btn, img] : captureIcons)
        img->setAlpha(0.2f);

    std::string ps5Path = getPS5ButtonImagePath(target);
    if (!ps5Path.empty())
        captureTargetIcon->setImageFromRes(ps5Path);

    captureSwitchBox->clearViews();
    auto* waitLbl = new brls::Label();
    waitLbl->setText("akira/remap/waiting"_i18n);
    waitLbl->setFontSize(22);
    waitLbl->setTextColor(akira::ui::active().success);
    captureSwitchBox->addView(waitLbl);
    captureOverlay->setVisibility(brls::Visibility::VISIBLE);
    mainContent->setVisibility(brls::Visibility::GONE);

    this->setFocusable(true);
    this->setHideHighlight(true);
    brls::Application::giveFocus(this);
}

void ControllerRemapView::exitCaptureMode(bool save)
{
    if (save && !peakHeldButtons.empty()) {
        std::vector<uint64_t> combo(peakHeldButtons.begin(), peakHeldButtons.end());
        currentMapping[captureTarget] = combo;
        saveMappings();
    }

    currentState = State::BROWSING;
    captureActive = false;
    captureOverlay->setVisibility(brls::Visibility::GONE);
    mainContent->setVisibility(brls::Visibility::VISIBLE);
    updateAllCellDisplays();

    this->setFocusable(false);
    brls::View* focus = scrollFrame->getDefaultFocus();
    if (focus)
        brls::Application::giveFocus(focus);
}

void ControllerRemapView::updateCaptureDisplay()
{
    captureSwitchBox->clearViews();

    if (peakHeldButtons.empty()) {
        auto* waitLbl = new brls::Label();
        waitLbl->setText("akira/remap/waiting"_i18n);
        waitLbl->setFontSize(22);
        waitLbl->setTextColor(akira::ui::active().success);
        captureSwitchBox->addView(waitLbl);
    } else {
        std::vector<uint64_t> buttons(peakHeldButtons.begin(), peakHeldButtons.end());
        for (size_t j = 0; j < buttons.size(); j++) {
            if (j > 0) {
                auto* plus = new brls::Label();
                plus->setText("+");
                plus->setFontSize(20);
                plus->setMargins(0, 6, 0, 6);
                plus->setTextColor(akira::ui::active().success);
                captureSwitchBox->addView(plus);
            }
            std::string path = getButtonImagePath(buttons[j]);
            if (!path.empty()) {
                auto* icon = new brls::Image();
                icon->setImageFromRes(path);
                icon->setWidth(40);
                icon->setHeight(40);
                captureSwitchBox->addView(icon);
            } else {
                auto* lbl = new brls::Label();
                lbl->setText(hidButtonToString(buttons[j]));
                lbl->setFontSize(22);
                lbl->setTextColor(akira::ui::active().success);
                captureSwitchBox->addView(lbl);
            }
        }
    }

    for (auto& [btn, img] : captureIcons)
        img->setAlpha(peakHeldButtons.count(btn) ? 1.0f : 0.2f);
}

void ControllerRemapView::pollCaptureInput()
{
    if (!captureActive || !capturePadInitialized) return;

    padUpdate(&capturePad);
    u64 buttons = padGetButtons(&capturePad);

    if (waitingForRelease) {
        if (buttons == 0) {
            waitingForRelease = false;
        }
        return;
    }

    if (buttons != 0) {
        anyButtonWasPressed = true;
        captureGraceFrames = 0;

        static const uint64_t allButtons[] = {
            HidNpadButton_A, HidNpadButton_B, HidNpadButton_X, HidNpadButton_Y,
            HidNpadButton_L, HidNpadButton_R, HidNpadButton_ZL, HidNpadButton_ZR,
            HidNpadButton_Plus, HidNpadButton_Minus,
            HidNpadButton_StickL, HidNpadButton_StickR,
            HidNpadButton_LeftSL, HidNpadButton_LeftSR,
            HidNpadButton_RightSL, HidNpadButton_RightSR,
            HidNpadButton_Up, HidNpadButton_Down,
            HidNpadButton_Left, HidNpadButton_Right,
            HidNpadButton_StickLUp, HidNpadButton_StickLDown,
            HidNpadButton_StickLLeft, HidNpadButton_StickLRight,
            HidNpadButton_StickRUp, HidNpadButton_StickRDown,
            HidNpadButton_StickRLeft, HidNpadButton_StickRRight,
        };

        for (uint64_t btn : allButtons) {
            if (buttons & btn) {
                peakHeldButtons.insert(btn);
            }
        }

        updateCaptureDisplay();
    } else if (anyButtonWasPressed) {
        captureGraceFrames++;
        if (captureGraceFrames >= GRACE_FRAME_COUNT) {
            exitCaptureMode(true);
        }
    }
}

void ControllerRemapView::draw(NVGcontext* vg, float x, float y, float width, float height,
                                brls::Style style, brls::FrameContext* ctx)
{
    if (currentState == State::CAPTURING) {
        pollCaptureInput();
    } else if (capturePadInitialized) {
        padUpdate(&capturePad);
        u64 buttons = padGetButtons(&capturePad);
        if (buttons & HidNpadButton_X) {
            xHoldFrames++;
            if (!xWasHeld && xHoldFrames >= LONG_PRESS_FRAMES) {
                xWasHeld = true;
                brls::View* focused = brls::Application::getCurrentFocus();
                for (size_t i = 0; i < detailCells.size(); i++) {
                    if (detailCells[i] == focused) {
                        resetSingleToDefault(remappableButtons[i].chiakiButton);
                        break;
                    }
                }
            }
        } else {
            xHoldFrames = 0;
            xWasHeld = false;
        }
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

void ControllerRemapView::resetAllToDefaults()
{
    currentMapping = settings->getDefaultButtonMapping();
    saveMappings();

    settings->setTouchpadEnabled(true);
    settings->setSwipeUpEnabled(true);
    settings->setSwipeDownEnabled(true);
    settings->setSwipeLeftEnabled(true);
    settings->setSwipeRightEnabled(true);
    settings->writeFile();

    for (auto& [btn, toggle] : enableToggles)
        toggle->setOn(true, true);

    updateAllCellDisplays();
}

void ControllerRemapView::setStreamMode(bool enabled)
{
    auto* img = (brls::Image*)this->getView("remap/controllerImage");
    if (img && enabled) {
        img->setVisibility(brls::Visibility::GONE);
        img->getParent()->setWidth(0);
        this->setPaddingLeft(200);
        this->setPaddingRight(200);
    }
}

bool ControllerRemapView::isToggleableButton(uint32_t chiakiButton)
{
    return chiakiButton == remapKey(RemapTarget::Touchpad)
        || chiakiButton == SWIPE_TOUCHPAD_UP
        || chiakiButton == SWIPE_TOUCHPAD_DOWN
        || chiakiButton == SWIPE_TOUCHPAD_LEFT
        || chiakiButton == SWIPE_TOUCHPAD_RIGHT;
}

void ControllerRemapView::resetSingleToDefault(uint32_t chiakiButton)
{
    auto defaults = settings->getDefaultButtonMapping();
    auto it = defaults.find(chiakiButton);
    if (it != defaults.end()) {
        currentMapping[chiakiButton] = it->second;
    } else {
        currentMapping[chiakiButton] = {};
    }
    saveMappings();
    updateAllCellDisplays();
}

bool ControllerRemapView::isDefaultMapping(uint32_t chiakiButton) const
{
    auto defaults = settings->getDefaultButtonMapping();
    auto defIt = defaults.find(chiakiButton);
    auto curIt = currentMapping.find(chiakiButton);

    std::vector<uint64_t> defVec;
    std::vector<uint64_t> curVec;

    if (defIt != defaults.end()) defVec = defIt->second;
    if (curIt != currentMapping.end()) curVec = curIt->second;

    std::vector<uint64_t> defSorted = defVec;
    std::vector<uint64_t> curSorted = curVec;
    std::sort(defSorted.begin(), defSorted.end());
    std::sort(curSorted.begin(), curSorted.end());

    return defSorted == curSorted;
}

bool ControllerRemapView::hasConflict(uint32_t chiakiButton) const
{
    auto it = currentMapping.find(chiakiButton);
    if (it == currentMapping.end() || it->second.empty())
        return false;

    std::vector<uint64_t> mySorted = it->second;
    std::sort(mySorted.begin(), mySorted.end());

    for (const auto& [otherButton, otherMapping] : currentMapping) {
        if (otherButton == chiakiButton || otherMapping.empty())
            continue;
        std::vector<uint64_t> otherSorted = otherMapping;
        std::sort(otherSorted.begin(), otherSorted.end());
        if (mySorted == otherSorted)
            return true;
    }
    return false;
}

std::vector<uint64_t> ControllerRemapView::getDefaultForButton(uint32_t chiakiButton) const
{
    auto defaults = settings->getDefaultButtonMapping();
    auto it = defaults.find(chiakiButton);
    if (it != defaults.end()) return it->second;
    return {};
}

std::string ControllerRemapView::formatMapping(const std::vector<uint64_t>& buttons)
{
    if (buttons.empty()) return "akira/remap/not_mapped"_i18n;

    std::string result;
    for (size_t i = 0; i < buttons.size(); i++) {
        if (i > 0) result += " + ";
        result += hidButtonToString(buttons[i]);
    }
    return result;
}

std::string ControllerRemapView::hidButtonToString(uint64_t button)
{
    switch (button) {
        case HidNpadButton_A: return "A";
        case HidNpadButton_B: return "B";
        case HidNpadButton_X: return "X";
        case HidNpadButton_Y: return "Y";
        case HidNpadButton_L: return "L";
        case HidNpadButton_R: return "R";
        case HidNpadButton_ZL: return "ZL";
        case HidNpadButton_ZR: return "ZR";
        case HidNpadButton_Plus: return "Plus";
        case HidNpadButton_Minus: return "Minus";
        case HidNpadButton_StickL: return "L Stick";
        case HidNpadButton_StickR: return "R Stick";
        case HidNpadButton_LeftSL: return "SL(L)";
        case HidNpadButton_LeftSR: return "SR(L)";
        case HidNpadButton_RightSL: return "SL(R)";
        case HidNpadButton_RightSR: return "SR(R)";
        case HidNpadButton_Up: return "D-Up";
        case HidNpadButton_Down: return "D-Down";
        case HidNpadButton_Left: return "D-Left";
        case HidNpadButton_Right: return "D-Right";
        case HidNpadButton_StickLUp: return "LS-Up";
        case HidNpadButton_StickLDown: return "LS-Down";
        case HidNpadButton_StickLLeft: return "LS-Left";
        case HidNpadButton_StickLRight: return "LS-Right";
        case HidNpadButton_StickRUp: return "RS-Up";
        case HidNpadButton_StickRDown: return "RS-Down";
        case HidNpadButton_StickRLeft: return "RS-Left";
        case HidNpadButton_StickRRight: return "RS-Right";
        default: return "?";
    }
}

uint64_t ControllerRemapView::stringToHidButton(const std::string& name)
{
    if (name == "A") return HidNpadButton_A;
    if (name == "B") return HidNpadButton_B;
    if (name == "X") return HidNpadButton_X;
    if (name == "Y") return HidNpadButton_Y;
    if (name == "L") return HidNpadButton_L;
    if (name == "R") return HidNpadButton_R;
    if (name == "ZL") return HidNpadButton_ZL;
    if (name == "ZR") return HidNpadButton_ZR;
    if (name == "Plus") return HidNpadButton_Plus;
    if (name == "Minus") return HidNpadButton_Minus;
    if (name == "L Stick") return HidNpadButton_StickL;
    if (name == "R Stick") return HidNpadButton_StickR;
    if (name == "SL(L)") return HidNpadButton_LeftSL;
    if (name == "SR(L)") return HidNpadButton_LeftSR;
    if (name == "SL(R)") return HidNpadButton_RightSL;
    if (name == "SR(R)") return HidNpadButton_RightSR;
    if (name == "D-Up") return HidNpadButton_Up;
    if (name == "D-Down") return HidNpadButton_Down;
    if (name == "D-Left") return HidNpadButton_Left;
    if (name == "D-Right") return HidNpadButton_Right;
    if (name == "LS-Up") return HidNpadButton_StickLUp;
    if (name == "LS-Down") return HidNpadButton_StickLDown;
    if (name == "LS-Left") return HidNpadButton_StickLLeft;
    if (name == "LS-Right") return HidNpadButton_StickLRight;
    if (name == "RS-Up") return HidNpadButton_StickRUp;
    if (name == "RS-Down") return HidNpadButton_StickRDown;
    if (name == "RS-Left") return HidNpadButton_StickRLeft;
    if (name == "RS-Right") return HidNpadButton_StickRRight;
    return 0;
}

std::string ControllerRemapView::chiakiButtonToConfigKey(uint32_t button)
{
    switch (button) {
        case remapKey(RemapTarget::Cross): return "cross";
        case remapKey(RemapTarget::Moon): return "circle";
        case remapKey(RemapTarget::Box): return "square";
        case remapKey(RemapTarget::Pyramid): return "triangle";
        case remapKey(RemapTarget::L1): return "l1";
        case remapKey(RemapTarget::R1): return "r1";
        case remapKey(RemapTarget::L2): return "l2";
        case remapKey(RemapTarget::R2): return "r2";
        case remapKey(RemapTarget::L3): return "l3";
        case remapKey(RemapTarget::R3): return "r3";
        case remapKey(RemapTarget::Options): return "options";
        case remapKey(RemapTarget::Share): return "share";
        case remapKey(RemapTarget::Touchpad): return "touchpad";
        case remapKey(RemapTarget::Ps): return "ps";
        case SWIPE_TOUCHPAD_UP: return "swipe_up";
        case SWIPE_TOUCHPAD_DOWN: return "swipe_down";
        case SWIPE_TOUCHPAD_LEFT: return "swipe_left";
        case SWIPE_TOUCHPAD_RIGHT: return "swipe_right";
        default: return "";
    }
}

uint32_t ControllerRemapView::configKeyToChiakiButton(const std::string& key)
{
    if (key == "cross") return remapKey(RemapTarget::Cross);
    if (key == "circle") return remapKey(RemapTarget::Moon);
    if (key == "square") return remapKey(RemapTarget::Box);
    if (key == "triangle") return remapKey(RemapTarget::Pyramid);
    if (key == "l1") return remapKey(RemapTarget::L1);
    if (key == "r1") return remapKey(RemapTarget::R1);
    if (key == "l2") return remapKey(RemapTarget::L2);
    if (key == "r2") return remapKey(RemapTarget::R2);
    if (key == "l3") return remapKey(RemapTarget::L3);
    if (key == "r3") return remapKey(RemapTarget::R3);
    if (key == "options") return remapKey(RemapTarget::Options);
    if (key == "share") return remapKey(RemapTarget::Share);
    if (key == "touchpad") return remapKey(RemapTarget::Touchpad);
    if (key == "ps") return remapKey(RemapTarget::Ps);
    if (key == "swipe_up") return SWIPE_TOUCHPAD_UP;
    if (key == "swipe_down") return SWIPE_TOUCHPAD_DOWN;
    if (key == "swipe_left") return SWIPE_TOUCHPAD_LEFT;
    if (key == "swipe_right") return SWIPE_TOUCHPAD_RIGHT;
    return 0;
}
