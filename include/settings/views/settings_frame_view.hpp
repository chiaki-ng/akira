#ifndef AKIRA_SETTINGS_FRAME_VIEW_HPP
#define AKIRA_SETTINGS_FRAME_VIEW_HPP

#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class SettingsFrameView : public brls::Box {
public:
    SettingsFrameView();
    ~SettingsFrameView() override;

    /*
     * Called from the Account tab's profile switcher. The rebuild is deferred with
     * brls::sync because it destroys the very view whose click handler is still on the
     * stack.
     */
    void onActiveProfileChanged();

    static SettingsFrameView* currentInstance;

private:
    struct SubTab {
        std::string name;
        std::function<brls::Box*()> make;
    };
    struct Menu {
        std::string label;
        std::vector<SubTab> subs;
    };

    std::vector<Menu> menus;
    int activeMenu = 0;
    int activeSub = 0;

    brls::Box* menuBar = nullptr;
    brls::Box* subBar = nullptr;
    brls::Box* contentHolder = nullptr;
    brls::Box* descPanel = nullptr;
    brls::Label* descTitle = nullptr;
    brls::Label* descBody = nullptr;
    brls::Image* descImage = nullptr;

    brls::GenericEvent::Subscription focusSub;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    brls::Animatable contentFade{1.0f};
    brls::Animatable entryAnim{1.0f};

    void buildMenus();
    void renderMenuBar();
    void renderSubBar();
    void loadContent(bool focus);
    void switchMenu(int delta);
    void switchSub(int delta);
    void updateDescriptionFromFocus();
    void showDescPanel(bool show);
};

#endif // AKIRA_SETTINGS_FRAME_VIEW_HPP
