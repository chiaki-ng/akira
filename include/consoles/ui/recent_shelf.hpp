#pragma once

#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "consoles/recent_play.hpp"

namespace akira::consoles::ui {

class RecentShelf {
public:
    struct Banner {
        std::string catalogName;
        std::string titleId;
        std::optional<RecentPlay::Clock::time_point> lastUsed;
        std::string background;
        std::string hero;
        std::string logo;

        friend bool operator==(const Banner&, const Banner&) = default;
    };

    struct View {
        std::string catalogName;
        std::string console;
        std::vector<RecentPlay::Title> titles;
        std::optional<Banner> banner;
        std::optional<std::string> launching;
        std::vector<Banner> gallery;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }
    auto title(const std::string& titleId) const -> std::optional<RecentPlay::Title>;

    void show(std::string catalogName, std::string console, const RecentPlay& played);
    void showBanner(Banner banner);
    void clear();
    void launch(std::string titleId);
    void landed();
    auto launchArt() const -> std::optional<Banner>;
    void exhibit(Banner banner);
    auto loadingArt(const std::string& console) const -> std::vector<Banner>;

private:
    ::arisa::composition::Observable<View> state;
};

}
