#include "app/arisa/qr_code.hpp"

#include <arisa/components/group.hpp>
#include <arisa/components/qr_code.hpp>

#include <cstddef>
#include <optional>
#include <utility>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;

QrCode::QrCode(QrMatrix matrix, float side)
    : matrix(std::move(matrix))
    , side(side)
{
}

auto QrCode::body(composition::Context&) const -> composition::Element
{
    std::optional<components::QrCode::Matrix> modules =
        components::QrCode::Matrix::create(static_cast<std::size_t>(matrix.size()), matrix.modules());
    if (!modules)
        return composition::Element(components::Group()).frame({ .width = side, .height = side });
    return composition::Element(components::QrCode(std::move(*modules),
                                    components::QrCodeOptions{ .quiet_zone = QrMatrix::quietModules, .corner_radius = radius }))
        .frame({ .width = side, .height = side });
}

}
