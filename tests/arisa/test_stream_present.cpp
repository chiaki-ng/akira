#include "test_util.hpp"

#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "app/arisa/notices.hpp"
#include "app/arisa/overlay_handoff.hpp"
#include "app/arisa/stream_stage.hpp"
#include "play/present_loop.hpp"

namespace {

namespace provider = arisa::provider;
namespace runtime = arisa::runtime;
using namespace std::chrono_literals;
using akira::app::arisa::Notices;
using akira::app::arisa::OverlayHandoff;
using akira::app::arisa::StreamStage;
using akira::play::PresentLoop;
using akira::play::PresentTiming;
using provider::headless::NullGpu;

constexpr std::uint32_t width = 160;
constexpr std::uint32_t height = 90;
constexpr provider::SurfaceSize extent{ .width = provider::SurfaceWidth{ width },
    .height = provider::SurfaceHeight{ height } };

class DecodedSurface {
public:
    auto storage() const -> const std::byte* { return bytes.data(); }
    auto colour() const -> provider::Colour { return provider::Colour{ .red = 0.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F }; }

private:
    std::array<std::byte, width * height * 4> bytes{};
};

class NullPresenter final : public PresentLoop::Target {
public:
    NullPresenter(NullGpu& gpu, OverlayHandoff& handoff, const DecodedSurface& surface)
        : gpu(gpu)
        , handoff(handoff)
        , surface(surface)
    {
    }

    auto present(PresentTiming::Kind, PresentTiming&) -> std::expected<PresentLoop::Presented, PresentTiming::Miss> override
    {
        if (!takeover && !seize())
            return std::unexpected(PresentTiming::Miss::unavailable);
        const auto index = takeover->acquire();
        if (!index)
            return std::unexpected(PresentTiming::Miss::unavailable);
        NullGpu::FrameCanvas canvas = takeover->target(*index);
        bound.push_back(surface.storage());
        if (canvas)
            canvas->get().fill(surface.colour());
        if (overlay)
            if (const auto recorded = overlay->record(canvas); recorded && *recorded)
                ++overlayRecords;
        const auto submitted = PresentLoop::Clock::now();
        static_cast<void>(takeover->present(*index));
        return PresentLoop::Presented{ .submitted = submitted, .presented = PresentLoop::Clock::now() };
    }

    void swapInterval(std::uint32_t interval) override { intervals.push_back(interval); }

    auto freshScenes() const -> std::uint64_t { return overlay ? overlay->statistics().fresh : 0; }

    void handBack()
    {
        overlay.reset();
        takeover.reset();
    }

    std::vector<const std::byte*> bound;
    std::vector<std::uint32_t> intervals;
    int overlayRecords = 0;

private:
    auto seize() -> bool
    {
        auto taken = gpu.take_over();
        if (!taken)
        {
            handoff.refuse("arisa refused the GPU takeover: " + taken.error().explanation());
            return false;
        }
        takeover.emplace(std::move(*taken));
        auto opened = takeover->overlay();
        if (!opened)
            return false;
        overlay.emplace(std::move(*opened));
        handoff.offer(overlay->frames(), extent);
        return true;
    }

    NullGpu& gpu;
    OverlayHandoff& handoff;
    const DecodedSurface& surface;
    std::optional<NullGpu::Takeover> takeover;
    std::optional<NullGpu::Overlay> overlay;
};

class StageRig {
public:
    StageRig()
    {
        providers.surface().resize(extent);
        providers.surface().set_scale(provider::SurfaceScale::unit());
        providers.gpu().rasterize(extent);
        providers.gpu().swapchain(2, true);
        auto created = runtime::Application::create(providers, arisa::components::Text("console list"));
        if (created)
            application.emplace(std::move(*created));
        frames(2);
        if (application)
            stage.emplace(*application, providers, handoff, notices);
    }

    void frames(int count)
    {
        for (int index = 0; index < count && application; ++index)
        {
            static_cast<void>(providers.clock().advance(16ms));
            static_cast<void>(application->frame());
            if (stage)
                stage->follow();
        }
    }

    provider::headless::HeadlessProviderFactory providers;
    OverlayHandoff handoff;
    Notices notices;
    DecodedSurface decoded;
    std::optional<runtime::Application> application;
    std::optional<StreamStage> stage;
};

}

