#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "log/log_excerpt.hpp"
#include "log/log_folder.hpp"

namespace akira::log::ui {

class LogShelf {
public:
    struct View {
        std::filesystem::path root;
        std::filesystem::path current;
        std::optional<std::vector<LogFolder::Entry>> entries;
        std::optional<std::filesystem::path> file;
        std::optional<LogExcerpt> excerpt;

        friend bool operator==(const View&, const View&) = default;
    };

    explicit LogShelf(std::filesystem::path root);

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void open();
    void enter(const LogFolder::Entry& entry);
    [[nodiscard]] auto back() -> bool;
    void refresh();
    auto atRoot() const -> bool;

private:
    ::arisa::composition::Observable<View> state;
};

}
