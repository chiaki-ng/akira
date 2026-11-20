#pragma once

#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "consoles/recent_play.hpp"

namespace akira::consoles::ui {

class RecentShelf {
public:
    struct View {
        std::string catalogName;
        std::string console;
        std::vector<RecentPlay::Title> titles;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }
    auto title(const std::string& titleId) const -> std::optional<RecentPlay::Title>;

    void show(std::string catalogName, std::string console, const RecentPlay& played);
    void clear();

private:
    ::arisa::composition::Observable<View> state;
};

}
