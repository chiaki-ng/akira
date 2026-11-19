#include "platform/stream/couch/provider.hpp"
#include "log/log.hpp"

#include <borealis.hpp>

#include "consoles/host.hpp"
#include "platform/pads/extended/provider.hpp"
#include "play/connection.hpp"
#include "platform/stream/couch/controller_picker_view.hpp"
#include "platform/stream/couch/players_panel_view.hpp"
#include "play/views/stream_menu.hpp"

using namespace brls::literals;

namespace akira::platform::couch
{

CouchStreamControlsProvider::CouchStreamControlsProvider(extended::ExtendedPadProvider& pads, std::weak_ptr<StreamControlsHost> host)
    : m_pads(pads)
    , m_host(std::move(host))
{
}

void CouchStreamControlsProvider::attach()
{
    auto weak = m_host;
    m_pads.setOnPadArrived([this, weak](HidNpadIdType npad) {
        brls::sync([this, weak, npad]() {
            if (weak.lock())
                onPadArrived(npad);
        });
    });

    m_pads.setOnCouchDisconnected([weak](HidNpadIdType, uint8_t slot) {
        brls::sync([weak, slot]() {
            if (auto view = weak.lock()) {
                if (auto connection = view->controlsConnection())
                    connection->removePlayer(slot);
            }
        });
    });
}

bool CouchStreamControlsProvider::beforeStart()
{
    if (m_pad_choice_done)
        return false;

    auto view = m_host.lock();
    if (!view || view->controlsSession() == nullptr) {
        m_pad_choice_done = true;
        return false;
    }

    if (!view->prepareController()) {
        m_pad_choice_done = true;
        return false;
    }

    waitForPads(0);
    return true;
}

void CouchStreamControlsProvider::waitForPads(int attempt)
{
    constexpr int kMaxAttempts = 20;
    constexpr int kIntervalMs  = 100;

    auto view = m_host.lock();
    if (!view || view->controlsSession() == nullptr) {
        finishPadChoice();
        return;
    }

    const auto availability = m_pads.extendedInput().availability();
    const bool backendUsable =
        availability == extended::ExtendedInputManager::Availability::Available;

    if (!backendUsable) {
        akira::log::info("PadChoice: no backend to wait for, deciding now");
        finishPadChoice();
        return;
    }

    for (const auto& pad : m_pads.describePads()) {
        if (pad.kind == input::PadPathKind::McPsNative ||
            pad.kind == input::PadPathKind::McGeneric) {
            akira::log::info("PadChoice: backend named a pad after {}ms",
                               attempt * kIntervalMs);
            finishPadChoice();
            return;
        }
    }

    if (attempt >= kMaxAttempts) {
        akira::log::info("PadChoice: no backend pad after {}ms, deciding anyway",
                           attempt * kIntervalMs);
        finishPadChoice();
        return;
    }

    auto weak = m_host;
    brls::delay(kIntervalMs, [this, weak, attempt]() {
        if (weak.lock())
            waitForPads(attempt + 1);
    });
}

void CouchStreamControlsProvider::finishPadChoice()
{
    m_pad_choice_done = true;

    auto view = m_host.lock();
    if (!view)
        return;
    if (view->controlsSession() == nullptr) {
        view->continueStart();
        return;
    }

    auto pads = m_pads.describePads();

    {
        AkiraInputDeviceList devices{};
        if (m_pads.extendedInput().listDevices(&devices)) {
            akira::log::info("PadChoice: backend lists {} device(s)", devices.count);
            for (uint8_t i = 0; i < devices.count && i < AKIRA_INPUT_MAX_LISTED_DEVICES; i++) {
                const AkiraInputDeviceInfo& d = devices.devices[i];
                akira::log::info(
                    "PadChoice:   {:04x}:{:04x} report=0x{:02x} flags=0x{:02x}{}{}{}",
                    d.vendor_id, d.product_id, d.report_id, d.flags,
                    (d.flags & AkiraInputDevice_Identified) ? " identified" : "",
                    (d.flags & AkiraInputDevice_Reporting)  ? " reporting"  : "",
                    (d.flags & AkiraInputDevice_Claimed)    ? " claimed"    : "");
            }
        } else {
            akira::log::warning("PadChoice: listDevices failed");
        }
    }

    for (const auto& pad : pads) {
        akira::log::info("PadChoice: npad {} {} [{}{}]",
                           (int)pad.npad, pad.label,
                           pad.caps.analog_triggers ? "analog " : "",
                           pad.caps.touchpad ? "touch " : "");
    }

    if (!ControllerPickerView::worthAsking(pads)) {
        akira::log::info("PadChoice: {} pad(s), nothing worth asking", pads.size());
        for (const auto& pad : pads) {
            if (pad.caps.analog_triggers || pad.caps.touchpad) {
                m_pads.selectNpad(pad.npad);
                break;
            }
        }
        view->continueStart();
        return;
    }

    akira::log::info("PadChoice: asking between {} pads", pads.size());

    auto weak = m_host;
    auto describe = [this, weak]() -> std::vector<input::PadDescription> {
        if (weak.lock())
            return m_pads.describePads();
        return {};
    };

    auto* picker = new ControllerPickerView(pads, [this, weak](HidNpadIdType npad) {
        auto self = weak.lock();

        if (npad == ControllerPickerView::kCancelled) {
            if (self)
                abandonStart();
            else
                brls::Application::popActivity();
            return;
        }

        brls::Application::popActivity();

        if (!self)
            return;

        if (npad != ControllerPickerView::kNoChoice)
            m_pads.selectNpad(npad);
        self->continueStart();
    }, describe);

    brls::Application::pushActivity(new brls::Activity(picker),
                                    brls::TransitionAnimation::NONE);
}

void CouchStreamControlsProvider::abandonStart()
{
    m_pad_choice_done = false;
    if (auto view = m_host.lock())
        view->abandonStart();
}

void CouchStreamControlsProvider::streaming()
{
    if (m_pads.extendedInput().consumeDegradedNotice())
    {
        brls::sync([]() {
            brls::Application::notify("akira/settings/analog_triggers_degraded"_i18n);
        });
    }
}

void CouchStreamControlsProvider::decorateMenu(play::views::StreamMenu& menu)
{
    auto view = m_host.lock();
    if (!view)
        return;

    menu.setPlayersAvailable(!view->controlsHost()->isCloud());

    auto weak = m_host;
    menu.setOnPlayers([this, weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        if (self->controlsSession() == nullptr) {
            akira::log::warning("StreamMenu: no input manager for players panel");
            return;
        }
        auto* panel = new PlayersPanelView(self->controlsHost(), self->controlsConnection(), &m_pads);
        brls::Application::pushActivity(new brls::Activity(panel), brls::TransitionAnimation::NONE);
    });
}

void CouchStreamControlsProvider::onPadArrived(HidNpadIdType npad)
{
    auto view = m_host.lock();
    if (!view)
        return;

    auto connection = view->controlsConnection();
    if (!connection || !connection->couchSlotAvailable()) {
        akira::log::info("Couch: pad arrived on npad {} but no slot is free", (int)npad);
        return;
    }

    if (m_arrival_prompt_open || view->controlsMenuOpen()) {
        akira::log::info("Couch: pad arrived on npad {} while UI is busy, not prompting", (int)npad);
        return;
    }
    m_arrival_prompt_open = true;

    view->pauseForUi();

    auto weak = m_host;
    auto* dialog = new brls::Dialog("akira/couch/controller_arrived"_i18n);

    dialog->addButton("akira/couch/add_player"_i18n, [this, weak]() {
        if (!weak.lock())
            return;
        m_arrival_prompt_open = false;
        openCouchClaim();
    });

    dialog->addButton("akira/couch/not_now"_i18n, [this, weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        m_arrival_prompt_open = false;
        if (self->controlsSession()) {
            m_pads.resolvePadArrival();
            if (m_pads.couchRoster().empty()) {
                openPrimaryControllerPicker();
                return;
            }
        }
        self->resumeAfterUi();
    });

    dialog->setCloseCallback([this, weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        m_arrival_prompt_open = false;
        if (self->controlsSession())
            m_pads.resolvePadArrival();
        self->resumeAfterUi();
    });

    akira::log::info("Couch: showing controller arrival prompt");
    dialog->open();
}

void CouchStreamControlsProvider::openCouchClaim()
{
    auto view = m_host.lock();
    if (!view)
        return;
    if (view->controlsSession() == nullptr) {
        akira::log::warning("StreamView: no input manager for couch claim");
        view->resumeAfterUi();
        return;
    }

    auto weak = m_host;
    auto* panel = new PlayersPanelView(view->controlsHost(), view->controlsConnection(), &m_pads, true);
    panel->setOnClosed([this, weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        if (self->controlsSession())
            m_pads.resolvePadArrival();
        self->resumeAfterUi();
    });
    brls::Application::pushActivity(new brls::Activity(panel), brls::TransitionAnimation::NONE);
}

void CouchStreamControlsProvider::openPrimaryControllerPicker()
{
    auto view = m_host.lock();
    if (!view)
        return;
    if (view->controlsSession() == nullptr) {
        view->resumeAfterUi();
        return;
    }

    auto pads = m_pads.describePads();
    if (!ControllerPickerView::worthAsking(pads)) {
        akira::log::info("Couch: no alternate primary controller worth asking about");
        view->resumeAfterUi();
        return;
    }

    akira::log::info("Couch: opening primary controller picker after Not now");
    auto weak = m_host;
    auto describe = [this, weak]() -> std::vector<input::PadDescription> {
        if (weak.lock())
            return m_pads.describePads();
        return {};
    };

    auto* picker = new ControllerPickerView(pads, [this, weak](HidNpadIdType npad) {
        brls::Application::popActivity(brls::TransitionAnimation::NONE);

        auto self = weak.lock();
        if (!self)
            return;
        if (npad != ControllerPickerView::kCancelled &&
            npad != ControllerPickerView::kNoChoice)
            m_pads.selectNpad(npad);
        self->resumeAfterUi();
    }, describe);

    brls::Application::pushActivity(new brls::Activity(picker),
                                    brls::TransitionAnimation::NONE);
}

}
