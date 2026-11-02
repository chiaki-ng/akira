#ifndef AKIRA_CLOUD_START_STEPS_HPP
#define AKIRA_CLOUD_START_STEPS_HPP

namespace cloud {

class StartSteps {
public:
    enum class Step {
        Checking,
        Preparing,
        FindingServer,
        Connecting
    };

    enum class Mark {
        Pending,
        Running,
        Done,
        Failed
    };

    static constexpr Step all[] = {Step::Checking, Step::Preparing, Step::FindingServer, Step::Connecting};

    bool advance(Step step);
    void playing();
    void fail();

    auto current() const -> Step { return reached; }
    auto mark(Step step) const -> Mark;
    bool failed() const { return phase == Phase::Failed; }

private:
    enum class Phase {
        Starting,
        Playing,
        Failed
    };

    Step reached = Step::Checking;
    Phase phase = Phase::Starting;
};

} // namespace cloud

#endif // AKIRA_CLOUD_START_STEPS_HPP
