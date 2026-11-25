#pragma once

#include <borealis.hpp>

#include "log/views/log_pane.hpp"

namespace akira::consoles {
class Host;
}

namespace akira::consoles::views {

class AutoRegistrationScreen : public brls::Box {
public:
    explicit AutoRegistrationScreen(bool showStages);
    ~AutoRegistrationScreen() override;

    static void start(Host* host);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

private:
    static void show(Host* host);
    static void showStage(int index, int total);
    static void dismiss();

    static inline brls::Activity* progress = nullptr;
    static inline brls::Label* stageLabel = nullptr;

    bool showStages;
    log::views::LogPane logPane;
};

}
