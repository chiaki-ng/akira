#include "app/arisa/qr_code.hpp"

#include <arisa/components/shape_view.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace graphics = ::arisa::graphics;
namespace theme = ::arisa::theme;

QrCode::Modules::Modules(QrMatrix matrix)
    : matrix(std::move(matrix))
{
}

auto QrCode::Modules::path(composition::Rect rect) const -> graphics::Path
{
    graphics::Path drawn;
    const int count = matrix.size() + QrMatrix::quietModules * 2;
    if (matrix.size() <= 0)
        return drawn;
    const float module = std::floor(std::min(rect.width, rect.height) / static_cast<float>(count));
    const float extent = module * static_cast<float>(count);
    const float left = std::round(rect.x + (rect.width - extent) / 2.0F) + module * QrMatrix::quietModules;
    const float top = std::round(rect.y + (rect.height - extent) / 2.0F) + module * QrMatrix::quietModules;
    std::vector<composition::Rect> runs;
    for (int row = 0; row < matrix.size(); ++row)
    {
        int column = 0;
        while (column < matrix.size())
        {
            if (!matrix.dark(column, row))
            {
                ++column;
                continue;
            }
            const int start = column;
            while (column < matrix.size() && matrix.dark(column, row))
                ++column;
            runs.push_back(composition::Rect{ .x = left + static_cast<float>(start) * module,
                .y = top + static_cast<float>(row) * module,
                .width = static_cast<float>(column - start) * module,
                .height = module });
        }
    }
    drawn.add_rects(runs);
    return drawn;
}

QrCode::QrCode(QrMatrix matrix, float side)
    : matrix(std::move(matrix))
    , side(side)
{
}

auto QrCode::body(composition::Context&) const -> composition::Element
{
    return components::ShapeView(Modules(matrix))
        .fill(theme::Colour::hex(0x000000))
        .frame({ .width = side, .height = side })
        .background(theme::Colour::hex(0xFFFFFF), theme::Shape::rounded(radius));
}

}
