#ifndef AKIRA_ENTER_PIN_VIEW_HPP
#define AKIRA_ENTER_PIN_VIEW_HPP

#include <borealis.hpp>
#include <functional>

#include "core/host.hpp"

enum class PinViewType {
    Registration,
    Login,
    CouchPasscode
};

class EnterPinView : public brls::Box {
public:
    using PinCallback = std::function<void(const std::string& pin)>;
    using CancelCallback = std::function<void()>;

    EnterPinView(Host* host, PinViewType type, bool isError = false);
    ~EnterPinView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    void setOnPinEntered(PinCallback callback) { onPinEntered = std::move(callback); }
    void setOnCancel(CancelCallback callback) { onCancel = std::move(callback); }

    static brls::View* create();

private:
    Host* host = nullptr;
    PinViewType type;
    bool isError = false;
    bool pinDialogShown = false;
    std::string pin;

    PinCallback onPinEntered;
    CancelCallback onCancel;

    void showPinDialog();
    void closePinView();
    int getMaxPinLength() const;
    std::string getTitle() const;
};

#endif // AKIRA_ENTER_PIN_VIEW_HPP
