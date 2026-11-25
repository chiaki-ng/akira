#ifndef AKIRA_PSN_GATED_BOX_HPP
#define AKIRA_PSN_GATED_BOX_HPP

#include <borealis.hpp>

#include <unordered_set>

#include "accounts/accounts.hpp"

namespace akira::accounts::views {

class PsnGatedBox : public brls::Box {
public:
    PsnGatedBox();
    ~PsnGatedBox() override;

    static brls::View* create();

    bool evaluate();
    bool allowed() const { return gateOpen; }

    static void evaluateAll();

private:
    void applyState(SessionState state);

    brls::Label* placeholder = nullptr;
    bool gateOpen = true;
    bool applied = false;
    SessionState appliedState = SessionState::Valid;

    static std::unordered_set<PsnGatedBox*> liveBoxes;
    static bool observerInstalled;
};

}

#endif // AKIRA_PSN_GATED_BOX_HPP
