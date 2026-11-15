#pragma once

#include <switch.h>
#include <atomic>
#include "platform/stats/nxext/nxext_ipc_server.h"

class Session;

namespace akira::platform::nxext
{

class IpcStatsService
{
public:
    IpcStatsService(Session* session);
    ~IpcStatsService();

    void Start();
    void Stop();
    bool IsRunning() const { return m_running; }
    void SetStreamActive(bool active) { m_stream_active.store(active); }

private:
    static void ProcessThreadFunc(void* arg);
    static Result ServiceHandler(void* userdata, const IpcServerRequest* r, u8* out_data, size_t* out_dataSize);

    Session* m_session;
    IpcServer m_server;
    Thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stream_active{false};
};

}

