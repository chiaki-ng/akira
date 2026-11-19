#pragma once

#include <borealis/core/event.hpp>

#include "app/ui_window.hpp"

namespace akira::app::views {

class BorealisWindow : public UiWindow {
public:
    auto width() const -> unsigned override;
    auto height() const -> unsigned override;
    void suspendRendering() override;
    auto watchFocus(std::function<void(bool focused)> changed) -> std::unique_ptr<FocusWatch> override;

private:
    class FocusSubscription final : public FocusWatch {
    public:
        explicit FocusSubscription(std::function<void(bool focused)> changed);
        ~FocusSubscription() override;

        FocusSubscription(const FocusSubscription&) = delete;
        FocusSubscription& operator=(const FocusSubscription&) = delete;

    private:
        brls::Event<bool>::Subscription subscription;
    };
};

}
