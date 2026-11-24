#include "consoles/console_list.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace akira::consoles {

ConsoleList::ConsoleList(std::vector<ConsoleFacts> consoles, Profile profile)
    : consoles(std::move(consoles))
    , profile(profile)
{
}

ConsoleList::Profile ConsoleList::profileFor(bool hasProfiles, bool legacy)
{
    if (!hasProfiles)
        return Profile::None;
    return legacy ? Profile::Legacy : Profile::Signed;
}

std::vector<ConsoleEntry> ConsoleList::shown() const
{
    std::vector<ConsoleEntry> entries;
    if (!hasProfile())
        return entries;
    const bool targets = std::ranges::any_of(consoles, &ConsoleList::linkable);
    for (const ConsoleFacts& facts : consoles) {
        if (lists(facts) && !folded(facts))
            entries.emplace_back(facts, targets, internetFor(facts));
    }
    return entries;
}

std::vector<std::string> ConsoleList::linkTargets() const
{
    std::vector<std::string> names;
    for (const ConsoleFacts& facts : consoles) {
        if (linkable(facts))
            names.push_back(facts.catalogName);
    }
    return names;
}

ConsoleEntry ConsoleList::entry(ConsoleFacts facts) const
{
    return ConsoleEntry(std::move(facts), std::ranges::any_of(consoles, &ConsoleList::linkable));
}

bool ConsoleList::lists(const ConsoleFacts& facts) const
{
    if (!facts.paired && !facts.discovered && facts.type != HostType::Manual)
        return false;
    /*
     * A profile converted from a normal one keeps its "(Remote)" entries in the
     * toml. They are inert without a token, but they would still list and then
     * dead-end on click.
     */
    return !(facts.type == HostType::Remote && profile == Profile::Legacy);
}

std::optional<ConsoleFacts> ConsoleList::internetFor(const ConsoleFacts& home) const
{
    if (profile == Profile::Legacy || home.type == HostType::Remote || !home.paired)
        return std::nullopt;
    const auto found = std::ranges::find_if(consoles, [&home](const ConsoleFacts& facts) {
        return facts.type == HostType::Remote && facts.paired && facts.catalogName == home.catalogName + std::string(ConsoleEntry::remoteSuffix);
    });
    if (found == consoles.end())
        return std::nullopt;
    return *found;
}

bool ConsoleList::folded(const ConsoleFacts& facts) const
{
    if (facts.type != HostType::Remote || !facts.catalogName.ends_with(ConsoleEntry::remoteSuffix))
        return false;
    const std::string home = facts.catalogName.substr(0, facts.catalogName.size() - ConsoleEntry::remoteSuffix.size());
    return std::ranges::any_of(consoles, [this, &home](const ConsoleFacts& candidate) {
        return candidate.catalogName == home && internetFor(candidate).has_value();
    });
}

bool ConsoleList::linkable(const ConsoleFacts& facts)
{
    return facts.paired && facts.type != HostType::Remote;
}

}
