#include "app/arisa/stage_screen.hpp"

#include <arisa/components/group.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/modifiers.hpp>

#include <utility>

#include "app/arisa/notice_toast.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;

StageScreen::StageScreen(Notices& notices)
    : notices(&notices)
{
}

auto StageScreen::body(composition::Context& context) const -> composition::Element
{
    static_cast<void>(context.read(notices->view()));
    composition::Element stage = components::ZStack(
        components::ZStackOptions{ .alignment = composition::Alignment::top_leading },
        components::Group().frame({ .max_width = composition::infinity, .max_height = composition::infinity }))
                                     .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
                                     .allows_hit_testing(false);
    return NoticeToast::attach(std::move(stage), *notices, true);
}

}
