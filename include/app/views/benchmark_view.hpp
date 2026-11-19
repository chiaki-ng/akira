#ifndef AKIRA_BENCHMARK_VIEW_HPP
#define AKIRA_BENCHMARK_VIEW_HPP

#include <borealis.hpp>
#include <libchiaking/platform/platform.hpp>
#include <atomic>
#include <deque>
#include <mutex>

namespace libchiaking::platform {
class ClockProvider;
class CryptoProvider;
}

class BenchmarkView : public brls::Box {
public:
    BenchmarkView();
    ~BenchmarkView() override;

    void startBenchmark();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override { return this; }

    static brls::View* create();

private:
    brls::Box* logContainer = nullptr;
    brls::ScrollingFrame* scrollFrame = nullptr;
    brls::Button* closeBtn = nullptr;
    std::deque<std::string> logLines;
    std::mutex logMutex;
    static constexpr size_t MAX_LOG_LINES = 200;
    static constexpr int ITERATIONS = 10000;

    libchiaking::platform::Thread benchmarkThread;
    std::atomic<bool> benchmarkFinished{false};
    bool statusShown = false;

    std::atomic<bool> logsNeedUpdate{false};
    bool needsScrollToBottom = false;

    void addLogLine(const std::string& line);
    void updateLogDisplay();
    void showCompletion();

    void run(libchiaking::platform::CryptoProvider& crypto, const libchiaking::platform::ClockProvider& clock,
             const libchiaking::platform::StopToken& stop);
};

#endif // AKIRA_BENCHMARK_VIEW_HPP
