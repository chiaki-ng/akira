#include "app/views/borealis_rumble.hpp"

#include <borealis.hpp>

namespace akira::app::views {

void BorealisRumble::playerOne(Wave wave)
{
    if (auto* input = brls::Application::getPlatform()->getInputManager())
        input->sendRumbleRaw(0, wave.freqLow, wave.freqHigh, wave.ampLow, wave.ampHigh);
}

void BorealisRumble::npad(unsigned npad, Wave wave)
{
    if (auto* input = brls::Application::getPlatform()->getInputManager())
        input->sendRumbleToNpad(npad, wave.freqLow, wave.freqHigh, wave.ampLow, wave.ampHigh);
}

void BorealisRumble::refresh(unsigned npad)
{
    if (auto* input = brls::Application::getPlatform()->getInputManager())
        input->refreshRumbleHandles(npad);
}

}
