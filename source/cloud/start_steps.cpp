#include "cloud/start_steps.hpp"

namespace cloud {

bool StartSteps::advance(Step step)
{
    if (phase != Phase::Starting || step <= reached)
        return false;
    reached = step;
    return true;
}

void StartSteps::playing()
{
    if (phase == Phase::Starting)
        phase = Phase::Playing;
}

void StartSteps::fail()
{
    if (phase == Phase::Starting)
        phase = Phase::Failed;
}

auto StartSteps::mark(Step step) const -> Mark
{
    if (phase == Phase::Playing || step < reached)
        return Mark::Done;
    if (step > reached)
        return Mark::Pending;
    return phase == Phase::Failed ? Mark::Failed : Mark::Running;
}

} // namespace cloud
