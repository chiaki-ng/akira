#pragma once

#include <functional>
#include <memory>

namespace akira::app {

class UiWindow {
public:
    class FocusWatch {
    public:
        virtual ~FocusWatch() = default;
    };

    virtual ~UiWindow() = default;

    virtual auto width() const -> unsigned = 0;
    virtual auto height() const -> unsigned = 0;
    virtual void suspendRendering() = 0;
    virtual auto watchFocus(std::function<void(bool focused)> changed) -> std::unique_ptr<FocusWatch> = 0;
};

}
