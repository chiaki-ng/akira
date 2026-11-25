#include "app/arisa/art_session.hpp"

#include <arisa/provider/browser/url.hpp>
#include <arisa/provider/error.hpp>

#include <expected>
#include <utility>

namespace akira::app::arisa {

namespace async = ::arisa::async;
namespace components = ::arisa::components;
namespace provider = ::arisa::provider;

ArtSession::ArtSession(ArtSource& source)
    : shared([held = std::ref(source)](components::UrlRequest request) { return load(held, std::move(request)); })
{
}

auto ArtSession::request(std::string_view url) -> std::optional<components::UrlRequest>
{
    if (url.empty())
        return std::nullopt;
    auto parsed = provider::Url::create(url);
    if (!parsed)
        return std::nullopt;
    return components::UrlRequest(std::move(*parsed));
}

ArtSession::Fetch::Fetch(ArtSource& source, std::string url)
    : source(source)
    , url(std::move(url))
{
}

auto ArtSession::Fetch::begin(async::Work work) -> async::Job
{
    auto [made, ticket] = async::Job::create(std::move(work));
    source.get().fetch(url, [slot = slot, ticket = std::move(ticket)](std::optional<ArtSource::Bytes> bytes) mutable {
        *slot = std::move(bytes);
        ticket.run();
    });
    return std::move(made);
}

auto ArtSession::load(std::reference_wrapper<ArtSource> source, components::UrlRequest request) -> async::Task<Bytes>
{
    std::optional<ArtSource::Bytes> fetched =
        co_await Fetch(source.get(), std::string(request.url().value()));
    if (!fetched)
        co_return std::unexpected(provider::ProviderError{ provider::ProviderErrorCode::not_found });
    co_return std::move(*fetched);
}

}
