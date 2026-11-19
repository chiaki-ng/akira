#ifndef AKIRA_UI_QR_CODE_VIEW_HPP
#define AKIRA_UI_QR_CODE_VIEW_HPP

#include <borealis.hpp>

#include <optional>
#include <string>
#include <string_view>

#include <qrcodegen.hpp>

namespace akira::ui {

class QrCodeView : public brls::View {
public:
    static auto make(std::string_view text, float side) -> QrCodeView*;
    static void openDialog(std::string_view link, const std::string& body);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

private:
    explicit QrCodeView(qrcodegen::QrCode code, float side);

    static auto encode(std::string_view text) -> std::optional<qrcodegen::QrCode>;

    static constexpr int quietModules = 4;

    qrcodegen::QrCode code;
};

} // namespace akira::ui

#endif // AKIRA_UI_QR_CODE_VIEW_HPP
