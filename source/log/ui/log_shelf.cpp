#include "log/ui/log_shelf.hpp"

#include <utility>

namespace akira::log::ui {

LogShelf::LogShelf(std::filesystem::path root)
    : state(View{ .root = root, .current = root })
{
}

auto LogShelf::atRoot() const -> bool
{
    return state.get().current == state.get().root;
}

void LogShelf::open()
{
    state.update([](View& view) {
        view.current = view.root;
        view.file.reset();
        view.excerpt.reset();
        view.entries = LogFolder::list(view.current);
    });
}

void LogShelf::enter(const LogFolder::Entry& entry)
{
    state.update([&entry](View& view) {
        if (entry.directory)
        {
            view.current = entry.path;
            view.file.reset();
            view.excerpt.reset();
            view.entries = LogFolder::list(view.current);
            return;
        }
        view.file = entry.path;
        view.excerpt = LogExcerpt::read(entry.path);
    });
}

auto LogShelf::back() -> bool
{
    if (state.get().file)
    {
        state.update([](View& view) {
            view.file.reset();
            view.excerpt.reset();
        });
        return true;
    }
    if (atRoot())
        return false;
    state.update([](View& view) {
        view.current = view.current.parent_path();
        view.entries = LogFolder::list(view.current);
    });
    return true;
}

void LogShelf::refresh()
{
    state.update([](View& view) {
        if (view.file)
            view.excerpt = LogExcerpt::read(*view.file);
        else
            view.entries = LogFolder::list(view.current);
    });
}

}
