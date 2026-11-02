#ifndef AKIRA_PAIR_ADVERTISER_HPP
#define AKIRA_PAIR_ADVERTISER_HPP

#include <libchiaking/platform/platform.hpp>

namespace akira::pair {

class PairAdvertiser {
public:
    PairAdvertiser() = default;
    ~PairAdvertiser();

    void start(int port);
    void stop();

private:
    void run(int port, const libchiaking::platform::StopToken& stop);

    libchiaking::platform::Thread thread_;
};

}

#endif
