#ifndef AKIRA_CUSTOM_DNS_PROMPT_HPP
#define AKIRA_CUSTOM_DNS_PROMPT_HPP

#include <functional>

namespace akira::network::views {

class CustomDnsPrompt {
public:
    static void open(std::function<void()> onSaved);

private:
    static void reject();
};

}

#endif
