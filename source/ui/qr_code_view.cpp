#include "ui/qr_code_view.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace akira::ui {

QrCodeView::QrCodeView(qrcodegen::QrCode code, float side)
    : code(std::move(code))
{
    setWidth(side);
    setHeight(side);
    setFocusable(false);
}

auto QrCodeView::make(std::string_view text, float side) -> QrCodeView*
{
    auto code = encode(text);
    if (!code)
        return nullptr;
    return new QrCodeView(std::move(*code), side);
}

void QrCodeView::openDialog(std::string_view link, const std::string& body)
{
    auto* content = new brls::Box(brls::Axis::COLUMN);
    content->setAlignItems(brls::AlignItems::CENTER);
    content->setPadding(30, 40, 30, 40);
    if (auto* qr = link.empty() ? nullptr : make(link, 280))
    {
        qr->setMarginBottom(20);
        content->addView(qr);
    }
    auto* label = new brls::Label();
    label->setText(body);
    label->setFontSize(brls::Application::getStyle()["brls/dialog/fontSize"]);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);
    content->addView(label);

    auto* dialog = new brls::Dialog(content);
    dialog->addButton(brls::getStr("akira/common/ok"), [] {});
    dialog->open();
}

auto QrCodeView::encode(std::string_view text) -> std::optional<qrcodegen::QrCode>
{
    try
    {
        return qrcodegen::QrCode::encodeText(std::string(text).c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    }
    catch (const qrcodegen::data_too_long&)
    {
        return std::nullopt;
    }
}

void QrCodeView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style,
                      brls::FrameContext*)
{
    const int size = code.getSize();
    const float side = std::min(width, height);
    const float module = std::floor(side / static_cast<float>(size + quietModules * 2));
    const float codeSide = module * static_cast<float>(size + quietModules * 2);
    const float left = std::round(x + (width - codeSide) / 2.0f);
    const float top = std::round(y + (height - codeSide) / 2.0f);

    nvgBeginPath(vg);
    nvgRect(vg, left, top, codeSide, codeSide);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);

    nvgBeginPath(vg);
    const float origin = static_cast<float>(quietModules) * module;
    for (int row = 0; row < size; ++row)
    {
        int column = 0;
        while (column < size)
        {
            if (!code.getModule(column, row))
            {
                ++column;
                continue;
            }
            const int start = column;
            while (column < size && code.getModule(column, row))
                ++column;
            nvgRect(vg, left + origin + static_cast<float>(start) * module, top + origin + static_cast<float>(row) * module,
                    static_cast<float>(column - start) * module, module);
        }
    }
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
}

} // namespace akira::ui
