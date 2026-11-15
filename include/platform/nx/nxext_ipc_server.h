#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <switch.h>

#define IPC_SERVER_EXT_RESPONSE_MAX_DATA_SIZE (0x100 - 0x10 - sizeof(IpcServerRawHeader))

typedef struct
{
    u64 magic;
    union
    {
        u64 cmdId;
        u64 result;
    };
} IpcServerRawHeader;

typedef struct
{
    SmServiceName srvName;
    Handle handles[MAX_WAIT_OBJECTS];
    u32 max;
    u32 count;
} IpcServer;

typedef struct
{
    u64 cmdId;
    void* ptr;
    size_t size;
} IpcServerRequestData;

typedef struct
{
    HipcParsedRequest hipc;
    IpcServerRequestData data;
} IpcServerRequest;

typedef Result (*IpcServerRequestHandler)(void* userdata, const IpcServerRequest* r, u8* out_data, size_t* out_dataSize);

Result ipcServerInit(IpcServer* server, const char* name, u32 max_sessions);
Result ipcServerExit(IpcServer* server);
Result ipcServerProcess(IpcServer* server, IpcServerRequestHandler handler, void* userdata);

#ifdef __cplusplus
}
#endif
