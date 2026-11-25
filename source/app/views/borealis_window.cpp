#include "app/views/borealis_window.hpp"

#include <borealis/core/application.hpp>

#include <utility>

namespace akira::app::views {

auto BorealisWindow::width() const -> unsigned
{
    return brls::Application::windowWidth;
}

auto BorealisWindow::height() const -> unsigned
{
    return brls::Application::windowHeight;
}

void BorealisWindow::suspendRendering()
{
    brls::Application::setRenderSuspended(true);
}

auto BorealisWindow::watchFocus(std::function<void(bool focused)> changed) -> std::unique_ptr<FocusWatch>
{
    return std::make_unique<FocusSubscription>(std::move(changed));
}

BorealisWindow::FocusSubscription::FocusSubscription(std::function<void(bool focused)> changed)
    : subscription(brls::Application::getWindowFocusChangedEvent()->subscribe(std::move(changed)))
{
}

BorealisWindow::FocusSubscription::~FocusSubscription()
{
    brls::Application::getWindowFocusChangedEvent()->unsubscribe(subscription);
}

}
