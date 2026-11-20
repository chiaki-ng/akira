#include "app/qr_matrix.hpp"

#include <qrcodegen.hpp>

#include <string>
#include <utility>

namespace akira::app {

QrMatrix::QrMatrix(int side, std::vector<bool> modules)
    : side(side)
    , modules(std::move(modules))
{
}

auto QrMatrix::encode(std::string_view text) -> std::optional<QrMatrix>
{
    try
    {
        const qrcodegen::QrCode code = qrcodegen::QrCode::encodeText(std::string(text).c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        const int side = code.getSize();
        std::vector<bool> modules(static_cast<std::size_t>(side * side));
        for (int row = 0; row < side; ++row)
            for (int column = 0; column < side; ++column)
                modules[static_cast<std::size_t>(row * side + column)] = code.getModule(column, row);
        return QrMatrix(side, std::move(modules));
    }
    catch (const qrcodegen::data_too_long&)
    {
        return std::nullopt;
    }
}

auto QrMatrix::dark(int column, int row) const -> bool
{
    if (column < 0 || row < 0 || column >= side || row >= side)
        return false;
    return modules[static_cast<std::size_t>(row * side + column)];
}

}
