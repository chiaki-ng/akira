#pragma once

#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "consoles/console_facts.hpp"

namespace akira::app::arisa {

class ShellModel {
public:
    enum class Phase { Consoles, Connecting, Streaming };

    struct Console {
        std::string catalogName;
        std::string name;
        bool ready = false;
        bool paired = false;

        friend bool operator==(const Console&, const Console&) = default;
    };

    struct View {
        Phase phase = Phase::Consoles;
        std::vector<Console> consoles;
        std::string target;
        std::string status;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void list(const std::vector<consoles::ConsoleFacts>& facts);
    void connecting(std::string target);
    void stage(std::string label);
    void streaming();
    void stopped(std::string status);

private:
    ::arisa::composition::Observable<View> state;
};

}
