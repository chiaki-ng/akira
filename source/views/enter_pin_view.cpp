#include "views/enter_pin_view.hpp"
#include "ui/theme.hpp"
#include <format>

#include <borealis/core/i18n.hpp>
using namespace brls::literals;

EnterPinView::EnterPinView(Host* host, PinViewType type, bool isError)
    : host(host)
    , type(type)
    , isError(isError)
{
    setFocusable(true);

    this->registerAction("Back", brls::ControllerButton::BUTTON_B, [this](brls::View* view) {
        brls::Logger::info("EnterPinView: Back pressed");
        closePinView();
        return true;
    });
}

EnterPinView::~EnterPinView()
{
}

brls::View* EnterPinView::create()
{
    return nullptr;
}

int EnterPinView::getMaxPinLength() const
{
    switch (type)
    {
        case PinViewType::Registration:
            return 8;
        case PinViewType::Login:
            return 4;
        case PinViewType::CouchPasscode:
            return 4;
        default:
            return 8;
    }
}

std::string EnterPinView::getTitle() const
{
    switch (type)
    {
        case PinViewType::Registration:
            if (isError)
            {
                return "akira/pin/registration_error"_i18n;
            }
            return "akira/pin/registration_prompt"_i18n;
        case PinViewType::Login:
            if (isError)
            {
                return "akira/pin/login_error"_i18n;
            }
            return "akira/pin/login_prompt"_i18n;
        case PinViewType::CouchPasscode:
            if (isError)
            {
                return "akira/pin/couch_passcode_error"_i18n;
            }
            return "akira/pin/couch_passcode_prompt"_i18n;
        default:
            return "akira/pin/enter_pin"_i18n;
    }
}

void EnterPinView::showPinDialog()
{
    int maxLen = getMaxPinLength();
    std::string title = getTitle();
    std::string hint = brls::getStr("akira/pin/digits_hint", maxLen);

    ASYNC_RETAIN
    bool success = brls::Application::getImeManager()->openForNumber(
        [ASYNC_TOKEN, maxLen](long pinValue) {
            ASYNC_RELEASE

            std::string pinStr = std::to_string(pinValue);
            if (pinStr.length() < (size_t)maxLen) {
                pinStr.insert(0, maxLen - pinStr.length(), '0');
            }
            this->pin = pinStr;

            brls::Logger::info("PIN entered: {}", pin);

            if (onPinEntered)
            {
                onPinEntered(pin);
            }

            brls::Application::popActivity();
        },
        title,
        hint,
        maxLen,
        ""   // initial text
    );

    if (!success)
    {
        brls::Logger::info("Keyboard cancelled, closing PIN view");
        // Need to release the token we retained above
        ASYNC_RELEASE
        if (onCancel)
        {
            onCancel();
        }
        brls::Application::popActivity();
    }
}

void EnterPinView::closePinView()
{
    brls::Logger::info("EnterPinView: closePinView called");
    if (onCancel)
    {
        onCancel();
    }
    brls::Application::unblockInputs();
    brls::Application::popActivity();
}

void EnterPinView::draw(NVGcontext* vg, float x, float y, float width, float height,
                         brls::Style style, brls::FrameContext* ctx)
{
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillColor(vg, akira::ui::withAlpha(akira::ui::active().background, 240));
    nvgFill(vg);

    nvgFontSize(vg, 24);
    nvgFillColor(vg, akira::ui::active().text);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + width / 2, y + height / 2 - 20, getTitle().c_str(), nullptr);

    nvgFontSize(vg, 16);
    nvgFillColor(vg, akira::ui::active().textMuted);
    std::string instruction = "akira/pin/press_a"_i18n;
    nvgText(vg, x + width / 2, y + height / 2 + 20, instruction.c_str(), nullptr);

    if (!pinDialogShown)
    {
        pinDialogShown = true;
        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            showPinDialog();
        });
    }
}
