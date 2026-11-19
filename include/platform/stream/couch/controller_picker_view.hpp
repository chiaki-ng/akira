#ifndef AKIRA_CONTROLLER_PICKER_VIEW_HPP
#define AKIRA_CONTROLLER_PICKER_VIEW_HPP

#include <borealis.hpp>
#include <switch.h>

#include <functional>
#include <vector>

#include "platform/pads/extended/pad_path.hpp"

namespace akira::play {
class InputManager;
}

namespace akira::platform::couch
{

class ControllerPickerView : public brls::Box {
public:
    using OnChosen = std::function<void(HidNpadIdType)>;

    using Describe = std::function<std::vector<akira::input::PadDescription>()>;

    static constexpr HidNpadIdType kNoChoice  = static_cast<HidNpadIdType>(0xFF);

    static constexpr HidNpadIdType kCancelled = static_cast<HidNpadIdType>(0xFE);

    static bool worthAsking(const std::vector<akira::input::PadDescription>& pads);

    ControllerPickerView(std::vector<akira::input::PadDescription> pads,
                         OnChosen onChosen, Describe describe);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override { return this; }

    bool isTranslucent() override { return true; }

private:
    void choose(std::size_t index);

    void cancel();

    void rebuild(std::vector<akira::input::PadDescription> fresh);

    int carryOverSelection(bool hadSelection, HidNpadIdType selNpad,
                           akira::input::PadPathKind selKind,
                           uint16_t selVid, uint16_t selPid) const;
    void buildCards();
    static bool SameSet(const std::vector<akira::input::PadDescription>& a,
                        const std::vector<akira::input::PadDescription>& b);

    void accept();

    std::vector<akira::input::PadDescription> m_pads;
    std::vector<PadState>                     m_states;
    std::vector<brls::Box*>                   m_cards;
    int                                       m_selected = -1;
    std::vector<bool>                         m_seen_clear;
    unsigned                                  m_frames = 0;
    OnChosen                                  m_onChosen;
    Describe                                  m_describe;
    brls::Box*                                m_panel = nullptr;
    brls::Box*                                m_row = nullptr;
    bool                                      m_done = false;
};


}

#endif // AKIRA_CONTROLLER_PICKER_VIEW_HPP
