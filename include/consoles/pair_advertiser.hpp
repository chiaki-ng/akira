#ifndef AKIRA_PAIR_ADVERTISER_HPP
#define AKIRA_PAIR_ADVERTISER_HPP

#include <libchiaking/platform/platform.hpp>

namespace akira::consoles {

class PairAdvertiser {
public:
    explicit PairAdvertiser(libchiaking::platform::ThreadingProvider& threading);
    ~PairAdvertiser();

    void start(int port);
    void stop();

private:
    void run(int port, const libchiaking::platform::StopToken& stop);

    libchiaking::platform::ThreadingProvider& threading_;
    libchiaking::platform::Thread thread_;
};

}

#endif
