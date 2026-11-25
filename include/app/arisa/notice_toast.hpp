#pragma once

#include <arisa/composition/element.hpp>

#include "app/arisa/notices.hpp"

namespace akira::app::arisa {

class NoticeToast {
public:
    static constexpr float streamingOpacity = 0.85F;

    static auto attach(::arisa::composition::Element content, Notices& notices, bool streaming)
        -> ::arisa::composition::Element;
};

}
