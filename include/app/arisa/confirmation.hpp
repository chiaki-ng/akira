#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>

namespace akira::app::arisa {

class Confirmation {
public:
    enum class Emphasis { Plain, Prominent, Destructive };

    struct Choice {
        std::string label;
        std::function<void()> chosen;
        Emphasis emphasis = Emphasis::Plain;
        std::string key;
        std::optional<std::string> symbol;
    };

    static constexpr float width = 480.0F;
    static constexpr float titleSize = 22.0F;
    static constexpr float messageSize = 15.0F;
    static constexpr float spacing = 16.0F;
    static constexpr float buttonSpacing = 10.0F;
    static constexpr float horizontalPadding = 28.0F;
    static constexpr float verticalPadding = 26.0F;
    static constexpr float scrim = 0.32F;

    Confirmation(std::string title, std::string message, std::vector<Choice> choices, std::function<void()> dismissed);

    auto withWidth(float shown) && -> Confirmation;
    auto focusing(::arisa::composition::Namespace::ID scope) && -> Confirmation;

    auto card(std::string key) const -> ::arisa::composition::Element;
    auto dimmed(std::string key) const -> ::arisa::composition::Element;

private:
    auto button(const Choice& choice) const -> ::arisa::composition::Element;

    std::string title;
    std::string message;
    std::vector<Choice> choices;
    std::function<void()> dismissed;
    float shownWidth = width;
    std::optional<::arisa::composition::Namespace::ID> scope;
};

}
