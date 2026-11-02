#include "views/benchmark_view.hpp"
#include "core/chiaki_core.hpp"
#include "ui/theme.hpp"

#include <array>
#include <cstddef>
#include <vector>

#include <borealis/core/i18n.hpp>
using namespace brls::literals;

namespace platform = libchiaking::platform;

BenchmarkView::BenchmarkView()
{
    this->inflateFromXMLRes("xml/views/benchmark_view.xml");

    logContainer = (brls::Box*)this->getView("benchmark/logs");
    scrollFrame = (brls::ScrollingFrame*)this->getView("benchmark/scroll");
    closeBtn = (brls::Button*)this->getView("benchmark/close");

    closeBtn->registerClickAction([](brls::View* view) {
        brls::Application::popActivity();
        return true;
    });

    setFocusable(true);
}

BenchmarkView::~BenchmarkView()
{
    static_cast<void>(benchmarkThread.request_stop());
    benchmarkThread = platform::Thread{};
}

brls::View* BenchmarkView::create()
{
    return new BenchmarkView();
}

void BenchmarkView::startBenchmark()
{
    benchmarkFinished = false;
    statusShown = false;

    akira::ChiakiCore& core = akira::ChiakiCore::current();
    auto started = platform::Thread::start(core.threading(), "benchmark", platform::ThreadRole::crypto,
        [this, &crypto = core.crypto(), &clock = core.clock()](platform::StopToken stop) {
            run(crypto, clock, stop);
        });
    if (!started) {
        benchmarkFinished = true;
        addLogLine("akira/benchmark/error_thread"_i18n);
        return;
    }
    benchmarkThread = std::move(started).value();
}

void BenchmarkView::addLogLine(const std::string& line)
{
    std::lock_guard<std::mutex> lock(logMutex);
    logLines.push_back(line);
    while (logLines.size() > MAX_LOG_LINES) {
        logLines.pop_front();
    }
    logsNeedUpdate = true;
}

void BenchmarkView::updateLogDisplay()
{
    if (!logsNeedUpdate) return;

    std::lock_guard<std::mutex> lock(logMutex);

    std::string combinedText;
    for (size_t i = 0; i < logLines.size(); i++) {
        if (i > 0) combinedText += "\n";
        combinedText += logLines[i];
    }

    if (logContainer->getChildren().empty()) {
        auto* label = new brls::Label();
        label->setFontSize(18);
        label->setTextColor(akira::ui::active().textMuted);
        logContainer->addView(label);
    }

    auto* label = dynamic_cast<brls::Label*>(logContainer->getChildren().front());
    if (label) {
        label->setText(combinedText);
    }

    needsScrollToBottom = true;
    logsNeedUpdate = false;
}

void BenchmarkView::showCompletion()
{
    if (statusShown || !benchmarkFinished)
        return;
    statusShown = true;
    if (auto* status = dynamic_cast<brls::Label*>(getView("benchmark/status")))
        status->setText("akira/benchmark/complete"_i18n);
}

void BenchmarkView::run(platform::CryptoProvider& crypto, const platform::ClockProvider& clock,
                        const platform::StopToken& stop)
{
    addLogLine("akira/benchmark/header_line"_i18n);
    addLogLine("akira/benchmark/results_title"_i18n);
    addLogLine("akira/benchmark/header_line"_i18n);
    addLogLine("");

    addLogLine(brls::getStr("akira/benchmark/iterations", ITERATIONS));
    addLogLine("");

    const std::array<std::byte, 16> key{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
        std::byte{0x05}, std::byte{0x06}, std::byte{0x07}, std::byte{0x08},
        std::byte{0x09}, std::byte{0x0a}, std::byte{0x0b}, std::byte{0x0c},
        std::byte{0x0d}, std::byte{0x0e}, std::byte{0x0f}, std::byte{0x10}};
    const std::array<std::byte, 12> iv{};

    for (const size_t size : {size_t{64}, size_t{256}, size_t{1024}, size_t{4096}}) {
        if (stop.stop_requested())
            break;

        const std::vector<std::byte> data(size, std::byte{0xAB});
        std::array<std::byte, 16> tag{};

        addLogLine("akira/benchmark/separator"_i18n);
        addLogLine(brls::getStr("akira/benchmark/testing_blocks", size));
        addLogLine("");
        addLogLine(brls::getStr("akira/benchmark/pmull_running", ITERATIONS));

        const uint64_t started = clock.now_microseconds();
        int completed = 0;
        for (; completed < ITERATIONS && !stop.stop_requested(); completed++) {
            if (!crypto.aes128_gmac(key, iv, data, tag)) {
                addLogLine("akira/benchmark/error_gmac"_i18n);
                break;
            }
        }
        const double elapsedMs = static_cast<double>(clock.now_microseconds() - started) / 1000.0;
        if (completed == 0 || elapsedMs <= 0.0)
            break;

        const double ops = (completed * 1000.0) / elapsedMs;
        const double megabytes = (completed * static_cast<double>(size) * 1000.0) / (elapsedMs * 1024.0 * 1024.0);
        addLogLine(brls::getStr("akira/benchmark/pmull_result", ops, megabytes, elapsedMs));
        addLogLine("");
    }

    addLogLine("akira/benchmark/header_line"_i18n);
    addLogLine("akira/benchmark/complete_title"_i18n);
    addLogLine("akira/benchmark/header_line"_i18n);

    benchmarkFinished = true;
}

void BenchmarkView::draw(NVGcontext* vg, float x, float y, float width, float height,
                          brls::Style style, brls::FrameContext* ctx)
{
    updateLogDisplay();
    showCompletion();

    Box::draw(vg, x, y, width, height, style, ctx);

    if (needsScrollToBottom && scrollFrame && logContainer) {
        float contentHeight = logContainer->getHeight();
        float frameHeight = scrollFrame->getHeight();
        if (contentHeight > frameHeight) {
            scrollFrame->setContentOffsetY(-(contentHeight - frameHeight), false);
        }
        needsScrollToBottom = false;
    }
}
