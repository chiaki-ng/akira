#include "util/app_log.hpp"

namespace akira {

void AppLog::open(const std::string& path)
{
    auto& current = redirect();
    if (current)
        return;
    current = log::Log::process().redirect(path, log::Buffering::line);
    if (!current)
        return;
    log::info("File logging enabled: {}", path);
}

void AppLog::close()
{
    redirect().reset();
}

auto AppLog::redirect() -> std::optional<log::Log::Redirect>&
{
    log::Log::process();
    static std::optional<log::Log::Redirect> current;
    return current;
}

}
