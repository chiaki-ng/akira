#ifndef AKIRA_UPDATE_FLOW_HPP
#define AKIRA_UPDATE_FLOW_HPP

#include "platform/nx/update_manager.hpp"

namespace akira {

class UpdateFlow {
public:
    static void checkOnLaunch();
    static void promptUpdate(const UpdateInfo& info);
    static void simulate();

private:
    static void startDownload(const UpdateInfo& info);
};

}

#endif // AKIRA_UPDATE_FLOW_HPP
