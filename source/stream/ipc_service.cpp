#include "stream/ipc_service.hpp"
#include "stream/session.hpp"
#include "core/wireguard_manager.hpp"
#include <akira/ipc.h>
#include <cstring>
#include <borealis.hpp>

IpcStatsService::IpcStatsService(Session* session)
    : m_session(session)
{
    memset(&m_server, 0, sizeof(m_server));
    memset(&m_thread, 0, sizeof(m_thread));
}

IpcStatsService::~IpcStatsService()
{
    Stop();
}

void IpcStatsService::Start()
{
    if (m_running)
        return;

    Result rc = ipcServerInit(&m_server, AKIRA_IPC_SERVICE_NAME, 4);
    if (R_FAILED(rc))
    {
        brls::Logger::error("IpcStatsService: failed to init server: 0x{:X}", rc);
        return;
    }

    s32 priority;
    rc = svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    if (R_FAILED(rc))
    {
        brls::Logger::error("IpcStatsService: failed to get thread priority: 0x{:X}", rc);
        ipcServerExit(&m_server);
        return;
    }

    rc = threadCreate(&m_thread, &ProcessThreadFunc, this, NULL, 0x2000, priority, -2);
    if (R_FAILED(rc))
    {
        brls::Logger::error("IpcStatsService: failed to create thread: 0x{:X}", rc);
        ipcServerExit(&m_server);
        return;
    }

    rc = threadStart(&m_thread);
    if (R_FAILED(rc))
    {
        brls::Logger::error("IpcStatsService: failed to start thread: 0x{:X}", rc);
        threadClose(&m_thread);
        ipcServerExit(&m_server);
        return;
    }

    m_running = true;
    brls::Logger::info("IpcStatsService: started on service '{}'", AKIRA_IPC_SERVICE_NAME);
}

void IpcStatsService::Stop()
{
    if (!m_running)
        return;

    m_running = false;
    svcCancelSynchronization(m_thread.handle);
    threadWaitForExit(&m_thread);
    threadClose(&m_thread);
    ipcServerExit(&m_server);
    brls::Logger::info("IpcStatsService: stopped");
}

void IpcStatsService::ProcessThreadFunc(void* arg)
{
    auto* self = static_cast<IpcStatsService*>(arg);

    while (self->m_running)
    {
        Result rc = ipcServerProcess(&self->m_server, &ServiceHandler, self);
        if (R_FAILED(rc))
        {
            if (rc == KERNELRESULT(Cancelled))
                return;
        }
    }
}

Result IpcStatsService::ServiceHandler(void* userdata, const IpcServerRequest* r, u8* out_data, size_t* out_dataSize)
{
    auto* self = static_cast<IpcStatsService*>(userdata);

    switch (r->data.cmdId)
    {
        case AkiraIpcCmd_GetApiVersion:
        {
            *out_dataSize = sizeof(u32);
            *(u32*)out_data = AKIRA_IPC_API_VERSION;
            return 0;
        }

        case AkiraIpcCmd_IsStreamActive:
        {
            *out_dataSize = sizeof(u8);
            *(u8*)out_data = self->m_stream_active ? 1 : 0;
            return 0;
        }

        case AkiraIpcCmd_GetStreamStats:
        {
            if (!self->m_stream_active)
                return AKIRA_ERROR_NO_STREAM;

            if (r->hipc.meta.num_recv_buffers < 1)
                break;

            size_t bufSize = hipcGetBufferSize(r->hipc.data.recv_buffers);
            if (bufSize < sizeof(AkiraStreamStats))
                break;

            AkiraStreamStats* out = (AkiraStreamStats*)hipcGetBufferAddress(r->hipc.data.recv_buffers);
            StreamStats stats = self->m_session->getStreamStats();

            out->requested_width = stats.requested_width;
            out->requested_height = stats.requested_height;
            out->requested_fps = stats.requested_fps;
            out->requested_bitrate = stats.requested_bitrate;
            out->requested_hevc = stats.requested_hevc ? 1 : 0;

            out->video_width = stats.video_width;
            out->video_height = stats.video_height;
            out->is_hardware_decoder = stats.is_hardware_decoder ? 1 : 0;
            out->is_hevc = stats.is_hevc ? 1 : 0;

            out->fps = stats.fps;

            out->packet_loss_percent = stats.packet_loss_percent;
            out->packets_received = stats.packets_received;
            out->packets_lost = stats.packets_lost;
            out->measured_bitrate_mbps = stats.measured_bitrate_mbps;

            out->network_frames_lost = static_cast<uint32_t>(stats.network_frames_lost);
            out->frames_recovered = static_cast<uint32_t>(stats.frames_recovered);

            out->stream_duration_seconds = stats.stream_duration_seconds;

            out->ghash_mode = 1;

            auto& wg = WireGuardManager::instance();
            out->vpn_connected = wg.isConnected() ? 1 : 0;
            memset(out->vpn_ip, 0, sizeof(out->vpn_ip));
            if (wg.isConnected())
            {
                std::string ip = wg.getTunnelIP();
                strncpy(out->vpn_ip, ip.c_str(), sizeof(out->vpn_ip) - 1);
            }

            return 0;
        }
    }

    return MAKERESULT(11, 403);
}
