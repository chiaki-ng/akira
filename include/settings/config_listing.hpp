#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace akira::settings {

class ConfigListing {
public:
    enum class Token { Header, Key, Punct, String, Number, Boolean, Comment, Plain };

    struct Piece {
        std::string text;
        Token token = Token::Plain;

        friend bool operator==(const Piece&, const Piece&) = default;
    };

    struct Line {
        std::vector<Piece> pieces;
        bool header = false;

        friend bool operator==(const Line&, const Line&) = default;
    };

    static constexpr std::array<std::string_view, 8> sensitiveKeys{ "account_id", "online_id", "access_token",
        "refresh_token", "npsso", "duid", "rp_key", "rp_regist_key" };

    static auto read(const std::filesystem::path& file, bool revealed) -> std::optional<std::vector<Line>>;
    static auto parse(std::string_view raw) -> Line;
    static auto censored(const std::string& line) -> std::string;

private:
    static auto masked(std::string_view value) -> std::string;
    static auto scalar(std::string_view value) -> Token;
    static auto value(std::string_view value) -> Token;
    static auto trimmed(std::string_view text) -> std::string_view;
    static auto indent(std::string_view line) -> std::string_view;
    static auto section(std::string indent, std::string_view body) -> Line;
};

}
