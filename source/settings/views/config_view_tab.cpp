#include "settings/views/config_view_tab.hpp"
#include "app/views/theme.hpp"
#include <fstream>
#include <string_view>
#include <vector>

#include <borealis/core/i18n.hpp>

namespace akira::settings::views {

using namespace brls::literals;

static const std::vector<std::string> sensitiveKeys = {
    "account_id", "online_id", "access_token", "refresh_token", "npsso", "duid",
    "rp_key", "rp_regist_key"
};

namespace {

enum class TomlToken { Header, Key, Punct, String, Number, Boolean, Comment, Plain };

NVGcolor tokenColor(TomlToken token) {
    const app::views::Palette& palette = app::views::active();
    switch (token) {
        case TomlToken::Header:  return palette.accent;
        case TomlToken::Key:     return palette.text;
        case TomlToken::Punct:   return palette.textDim;
        case TomlToken::String:  return palette.success;
        case TomlToken::Number:  return palette.warning;
        case TomlToken::Boolean: return palette.media;
        case TomlToken::Comment: return palette.textDim;
        default:                 return palette.textMuted;
    }
}

brls::Label* makeToken(const std::string& text, TomlToken token) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(18);
    label->setTextColor(tokenColor(token));
    return label;
}

TomlToken classifyScalar(std::string_view value) {
    if (value.empty())
        return TomlToken::Plain;

    const char first = value.front();
    if (first == '\'' || first == '"')
        return TomlToken::String;
    if (value.starts_with("true") || value.starts_with("false"))
        return TomlToken::Boolean;
    if (first == '-' || first == '+' || (first >= '0' && first <= '9'))
        return TomlToken::Number;
    return TomlToken::Plain;
}

// Arrays are written inline by toml++, so they colour as whatever they hold:
// rtts = [ 1 ] reads as a number, favorites = [ 'EP...' ] as a string.
TomlToken classifyValue(std::string_view value) {
    if (!value.starts_with('['))
        return classifyScalar(value);

    size_t elem = value.find_first_not_of(" \t", 1);
    if (elem == std::string_view::npos || value[elem] == ']')
        return TomlToken::Punct;
    return classifyScalar(value.substr(elem));
}

std::string_view trimmed(std::string_view text) {
    size_t begin = text.find_first_not_of(" \t");
    if (begin == std::string_view::npos)
        return {};
    return text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
}

std::string_view indentOf(std::string_view line) {
    size_t begin = line.find_first_not_of(" \t");
    return line.substr(0, begin == std::string_view::npos ? line.size() : begin);
}

brls::Box* makeRow() {
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    return row;
}

} // anonymous namespace

ConfigViewTab::ConfigViewTab() {
    this->inflateFromXMLRes("xml/tabs/config_view_tab.xml");

    revealBtn->registerClickAction([this](brls::View* view) {
        credentialsRevealed = !credentialsRevealed;
        revealBtn->setText(credentialsRevealed ? "akira/config_view/hide_secrets"_i18n : "akira/config_view/reveal_secrets"_i18n);
        configContainer->clearViews();
        loadConfigFromFile();
        return true;
    });

    loadConfigFromFile();
}

brls::View* ConfigViewTab::create() {
    return new ConfigViewTab();
}

std::string ConfigViewTab::censorValue(const std::string& value) {
    if (value.length() <= 5) return value;
    return "****" + value.substr(value.length() - 5);
}

std::string ConfigViewTab::processLine(const std::string& line) {
    if (credentialsRevealed) return line;

    for (const auto& key : sensitiveKeys) {
        size_t keyPos = line.find(key);
        if (keyPos == std::string::npos) continue;

        size_t eqPos = line.find('=', keyPos);
        if (eqPos == std::string::npos) continue;

        size_t valueStart = line.find_first_not_of(" \t", eqPos + 1);
        if (valueStart == std::string::npos) continue;

        char quote = 0;
        if (line[valueStart] == '\'' || line[valueStart] == '"') {
            quote = line[valueStart];
            valueStart++;
        }

        size_t valueEnd = line.length();
        if (quote != 0 && valueEnd > valueStart && line[valueEnd - 1] == quote)
            valueEnd--;
        if (valueStart >= valueEnd) continue;

        std::string censored = censorValue(line.substr(valueStart, valueEnd - valueStart));
        std::string head = line.substr(0, eqPos + 1) + " ";
        if (quote != 0)
            return head + quote + censored + quote;
        return head + censored;
    }
    return line;
}

void ConfigViewTab::loadConfigFromFile() {
    std::ifstream file("sdmc:/switch/akira/akira.toml");

    if (!file.is_open()) {
        addLine("akira/config_view/file_not_found"_i18n, false);
        addLine("akira/config_view/file_path"_i18n, false);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (trimmed(line).empty()) {
            auto* spacer = new brls::Box();
            spacer->setHeight(8);
            configContainer->addView(spacer);
        } else {
            addTomlLine(processLine(line));
        }
    }
}

void ConfigViewTab::addTomlLine(const std::string& line) {
    const std::string indent(indentOf(line));
    std::string_view body = trimmed(line);

    if (body.starts_with('#')) {
        configContainer->addView(makeToken(indent + std::string(body), TomlToken::Comment));
        return;
    }

    if (body.starts_with('[') && body.ends_with(']')) {
        const size_t depth = body.starts_with("[[") ? 2 : 1;
        std::string_view name = body.substr(depth, body.size() - depth * 2);

        auto* row = makeRow();
        row->addView(makeToken(indent + std::string(body.substr(0, depth)), TomlToken::Punct));

        // Dotted paths read as segments: [cloud.datacenters.pscloud] accents each
        // name and dims the separators, so the nesting is visible at a glance.
        size_t segment = 0;
        while (segment <= name.size()) {
            size_t dot = name.find('.', segment);
            size_t end = dot == std::string_view::npos ? name.size() : dot;
            row->addView(makeToken(std::string(name.substr(segment, end - segment)),
                TomlToken::Header));
            if (dot == std::string_view::npos)
                break;
            row->addView(makeToken(".", TomlToken::Punct));
            segment = dot + 1;
        }

        row->addView(makeToken(std::string(body.substr(body.size() - depth)), TomlToken::Punct));
        row->setMarginTop(12);
        row->setMarginBottom(4);
        configContainer->addView(row);
        return;
    }

    const size_t eqPos = body.find('=');
    if (eqPos == std::string_view::npos) {
        configContainer->addView(makeToken(indent + std::string(body), TomlToken::Plain));
        return;
    }

    std::string_view key = trimmed(body.substr(0, eqPos));
    std::string_view value = trimmed(body.substr(eqPos + 1));

    auto* row = makeRow();
    row->addView(makeToken(indent + std::string(key), TomlToken::Key));
    row->addView(makeToken(" = ", TomlToken::Punct));

    auto* valueLabel = makeToken(std::string(value), classifyValue(value));
    valueLabel->setGrow(1.0f);
    row->addView(valueLabel);

    configContainer->addView(row);
}

void ConfigViewTab::addLine(const std::string& text, bool isHeader) {
    auto* label = makeToken(text, isHeader ? TomlToken::Header : TomlToken::Plain);

    if (isHeader) {
        label->setMarginTop(12);
        label->setMarginBottom(4);
    }

    configContainer->addView(label);
}

}
