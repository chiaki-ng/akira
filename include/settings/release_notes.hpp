#pragma once

#include <string>
#include <string_view>

namespace akira::settings {

class ReleaseNotes {
public:
    explicit ReleaseNotes(std::string_view published);

    const std::string& readable() const { return text; }
    bool empty() const { return text.empty(); }

private:
    static constexpr std::string_view footerOpen = "<!-- akira-footer -->";
    static constexpr std::string_view footerClose = "<!-- /akira-footer -->";
    static constexpr std::string_view commentOpen = "<!--";
    static constexpr std::string_view commentClose = "-->";
    static constexpr std::string_view bullet = "• ";

    static std::string withoutFooter(std::string_view body);
    static std::string withoutComments(std::string_view body);
    static std::string withoutImages(std::string_view line);
    static std::string linked(std::string_view line);
    static std::string reshaped(std::string_view body);
    static std::string_view trimmed(std::string_view line);
    static bool isRule(std::string_view line);
    static bool isFence(std::string_view line);
    static bool isTableRow(std::string_view line);

    std::string text;
};

}
