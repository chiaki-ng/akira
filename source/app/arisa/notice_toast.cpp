#include "app/arisa/notice_toast.hpp"

#include <arisa/components/text.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/toast_options.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <optional>
#include <utility>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

auto NoticeToast::attach(composition::Element content, Notices& notices, bool streaming) -> composition::Element
{
    Notices* held = &notices;
    composition::Binding<std::optional<Notices::Notice>> shown([held] { return held->view().get(); },
        [held](std::optional<Notices::Notice> value) {
            if (!value)
                held->clear();
        });
    composition::ToastOptions placement{ .alignment = composition::Alignment::top };
    if (streaming)
        placement.tint = theme::ColourValue(theme::ColourRole::orange).opacity(streamingOpacity);
    return std::move(content).toast(std::move(shown), std::chrono::seconds{ 3 }, placement,
        [](const Notices::Notice& notice) { return components::Text(notice.text).font(theme::Font::subheadline); });
}

}
