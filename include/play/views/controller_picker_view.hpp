#ifndef AKIRA_CONTROLLER_PICKER_VIEW_HPP
#define AKIRA_CONTROLLER_PICKER_VIEW_HPP

#include <borealis.hpp>
#include <switch.h>

#include <functional>
#include <string>
#include <vector>

#include "platform/pads/pad.hpp"

namespace akira::play {
class InputManager;
}

namespace akira::play::views
{

class ControllerPickerView : public brls::Box {
public:
    using OnChosen = std::function<void(HidNpadIdType)>;

    using Describe = std::function<std::vector<akira::platform::PadDescription>()>;

    static constexpr HidNpadIdType kNoChoice  = static_cast<HidNpadIdType>(0xFF);

    static constexpr HidNpadIdType kCancelled = static_cast<HidNpadIdType>(0xFE);

    static bool worthAsking(const std::vector<akira::platform::PadDescription>& pads);

    ControllerPickerView(std::vector<akira::platform::PadDescription> pads,
                         OnChosen onChosen, Describe describe);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override { return this; }

    bool isTranslucent() override { return true; }

private:
    void choose(std::size_t index);

    void cancel();

    void rebuild(std::vector<akira::platform::PadDescription> fresh);

    int carryOverSelection(bool hadSelection, HidNpadIdType selNpad,
                           akira::platform::PadPathKind selKind,
                           uint16_t selVid, uint16_t selPid) const;
    void buildCards();
    static bool SameSet(const std::vector<akira::platform::PadDescription>& a,
                        const std::vector<akira::platform::PadDescription>& b);
    static bool Differ(const akira::platform::PadDescription& a, const akira::platform::PadDescription& b);
    static std::vector<std::string> CapabilityLabels(const akira::platform::PadDescription& d);
    static brls::Box* MakeCapabilityChip(const std::string& text);
    static std::string SlotLine(HidNpadIdType npad);

    void accept();

    std::vector<akira::platform::PadDescription> m_pads;
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
