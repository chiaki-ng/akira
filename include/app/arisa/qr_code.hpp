#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/qr_matrix.hpp"

namespace akira::app::arisa {

class QrCode final : public ::arisa::composition::Component {
public:
    static constexpr float radius = 14.0F;

    QrCode(QrMatrix matrix, float side);

    bool operator==(const QrCode& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    QrMatrix matrix;
    float side;
};

}
