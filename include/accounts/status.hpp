#ifndef AKIRA_PSN_STATUS_HPP
#define AKIRA_PSN_STATUS_HPP

#include <string>

namespace psn {

enum class Status {
    Ok,
    NotLinked,
    SessionExpired,
    Offline,
    RateLimited,
    ServerError
};

inline const char* statusName(Status status)
{
    switch (status)
    {
        case Status::Ok: return "Ok";
        case Status::NotLinked: return "NotLinked";
        case Status::SessionExpired: return "SessionExpired";
        case Status::Offline: return "Offline";
        case Status::RateLimited: return "RateLimited";
        case Status::ServerError: return "ServerError";
    }
    return "Unknown";
}

struct Error {
    Status status = Status::Ok;
    std::string message;

    bool ok() const { return status == Status::Ok; }
};

} // namespace psn

#endif // AKIRA_PSN_STATUS_HPP