TEST(stream_present_takes_over_embeds_the_overlay_and_copies_no_frame)
{
    StageRig rig;
    REQUIRE(rig.application.has_value());
    REQUIRE(rig.stage.has_value());
    NullPresenter presenter(rig.providers.gpu(), rig.handoff, rig.decoded);
    {
        PresentLoop loop(presenter, PresentLoop::Pacing{ .stallRepaint = 10s });
        loop.frameReady();
        CHECK_EQ(rig.providers.gpu().takeover_counters().granted, 1u);
        CHECK(!rig.stage->embedded());
        rig.frames(2);
        REQUIRE(rig.stage->embedded());
        CHECK(&rig.stage->overlays() != &rig.application->root().overlays());
        CHECK(rig.application->last_frame().taken_over);

        std::optional<runtime::OverlayHandle> hud = rig.stage->overlays().show(arisa::composition::OverlayLayer::hud,
            arisa::components::VStack()
                .frame({ .width = 40.0F, .height = 30.0F })
                .background(arisa::theme::Colour::hex(0xC00000)),
            runtime::OverlayBehaviour{});
        rig.notices.notify("Screenshot saved");
        int ready = 1;
        for (int step = 0; step < 4; ++step)
        {
            rig.frames(1);
            loop.frameReady();
            ++ready;
        }
        CHECK(presenter.freshScenes() >= 3);

        for (int frame = 0; frame < 30; ++frame)
        {
            loop.frameReady();
            ++ready;
            if (frame % 5 == 0)
                rig.frames(1);
        }
        const PresentTiming::Totals totals = loop.totals();
        CHECK_EQ(totals.frames, static_cast<std::uint64_t>(ready));
        CHECK_EQ(totals.copies, 0u);
        CHECK_EQ(totals.copiedBytes, 0u);
        CHECK_EQ(presenter.bound.size(), static_cast<std::size_t>(ready));
        bool everyFrameBoundInPlace = true;
        for (const std::byte* storage : presenter.bound)
            everyFrameBoundInPlace = everyFrameBoundInPlace && storage == rig.decoded.storage();
        CHECK(everyFrameBoundInPlace);
        CHECK(presenter.overlayRecords > 0);
        REQUIRE(!presenter.intervals.empty());
        CHECK_EQ(presenter.intervals.front(), 0u);

        hud.reset();
        rig.stage->close();
        CHECK(!rig.stage->embedded());
        CHECK(&rig.stage->overlays() == &rig.application->root().overlays());
        loop.stop();
    }
    presenter.handBack();
    CHECK_EQ(rig.providers.gpu().takeover_counters().returned, 1u);
    rig.frames(2);
    CHECK(!rig.application->last_frame().taken_over);
}

TEST(stream_present_reports_a_refused_takeover_to_the_stage)
{
    StageRig rig;
    REQUIRE(rig.stage.has_value());
    auto held = rig.providers.gpu().take_over();
    REQUIRE(held.has_value());
    NullPresenter presenter(rig.providers.gpu(), rig.handoff, rig.decoded);
    PresentLoop loop(presenter, PresentLoop::Pacing{ .stallRepaint = 10s });
    loop.frameReady();
    CHECK_EQ(loop.totals().unavailable, 1u);
    rig.frames(1);
    const std::optional<std::string> refused = rig.stage->takeRefusal();
    REQUIRE(refused.has_value());
    CHECK(refused->find("refused the GPU takeover") != std::string::npos);
    CHECK(!rig.stage->embedded());
}

TEST(stream_present_detaches_when_the_overlay_closes_underneath)
{
    StageRig rig;
    REQUIRE(rig.stage.has_value());
    NullPresenter presenter(rig.providers.gpu(), rig.handoff, rig.decoded);
    PresentLoop loop(presenter, PresentLoop::Pacing{ .stallRepaint = 10s });
    loop.frameReady();
    rig.frames(2);
    REQUIRE(rig.stage->embedded());
    loop.stop();
    presenter.handBack();
    rig.frames(2);
    CHECK(!rig.stage->embedded());
    CHECK(&rig.stage->overlays() == &rig.application->root().overlays());
}
