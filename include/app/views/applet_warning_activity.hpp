#pragma once

#include <borealis.hpp>

namespace akira::app::views {

class AppletWarningActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/applet_warning.xml");
};

}
