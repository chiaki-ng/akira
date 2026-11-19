#include "log/app_log.hpp"

namespace akira::log {

void AppLog::open(const std::string& path)
{
    auto& current = redirect();
    if (current)
        return;
    current = Log::process().redirect(path, Buffering::line);
    if (!current)
        return;
    log::info("File logging enabled: {}", path);
}

void AppLog::close()
{
    redirect().reset();
}

auto AppLog::redirect() -> std::optional<Log::Redirect>&
{
    Log::process();
    static std::optional<Log::Redirect> current;
    return current;
}

}
