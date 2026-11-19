#pragma once

#include <optional>

#include <libchiaking/detail/expected.hpp>
#include <libchiaking/detail/play/nat_type.hpp>

namespace akira::network {

class NatReport {
public:
    enum class Tone { Good, Warning, Bad };

    explicit NatReport(libchiaking::Expected<libchiaking::play::NatType, libchiaking::play::NatTypeFailure> result);

    bool found() const { return result.has_value(); }
    std::optional<int> type() const;
    const char* detailKey() const;
    Tone tone() const;

private:
    libchiaking::Expected<libchiaking::play::NatType, libchiaking::play::NatTypeFailure> result;
};

}
