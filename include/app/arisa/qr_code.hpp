#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/path.hpp>

#include "app/qr_matrix.hpp"

namespace akira::app::arisa {

class QrCode final : public ::arisa::composition::Component {
public:
    class Modules {
    public:
        explicit Modules(QrMatrix matrix);

        auto path(::arisa::composition::Rect rect) const -> ::arisa::graphics::Path;

        friend bool operator==(const Modules&, const Modules&) = default;

    private:
        QrMatrix matrix;
    };

    static constexpr float radius = 14.0F;

    QrCode(QrMatrix matrix, float side);

    bool operator==(const QrCode& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    QrMatrix matrix;
    float side;
};

}
