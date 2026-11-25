#ifndef AKIRA_UI_LOG_PANE_HPP
#define AKIRA_UI_LOG_PANE_HPP

#include "log/log.hpp"

#include <borealis.hpp>

#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace akira::log::views {

class LogPane {
public:
    ~LogPane();

    void subscribe();
    void unsubscribe();

    void addLine(const std::string& line);
    void render(NVGcontext* vg, float x, float y, float width, float height);

private:
    std::deque<std::string> lines;
    std::mutex linesMutex;
    static constexpr size_t MAX_LINES = 100;

    std::optional<Log::Subscription> subscription;
};

} // namespace akira::log::views

#endif // AKIRA_UI_LOG_PANE_HPP
