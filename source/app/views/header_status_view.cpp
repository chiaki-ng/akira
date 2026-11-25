#include "app/views/header_status_view.hpp"

#include <borealis/views/widgets/battery.hpp>
#include <borealis/views/widgets/wireless.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace akira::app::views {

HeaderStatusView::HeaderStatusView()
{
    this->setAxis(brls::Axis::ROW);
    this->setAlignItems(brls::AlignItems::CENTER);

    brls::Platform* platform = brls::Application::getPlatform();

    auto* battery = new brls::BatteryWidget();
    battery->setVisibility(platform->canShowBatteryLevel() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    battery->setMarginRight(21);
    this->addView(battery);

    auto* wireless = new brls::WirelessWidget();
    wireless->setVisibility(platform->canShowWirelessLevel() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    wireless->setMarginRight(21);
    this->addView(wireless);

    timeLabel = new brls::Label();
    timeLabel->setVerticalAlign(brls::VerticalAlign::CENTER);
    timeLabel->setFontSize(21.5f);
    this->addView(timeLabel);
}

void HeaderStatusView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
    brls::FrameContext* ctx)
{
    auto in_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto tm        = *std::localtime(&in_time_t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");
    if (ss.str() != lastTime)
    {
        lastTime = ss.str();
        timeLabel->setText(lastTime);
    }
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

}
