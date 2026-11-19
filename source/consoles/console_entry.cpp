#include "consoles/console_entry.hpp"

#include <utility>

namespace akira::consoles {

ConsoleEntry::ConsoleEntry(ConsoleFacts facts, bool linkTargetsExist)
    : known(std::move(facts))
    , linkTargetsExist(linkTargetsExist)
{
}

std::string ConsoleEntry::displayName() const
{
    std::string name = known.name;
    if (remote() && name.length() > remoteSuffix.length() && name.ends_with(remoteSuffix))
        name.resize(name.length() - remoteSuffix.length());
    return name;
}

ConsoleEntry::Badge ConsoleEntry::badge() const
{
    switch (known.type) {
        case HostType::Remote: return Badge::Remote;
        case HostType::Manual: return Badge::Manual;
        case HostType::Auto: return Badge::Auto;
        default: return Badge::Local;
    }
}

ConsoleEntry::Status ConsoleEntry::status() const
{
    if (offersLink())
        return Status::Link;
    if (usesInternetPairing())
        return Status::Ready;
    if (offersRegistration())
        return Status::Register;
    if (known.standby && known.paired)
        return Status::Standby;
    if ((known.paired && known.ready) || (remote() && known.paired))
        return Status::Ready;
    return Status::Presence;
}

bool ConsoleEntry::dimmed() const
{
    return status() != Status::Ready;
}

std::optional<ConsoleEntry::Action> ConsoleEntry::primaryAction() const
{
    if (offersLink())
        return Action::Link;
    if (usesInternetPairing())
        return Action::Connect;
    if (offersRegistration())
        return Action::Register;
    if (known.standby && known.paired && !remote())
        return Action::Wake;
    if (known.paired)
        return Action::Connect;
    return std::nullopt;
}

std::vector<ConsoleEntry::Option> ConsoleEntry::options() const
{
    std::vector<Option> offered;
    if (known.paired && !remote())
        offered.push_back(Option::ConsolePin);
    if (!known.cloud)
        offered.push_back(known.hasAddress ? Option::SetAddress : Option::AddAddress);
    if (known.paired && !known.cloud)
        offered.push_back(Option::TakionVersion);
    if (known.paired)
        offered.push_back(Option::Delete);
    return offered;
}

bool ConsoleEntry::offersLink() const
{
    return remote() && known.needsLink && (linkTargetsExist || !known.canPairOverInternet);
}

bool ConsoleEntry::offersRegistration() const
{
    return !known.paired && (known.discovered || known.canPairOverInternet);
}

}
