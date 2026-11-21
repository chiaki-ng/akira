#include "app/arisa/stream_stage.hpp"

#include <type_traits>
#include <utility>
#include <variant>

#include "app/arisa/stage_screen.hpp"
#include "log/log.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;
namespace runtime = ::arisa::runtime;

StreamStage::StreamStage(runtime::Application& application, provider::ProviderFactory& providers,
    OverlayHandoff& handoff, Notices& notices)
    : application(application)
    , providers(providers)
    , handoff(handoff)
    , notices(notices)
{
}

StreamStage::~StreamStage()
{
    embedding.reset();
}

void StreamStage::follow()
{
    if (embedding && !embedding->attached())
    {
        log::info("[STAGE] the overlay closed underneath the stage; detached");
        embedding.reset();
    }
    std::optional<OverlayHandoff::Event> event = handoff.take();
    if (!event)
        return;
    std::visit(
        [this](auto&& arrived) {
            using Arrived = std::decay_t<decltype(arrived)>;
            if constexpr (std::is_same_v<Arrived, OverlayHandoff::Offer>)
                embed(std::move(arrived));
            else
                refusal = std::move(arrived.reason);
        },
        std::move(*event));
}

void StreamStage::embed(OverlayHandoff::Offer offer)
{
    embedding.reset();
    if (!stage)
        stage.emplace(StageScreen(notices), providers, application.executors());
    auto embedded = application.embed(*stage, std::move(offer.frames), offer.extent);
    if (!embedded)
    {
        refusal = "the overlay could not embed: " + embedded.error().explanation();
        log::error("[STAGE] {}", *refusal);
        return;
    }
    embedding.emplace(std::move(*embedded));
    log::info("[STAGE] stream overlay embedded at {}x{}", offer.extent.width.value(), offer.extent.height.value());
}

void StreamStage::close()
{
    if (!embedding)
        return;
    embedding.reset();
    log::info("[STAGE] stream overlay detached");
}

auto StreamStage::embedded() const -> bool
{
    return embedding && embedding->attached();
}

auto StreamStage::takeRefusal() -> std::optional<std::string>
{
    return std::exchange(refusal, std::nullopt);
}

auto StreamStage::target() -> runtime::Root&
{
    if (embedded())
        return *stage;
    return application.root();
}

auto StreamStage::overlays() -> runtime::Overlays&
{
    return target().overlays();
}

auto StreamStage::blockInput() -> runtime::InputBlock
{
    if (embedded())
        return stage->block_input();
    return application.block_input();
}

auto StreamStage::passInput(provider::Buttons buttons, runtime::PassedInput passed) -> runtime::InputPassthrough
{
    if (embedded())
        return stage->pass_input(buttons, passed);
    return application.pass_input(buttons, passed);
}

auto StreamStage::captureButtons() -> runtime::ButtonCapture
{
    if (embedded())
        return stage->capture_buttons();
    return application.capture_buttons();
}

}
