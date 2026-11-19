#include "log/views/log_pane.hpp"
#include "log/log.hpp"

#include "app/views/theme.hpp"

#include <chrono>
#include <ctime>
#include <format>

namespace akira::ui {

LogPane::~LogPane()
{
    unsubscribe();
}

void LogPane::subscribe()
{
    if (subscription)
        return;

    subscription = akira::log::Log::process().subscribe(
        [this](const akira::log::Entry& entry) {
            this->addLine(std::string(entry.message));
        });
}

void LogPane::unsubscribe()
{
    subscription.reset();
}

void LogPane::addLine(const std::string& line)
{
    std::lock_guard<std::mutex> lock(linesMutex);

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm time_tm = *std::localtime(&tt);

    lines.push_back(std::format("{:02}:{:02}:{:02}.{:03} {}",
        time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec, static_cast<int>(ms), line));

    while (lines.size() > MAX_LINES) {
        lines.pop_front();
    }
}

void LogPane::render(NVGcontext* vg, float x, float y, float width, float height)
{
    std::lock_guard<std::mutex> lock(linesMutex);

    nvgSave(vg);
    nvgScissor(vg, x, y, width, height);

    nvgFontSize(vg, 16);
    nvgFillColor(vg, akira::ui::active().textMuted);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    float padding = 20;
    float maxTextWidth = width - padding * 2;
    float availableHeight = height - padding * 2;

    size_t startIdx = lines.size();
    float totalHeight = 0;
    while (startIdx > 0) {
        float bounds[4];
        nvgTextBoxBounds(vg, 0, 0, maxTextWidth, lines[startIdx - 1].c_str(), nullptr, bounds);
        float lineH = bounds[3] - bounds[1];
        if (totalHeight + lineH > availableHeight)
            break;
        totalHeight += lineH;
        startIdx--;
    }

    float currentY = y + padding;
    for (size_t i = startIdx; i < lines.size(); i++) {
        nvgTextBox(vg, x + padding, currentY, maxTextWidth, lines[i].c_str(), nullptr);

        float bounds[4];
        nvgTextBoxBounds(vg, x + padding, currentY, maxTextWidth, lines[i].c_str(), nullptr, bounds);
        currentY += (bounds[3] - bounds[1]);
    }

    nvgRestore(vg);
}

} // namespace akira::ui
