#pragma once

#include <concepts>
#include <coroutine>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <arisa/async/job.hpp>
#include <arisa/async/task.hpp>
#include <arisa/async/task_promise.hpp>
#include <arisa/components/url_request.hpp>
#include <arisa/components/url_session.hpp>

#include "app/art_source.hpp"

namespace akira::app::arisa {

class ArtSession {
public:
    using Bytes = ::arisa::components::UrlSession::Bytes;

    explicit ArtSession(ArtSource& source);

    auto session() const -> const ::arisa::components::UrlSession& { return shared; }

    static auto request(std::string_view url) -> std::optional<::arisa::components::UrlRequest>;

private:
    class Fetch {
    public:
        using Slot = std::shared_ptr<std::optional<ArtSource::Bytes>>;

        Fetch(ArtSource& source, std::string url);

        auto await_ready() const noexcept -> bool { return false; }

        template <class Promise>
            requires std::derived_from<Promise, ::arisa::async::TaskPromiseBase>
        auto await_suspend(std::coroutine_handle<Promise> handle) -> bool
        {
            return handle.promise().park(handle, [this](::arisa::async::Work work) { return begin(std::move(work)); },
                job);
        }

        auto await_resume() const -> std::optional<ArtSource::Bytes> { return std::move(*slot); }

    private:
        auto begin(::arisa::async::Work work) -> ::arisa::async::Job;

        std::reference_wrapper<ArtSource> source;
        std::string url;
        Slot slot = std::make_shared<std::optional<ArtSource::Bytes>>();
        std::optional<::arisa::async::Job> job;
    };

    static auto load(std::reference_wrapper<ArtSource> source, ::arisa::components::UrlRequest request)
        -> ::arisa::async::Task<Bytes>;

    ::arisa::components::UrlSession shared;
};

}
