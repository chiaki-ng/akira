#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/presentation/help.hpp>

#include "app/strings.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class SettingRows {
public:
    SettingRows(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings);

    auto text(std::string_view key) const -> std::string { return strings->text(key); }

    auto toggle(std::string label, std::string_view help, std::function<bool()> read,
        std::function<void(bool)> write) const -> ::arisa::composition::Element;

    auto slider(std::string label, std::string_view help, ::arisa::controls::ClosedRange<double> range, double step,
        std::function<double()> read, std::function<void(double)> write, std::string shown) const
        -> ::arisa::composition::Element;

    auto action(std::string label, std::string symbol, std::string_view help, std::function<void()> perform) const
        -> ::arisa::composition::Element;

    auto link(std::string label, std::string detail, std::string_view help, std::function<void()> open) const
        -> ::arisa::composition::Element;

    auto value(std::string label, std::string shown) const -> ::arisa::composition::Element;

    auto note(std::string shown, bool warning) const -> ::arisa::composition::Element;

    auto section(std::string header, std::vector<::arisa::composition::Element> rows) const
        -> ::arisa::composition::Element;

    template <class... Rows>
        requires(sizeof...(Rows) > 0 && (std::convertible_to<Rows, ::arisa::composition::Element> && ...))
    auto section(std::string header, Rows&&... rows) const -> ::arisa::composition::Element
    {
        std::vector<::arisa::composition::Element> listed;
        (listed.emplace_back(std::forward<Rows>(rows)), ...);
        return section(std::move(header), std::move(listed));
    }

    template <class T>
    auto picker(std::string label, std::string_view help, std::vector<std::pair<T, std::string>> options,
        std::function<T()> read, std::function<void(T)> write) const -> ::arisa::composition::Element
    {
        SettingsModel* edit = model;
        const std::string key = rowKey(label, help);
        return described(::arisa::controls::Picker(std::move(label),
                             ::arisa::composition::Binding<T>(std::move(read),
                                 [edit, write = std::move(write)](T chosen) {
                                     write(chosen);
                                     edit->commit();
                                 }),
                             [options = std::move(options)] {
                                 std::vector<::arisa::composition::Element> items;
                                 for (const auto& [tag, shown] : options)
                                     items.push_back(::arisa::components::Text(shown).tag(tag));
                                 return ::arisa::composition::Element(::arisa::components::Group(std::move(items)));
                             }),
            help, key);
    }

private:
    static auto rowKey(std::string_view label, std::string_view help) -> std::string;
    auto described(::arisa::composition::Element row, std::string_view help, const std::string& key) const
        -> ::arisa::composition::Element;

    SettingsModel* model;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
