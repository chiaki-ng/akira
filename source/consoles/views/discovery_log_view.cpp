#include "consoles/views/discovery_log_view.hpp"

#include "consoles/discovery_manager.hpp"
#include "settings/settings_manager.hpp"
#include "app/views/theme.hpp"

#include <borealis/core/i18n.hpp>
#include <string>
#include <vector>

namespace akira::consoles::views {

using namespace brls::literals;

DiscoveryLogView::DiscoveryLogView()
{
    this->inflateFromXMLRes("xml/views/discovery_log_view.xml");

    logContainer = (brls::Box*)this->getView("discovery_log/logs");
    scrollFrame = (brls::ScrollingFrame*)this->getView("discovery_log/scroll");
    statusLabel = (brls::Label*)this->getView("discovery_log/status");
    closeBtn = (brls::Button*)this->getView("discovery_log/close");
    clearBtn = (brls::Button*)this->getView("discovery_log/clear");

    closeBtn->registerClickAction([](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    clearBtn->registerClickAction([this](brls::View*) {
        DiscoveryManager::clearDiscoveryLog();
        refreshLog();
        return true;
    });

    refreshTimer.setCallback([this]() { refreshLog(); });

    setFocusable(true);
}

DiscoveryLogView::~DiscoveryLogView()
{
    refreshTimer.stop();
}

brls::View* DiscoveryLogView::create()
{
    return new DiscoveryLogView();
}

void DiscoveryLogView::willAppear(bool resetState)
{
    Box::willAppear(resetState);
    lastVersion = UINT64_MAX;
    refreshLog();
    refreshTimer.start(300);
}

void DiscoveryLogView::willDisappear(bool resetState)
{
    refreshTimer.stop();
    Box::willDisappear(resetState);
}

void DiscoveryLogView::refreshLog()
{
    uint64_t version = DiscoveryManager::getDiscoveryLogVersion();
    if (version == lastVersion)
        return;
    lastVersion = version;

    std::vector<std::string> lines = DiscoveryManager::getDiscoveryLogSnapshot();

    if (statusLabel) {
        if (!SettingsManager::getInstance()->getDebugDiscoveryLog())
            statusLabel->setText("akira/settings/discovery_log_off"_i18n);
        else
            statusLabel->setText(std::to_string(lines.size()));
    }

    std::string combined;
    if (lines.empty()) {
        combined = "akira/settings/discovery_log_empty"_i18n;
    } else {
        for (size_t i = 0; i < lines.size(); i++) {
            if (i > 0)
                combined += "\n";
            combined += lines[i];
        }
    }

    if (logContainer->getChildren().empty()) {
        auto* label = new brls::Label();
        label->setFontSize(16);
        label->setTextColor(app::views::active().textMuted);
        logContainer->addView(label);
    }

    auto* label = dynamic_cast<brls::Label*>(logContainer->getChildren().front());
    if (label)
        label->setText(combined);

    needsScrollToBottom = true;
}

void DiscoveryLogView::draw(NVGcontext* vg, float x, float y, float width, float height,
                            brls::Style style, brls::FrameContext* ctx)
{
    Box::draw(vg, x, y, width, height, style, ctx);

    if (needsScrollToBottom && scrollFrame && logContainer) {
        float contentHeight = logContainer->getHeight();
        float frameHeight = scrollFrame->getHeight();
        if (contentHeight > frameHeight)
            scrollFrame->setContentOffsetY(-(contentHeight - frameHeight), false);
        needsScrollToBottom = false;
    }
}

}
