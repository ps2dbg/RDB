/*	TIFINET.IRX
    TIF_bridge_for_INET v1.01 */

#include <deci2.h>
#include <loadcore.h>
#include <modload.h>
#include <sifman.h>
#include <stdio.h>
#include <sysmem.h>
#include <thbase.h>
#include <ps2ip.h>

typedef int SOCKET;

#define MODNAME "TIF_bridge_for_INET"
IRX_ID(MODNAME, 1, 1);

struct PrivateData
{
    int MainThreadID;
    int ClientThreadID;
    SOCKET ServerSocket; // The server socket descriptor.
    struct Deci2_TIF_handlers *tif;
    unsigned char SendBuffer[1460];
    unsigned char RecvBuffer[1460];
    // Stuff added in, to support socket connections.
    SOCKET ClientSocket;
    int ServerPort;
};

static struct PrivateData PrivateData;

static int SysCreateThread(void *EntryPoint, void *parameter)
{
    iop_thread_t ThreadData;
    int ThreadID, result;

    ThreadData.thread = EntryPoint;
    ThreadData.attr = TH_C;
    ThreadData.priority = 9;
    ThreadData.stacksize = 0x1000;
    ThreadData.option = 0;

    if ((ThreadID = CreateThread(&ThreadData)) >= 0) {
        if ((result = StartThread(ThreadID, parameter)) != 0) {
            Kprintf("tifinet: StartThead -> %d\n", result);
            DeleteThread(ThreadID);
            result = -1;
        } else {
            result = 0;
        }
    } else {
        Kprintf("tifinet: CreateThead -> %d\n", ThreadID);
        result = -1;
    }

    return result;
}

static void SubServerThread(void *argument)
{
    struct Deci2_TIF_handlers *tif;
    int result, size, consumed;
    void *buffer;

    tif = ((struct PrivateData *)argument)->tif;

    while (1) {
        result = tif->tif_recv(tif->private, ((struct PrivateData *)argument)->RecvBuffer, sizeof(((struct PrivateData *)argument)->RecvBuffer));

        if (result > 0) {
            size = result;
            consumed = 0;

            while (size > 0) {
                buffer = (void *)((unsigned char *)((struct PrivateData *)argument)->RecvBuffer + consumed);

                if ((result = send(((struct PrivateData *)argument)->ClientSocket, buffer, size, 0)) <= 0) {
                    Kprintf("tifinet: send -> %d\n", result);
                    return;
                } else {
                    consumed += result;
                    size -= result;
                }
            }
        } else if (result < 0) {
            Kprintf("tifinet: tif-recv -> %d\n", result);
            break;
        }
    }
}

static char RebootParameters[12];

static void MainThread(void *argument)
{
    struct sockaddr_in addr;
    int result, size, consumed, isReset;
    void *buffer;
    struct Deci2_TIF_handlers *tif;

    ((struct PrivateData *)argument)->ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // Added to support socket connections.
    ((struct PrivateData *)argument)->ClientSocket = -1;
    ((struct PrivateData *)argument)->ServerPort = 8510;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(((struct PrivateData *)argument)->ServerPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if ((result = bind(((struct PrivateData *)argument)->ServerSocket, (struct sockaddr *)&addr, sizeof(addr))) == 0) {
        if ((result = listen(((struct PrivateData *)argument)->ServerSocket, 1)) == 0) {
            Kprintf("tifinet: listening port %d\n", ((struct PrivateData *)argument)->ServerPort);

            size = sizeof(addr);
            if ((result = ((struct PrivateData *)argument)->ClientSocket = accept(((struct PrivateData *)argument)->ServerSocket, (struct sockaddr *)&addr, &size)) >= 0) {
                Kprintf("tifinet: connected from %s.\n", inet_ntoa(addr.sin_addr));

                if ((((struct PrivateData *)argument)->tif = tif = sceDeci2GetTifHandlers()) != NULL) {

                    if ((result = tif->tif_open(tif->private)) >= 0) {
                        if ((((struct PrivateData *)argument)->ClientThreadID = SysCreateThread(&SubServerThread, argument)) >= 0) {

                            while (1) {
                                result = recv(((struct PrivateData *)argument)->ClientSocket, ((struct PrivateData *)argument)->SendBuffer, sizeof(((struct PrivateData *)argument)->SendBuffer), 0);

                                if (result == 0) {
                                    Kprintf("tifinet: Fin received\n");
                                    break;
                                } else if (result < 0) {
                                    Kprintf("tifinet: recv -> %d\n", result);
                                    break;
                                }

                                size = result;
                                consumed = 0;
                                while (size > 0) {
                                    buffer = (void *)((unsigned char *)((struct PrivateData *)argument)->SendBuffer + consumed);

                                    if ((result = tif->tif_send(tif->private, buffer, size)) < 0) {
                                        Kprintf("tifinet: tif-send -> %d\n", result);
                                        goto cleanup;
                                    } else {
                                        consumed += result;
                                        size -= result;
                                    }
                                }
                            }
                        } else {
                            goto cleanup;
                        }
                    } else {
                        Kprintf("tifinet: tif-open -> %d\n", result);
                    }
                } else {
                    Kprintf("tifinet: Deci2GetTifHandlers -> NULL\n");
                }

            cleanup:
                // Unlike the TDB startup card, give the TIF driver some time to send the reset packet to the EE.
                DelayThread(2000);

                isReset = 0;
                if ((tif = ((struct PrivateData *)argument)->tif) != NULL) {
                    // Determine whether the connection was lost, due to a reset request.
                    isReset = tif->tif_IsReset(tif->private);
                    tif->tif_close(tif->private);
                    ((struct PrivateData *)argument)->tif = NULL;
                }

                if (((struct PrivateData *)argument)->ServerSocket >= 0) {
                    disconnect(((struct PrivateData *)argument)->ServerSocket);
                    ((struct PrivateData *)argument)->ServerSocket = -1;
                }

                // Added to support socket connections
                if (((struct PrivateData *)argument)->ClientSocket >= 0) {
                    disconnect(((struct PrivateData *)argument)->ClientSocket);
                    ((struct PrivateData *)argument)->ClientSocket = -1;
                }

                if (((struct PrivateData *)argument)->ClientThreadID != 0) {
                    TerminateThread(((struct PrivateData *)argument)->ClientThreadID);
                    DeleteThread(((struct PrivateData *)argument)->ClientThreadID);
                    ((struct PrivateData *)argument)->ClientThreadID = 0;
                }

                if (isReset != 0) {
                    // Reboot the IOP, with the stock kernel. The DCMP command to reboot the EE has been sent by DRVTIF when the reset packet was received.
                    while (!(sceSifGetMSFlag() & 0x00010000)) {};

                    *(volatile u32 *)0xbf801538 = 0;
                    ReBootStart(RebootParameters, 0x80000000);
                }
            } else {
                Kprintf("tifinet: accept -> %d\n", result);
            }
        } else {
            Kprintf("tifinet: listen -> %d\n", result);
        }
    } else {
        Kprintf("tifinet: bind -> %d\n", result);
    }
}

int _start(int argc, char *argv[])
{
    PrivateData.MainThreadID = SysCreateThread(&MainThread, &PrivateData);
    return (PrivateData.MainThreadID < 0 ? MODULE_NO_RESIDENT_END : MODULE_RESIDENT_END);
}
