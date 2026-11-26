#include <switch.h>

#include <unistd.h>

namespace {
int nxlinkSocket = -1;
}

extern "C" void userAppInit()
{
    appletLockExit();

    SocketInitConfig config = *socketGetDefaultInitConfig();
    const AppletType type = appletGetAppletType();
    const bool application = type == AppletType_Application || type == AppletType_SystemApplication;
    config.num_bsd_sessions = application ? 12 : 2;
    config.sb_efficiency = application ? 8 : 1;
    if (application)
        config.udp_rx_buf_size = 0x40000;
    socketInitialize(&config);

    nxlinkSocket = nxlinkStdio();

    romfsInit();
    plInitialize(PlServiceType_User);
    setsysInitialize();
    setInitialize();
    psmInitialize();
    nifmInitialize(NifmServiceType_User);
}

extern "C" void userAppExit()
{
    nifmExit();
    psmExit();
    setExit();
    setsysExit();
    plExit();
    romfsExit();
    if (nxlinkSocket != -1)
        close(nxlinkSocket);
    socketExit();
    appletUnlockExit();
}
