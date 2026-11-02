#ifndef AKIRA_CLOUD_CLOUD_LAUNCH_HPP
#define AKIRA_CLOUD_CLOUD_LAUNCH_HPP

#include <memory>
#include <string>

#include <libchiaking/play.hpp>

namespace cloud {

class CloudLaunch {
public:
    using Started = libchiaking::Expected<std::shared_ptr<libchiaking::play::Session>, libchiaking::play::StartRefusal>;

    CloudLaunch(std::shared_ptr<libchiaking::users::PsUser> player, libchiaking::catalog::ClearedLaunch cleared, std::string name);

    auto name() const noexcept -> const std::string& { return title; }
    auto player() const noexcept -> const std::shared_ptr<libchiaking::users::PsUser>& { return user; }
    auto product() const noexcept -> const libchiaking::catalog::ProductId& { return cleared.product; }
    auto service() const noexcept -> libchiaking::catalog::ServiceGeneration { return cleared.service; }
    auto ps5() const noexcept -> bool { return cleared.service == libchiaking::catalog::ServiceGeneration::ps5_cloud; }

    [[nodiscard]] auto start(libchiaking::play::Preferences preferences, libchiaking::media::MediaSink& media,
                             libchiaking::play::SessionEvents& events) const -> Started;

    static auto refusalText(const libchiaking::play::LaunchRefused& refused, const std::string& name) -> std::string;
    static auto clearanceText(const libchiaking::catalog::LaunchRefusal& refusal, const std::string& name) -> std::string;

private:
    std::shared_ptr<libchiaking::users::PsUser> user;
    libchiaking::catalog::ClearedLaunch cleared;
    std::string title;
};

} // namespace cloud

#endif // AKIRA_CLOUD_CLOUD_LAUNCH_HPP
