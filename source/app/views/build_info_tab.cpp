#include "app/views/build_info_tab.hpp"
#include "app/views/theme.hpp"
#include <fstream>
#include <sstream>

#include <borealis/core/i18n.hpp>
using namespace brls::literals;

BuildInfoTab::BuildInfoTab() {
    this->inflateFromXMLRes("xml/tabs/build_info_tab.xml");
    loadBuildInfo();
}

brls::View* BuildInfoTab::create() {
    return new BuildInfoTab();
}

void BuildInfoTab::loadBuildInfo() {
    std::ifstream file("romfs:/build_info.txt");

    if (!file.is_open()) {
        addInfoRow("akira/build_info/load_error"_i18n, true);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            // Add spacer for empty lines
            auto* spacer = new brls::Box();
            spacer->setHeight(10);
            infoContainer->addView(spacer);
        } else if (line.find("===") != std::string::npos) {
            // Header line
            addInfoRow(line, true);
        } else {
            addInfoRow(line, false);
        }
    }
}

void BuildInfoTab::addInfoRow(const std::string& text, bool isHeader) {
    auto* label = new brls::Label();
    label->setText(text);

    if (isHeader) {
        label->setFontSize(22);
        label->setTextColor(akira::ui::active().accent);
        label->setMarginTop(20);
        label->setMarginBottom(10);
    } else {
        label->setFontSize(22);
        label->setTextColor(akira::ui::active().textMuted);
    }

    infoContainer->addView(label);
}
