/*	DRVTIF.IRX
    Deci2_TIF_interface_driver v1.01 */

#include <deci2.h>
#include <intrman.h>
#include <loadcore.h>
#include <sifman.h>
#include <sysmem.h>
#include <sysclib.h>
#include <thevent.h>

#define MODNAME "Deci2_TIF_interface_driver"
IRX_ID(MODNAME, 1, 1);

struct Deci2_TIF_TR_data
{
    int EventFlag;
    unsigned short int activity;
    unsigned short int WrPtr;
    unsigned short int RdPtr;
    unsigned short int BufferLevel;
    unsigned char buffer[0x4000]; // A ring buffer.
};

struct Deci2_TIF_data
{
    sceDeci2Interface *interface;
    int IKTTYPSocket;
    unsigned int DebugFlags;
    unsigned short int flags;
    unsigned short int activity;
    struct Deci2_TIF_TR_data TxData;
    struct Deci2_TIF_TR_data RxData;
    DECI2_HEADER IncomingDECI2Header;
    unsigned short int BytesCopied;
    unsigned short int RecvLength;
    unsigned short int SendLength;
};

// Interface flags.
#define IF_SEND_PENDING  0x0001
#define IF_SEND_DISABLED 0x0002
#define IF_SEND_ENABLED  0x0004
#define IF_CAN_SEND      0x0010
#define IF_SEND_COMPLETE 0x0020

#define IF_RECV_PENDING  0x0100
#define IF_RECV_DISABLED 0x0200
#define IF_RECV_ENABLED  0x0400
#define IF_CAN_RECV      0x1000
#define IF_RECV_COMPLETE 0x2000

#define IF_MAIN_RESET 0x4000

static struct Deci2_TIF_data TIFData;

int HostIFActivityPollCallback(void);

// Internal functions.
static int tif_open(void *private);
static int tif_close(void *private);
static int tif_send(void *private, const void *buffer, unsigned int length);
static int tif_recv(void *private, void *buffer, unsigned int length);
static int tif_callback(void *private);
static int tif_IsReset(void *private);
static int ReadData(struct Deci2_TIF_TR_data *pData, void *buffer, int length, int mode);

static struct Deci2_TIF_handlers TIFHandlers = {
    &TIFData,
    &tif_open,
    &tif_close,
    &tif_send,
    &tif_recv,
    &tif_callback,
    &tif_IsReset};

static int tif_callback(void *private)
{
    if (((struct Deci2_TIF_data *)private)->RxData.activity) {
        iSetEventFlag(((struct Deci2_TIF_data *)private)->RxData.EventFlag, 1);
        ((struct Deci2_TIF_data *)private)->RxData.activity = 0;
    }

    if (((struct Deci2_TIF_data *)private)->TxData.activity) {
        iSetEventFlag(((struct Deci2_TIF_data *)private)->TxData.EventFlag, 1);
        ((struct Deci2_TIF_data *)private)->TxData.activity = 0;
    }

    return 0;
}

static int SignalReadDone(struct Deci2_TIF_data *pTIFData)
{
    pTIFData->TxData.activity = 1;
    sceDeci2_27();

    return 0;
}

static int SignalWriteDone(struct Deci2_TIF_data *pTIFData)
{
    pTIFData->RxData.activity = 1;
    sceDeci2_27();

    return 0;
}

static int SignalTIFActivity(struct Deci2_TIF_data *pTIFData, unsigned int bits)
{
    pTIFData->activity = 1;
    if (pTIFData->flags & bits)
        sceDeci2_28();

    return 0;
}

static int ReadData(struct Deci2_TIF_TR_data *pData, void *buffer, int length, int mode)
{
    int CopyLength, CopyLength2, OldState;

    CopyLength = (length < pData->BufferLevel) ? length : pData->BufferLevel;
    if (0x4000 - (pData->RdPtr & 0x3FFF) < CopyLength) {
        CopyLength = 0x4000 - (pData->RdPtr & 0x3FFF);
    }

    bcopy(pData->buffer + (pData->RdPtr & 0x3FFF), buffer, CopyLength);

    if ((CopyLength2 = length - CopyLength) > 0) {
        // If the ring buffer wrapped around, copy data from the other end if there is data there.
        if (pData->BufferLevel - CopyLength > 0) {
            if (pData->BufferLevel - CopyLength < CopyLength2)
                CopyLength2 = pData->BufferLevel - CopyLength;

            bcopy((const void *)((unsigned char *)pData->buffer + ((pData->RdPtr + CopyLength) & 0x3FFF)), (void *)((unsigned char *)buffer + CopyLength), CopyLength2);
        } else
            CopyLength2 = 0;
    } else
        CopyLength2 = 0;

    if (mode >= 0) {
        if (mode != 0)
            CpuSuspendIntr(&OldState);

        pData->RdPtr += (CopyLength + CopyLength2);
        pData->BufferLevel -= (CopyLength + CopyLength2);

        if (mode != 0)
            CpuResumeIntr(OldState);
    }

    return (CopyLength + CopyLength2);
}

static int WriteData(struct Deci2_TIF_TR_data *pData, const void *buffer, int length, int mode)
{
    int CopyLength, SpaceAvailable, OldState;

    SpaceAvailable = 0x4000 - pData->BufferLevel;
    CopyLength = (length < SpaceAvailable) ? length : SpaceAvailable;
    if (0x4000 - (pData->WrPtr & 0x3FFF) < CopyLength) { // Amount of space available before the end of the buffer.
        CopyLength = 0x4000 - (pData->WrPtr & 0x3FFF);
    }

    bcopy(buffer, pData->buffer + (pData->WrPtr & 0x3FFF), CopyLength);

    if ((length = length - CopyLength) > 0) {
        // If the ring buffer wrapped around, copy data to the other end if there is space.
        if ((SpaceAvailable = 0x4000 - CopyLength - pData->BufferLevel) > 0) {
            if (SpaceAvailable < length)
                length = SpaceAvailable;

            bcopy((const void *)((unsigned char *)buffer + CopyLength), (void *)((unsigned char *)pData->buffer + ((pData->WrPtr + CopyLength) & 0x3FFF)), length);
        } else
            length = 0;
    } else
        length = 0;

    if (mode >= 0) {
        if (mode != 0)
            CpuSuspendIntr(&OldState);

        pData->WrPtr += (CopyLength + length);
        pData->BufferLevel += (CopyLength + length);

        if (mode != 0)
            CpuResumeIntr(OldState);
    }

    return (CopyLength + length);
}

static void HandleSendEvent(struct Deci2_TIF_data *pTIFData)
{
    if (pTIFData->flags & IF_SEND_PENDING) {
        if (pTIFData->RxData.BufferLevel < 0x4000) {
            pTIFData->flags = (pTIFData->flags | IF_CAN_SEND) & ~IF_SEND_ENABLED;
        } else {
            pTIFData->flags |= IF_SEND_ENABLED;
        }
    }
}

static void HandleRecvEvent(struct Deci2_TIF_data *pTIFData)
{
    struct Deci2_TIF_TR_data *pTRData;

    if (pTIFData->TxData.BufferLevel > 0) {
        pTRData = &pTIFData->TxData;
        pTIFData->flags = (pTIFData->flags & ~IF_RECV_ENABLED) | IF_CAN_RECV;
        if (pTIFData->IncomingDECI2Header.len == 0) {
            if (pTRData->BufferLevel < 8) {
                pTIFData->flags &= ~IF_CAN_RECV;
            } else {
                ReadData(&pTIFData->TxData, &pTIFData->IncomingDECI2Header, sizeof(pTIFData->IncomingDECI2Header), -1);
            }
        }
    }
}

static int HostIFHandler(int event, void *opt, int param1, int param2)
{
    int result, ContinueFlag, CopyLen;
    static const char *FunctionNames[] = {
        "RCV_START",
        "RCV_READ",
        "RCV_END",
        "SEND_START",
        "SEND_WRITE",
        "SEND_END",
        "POLL",
        "RCV_OFF",
        "RCV_ON",
        "SEND_OFF",
        "SEND_ON",
        "DEBUG"};

    result = 0; // The original does not seem to have this variable initialized.
    if ((((struct Deci2_TIF_data *)opt)->DebugFlags & 0x200) && event != IFD_POLL) {
        sceDeci2ExPanic("\ttif ifc func %s flag=%x p1=%x p2=%x\n", FunctionNames[event], ((struct Deci2_TIF_data *)opt)->flags, param1, param2);
    }

    switch (event) {
        case IFD_RCV_START:
            ((struct Deci2_TIF_data *)opt)->flags |= IF_RECV_PENDING;
            ((struct Deci2_TIF_data *)opt)->BytesCopied = 0;
            result = 0;
            break;
        case IFD_RCV_READ:
            CopyLen = ((struct Deci2_TIF_data *)opt)->IncomingDECI2Header.len - ((struct Deci2_TIF_data *)opt)->BytesCopied;
            if (param2 < CopyLen)
                CopyLen = param2;
            if ((result = ReadData(&((struct Deci2_TIF_data *)opt)->TxData, (void *)param1, CopyLen, 0)) > 0) {
                ((struct Deci2_TIF_data *)opt)->RecvLength = result;
                ((struct Deci2_TIF_data *)opt)->BytesCopied += result;
                ((struct Deci2_TIF_data *)opt)->flags |= IF_RECV_COMPLETE;
                SignalReadDone(opt);
                HandleRecvEvent(opt);
            }
            break;
        case IFD_RCV_END:
            ((struct Deci2_TIF_data *)opt)->IncomingDECI2Header.len = 0;
            ((struct Deci2_TIF_data *)opt)->flags &= ~(IF_CAN_RECV | IF_RECV_PENDING);
            if (!(((struct Deci2_TIF_data *)opt)->flags & IF_RECV_DISABLED)) {
                ((struct Deci2_TIF_data *)opt)->flags |= IF_RECV_ENABLED;
                HandleRecvEvent(opt);
            }
            result = 0;
            break;
        case IFD_SEND_START:
            ((struct Deci2_TIF_data *)opt)->flags |= IF_SEND_PENDING;
            HandleSendEvent(opt);
            result = 0;
            break;
        case IFD_SEND_WRITE:
            if ((result = WriteData(&((struct Deci2_TIF_data *)opt)->RxData, (const void *)param1, param2, 0)) > 0) {
                ((struct Deci2_TIF_data *)opt)->SendLength = result;
                ((struct Deci2_TIF_data *)opt)->flags |= IF_SEND_COMPLETE;
                SignalWriteDone(opt);
            }
            break;
        case IFD_SEND_END:
            ((struct Deci2_TIF_data *)opt)->flags &= ~(IF_CAN_SEND | IF_SEND_ENABLED | IF_SEND_PENDING);
            result = 0;
            break;
        case IFD_POLL:
            if (((struct Deci2_TIF_data *)opt)->activity) {
                ((struct Deci2_TIF_data *)opt)->activity = 0;
                HandleSendEvent(opt);
                HandleRecvEvent(opt);
            }

            do {
                ContinueFlag = 0;
                if (((struct Deci2_TIF_data *)opt)->flags & IF_SEND_COMPLETE) {
                    ((struct Deci2_TIF_data *)opt)->flags &= ~IF_SEND_COMPLETE;
                    sceDeci2IfEventHandler(IFM_OUTDONE, ((struct Deci2_TIF_data *)opt)->interface, ((struct Deci2_TIF_data *)opt)->SendLength, 0, 0);
                    ContinueFlag = 1;

                    if (((struct Deci2_TIF_data *)opt)->flags & IF_SEND_PENDING) {
                        HandleSendEvent(opt);
                    }
                }

                if ((((struct Deci2_TIF_data *)opt)->flags & (IF_CAN_SEND | IF_SEND_DISABLED | IF_SEND_PENDING)) == (IF_CAN_SEND | IF_SEND_PENDING)) {
                    ((struct Deci2_TIF_data *)opt)->flags &= ~IF_CAN_SEND;
                    ContinueFlag = 1;
                    sceDeci2IfEventHandler(IFM_OUT, ((struct Deci2_TIF_data *)opt)->interface, 0, 0, 0);
                }

                if (((struct Deci2_TIF_data *)opt)->flags & IF_RECV_COMPLETE) {
                    ((struct Deci2_TIF_data *)opt)->flags &= ~IF_RECV_COMPLETE;
                    sceDeci2IfEventHandler(IFM_INDONE, ((struct Deci2_TIF_data *)opt)->interface, ((struct Deci2_TIF_data *)opt)->RecvLength, 0, 0);
                    ContinueFlag = 1;
                    ((struct Deci2_TIF_data *)opt)->flags |= IF_RECV_ENABLED;
                }

                if ((((struct Deci2_TIF_data *)opt)->flags & (IF_CAN_RECV | IF_RECV_DISABLED)) == IF_CAN_RECV) {
                    ((struct Deci2_TIF_data *)opt)->flags &= ~IF_CAN_RECV;
                    ContinueFlag = 1;
                    sceDeci2IfEventHandler(IFM_IN, ((struct Deci2_TIF_data *)opt)->interface, ((struct Deci2_TIF_data *)opt)->TxData.BufferLevel < ((struct Deci2_TIF_data *)opt)->IncomingDECI2Header.len ? ((struct Deci2_TIF_data *)opt)->TxData.BufferLevel : ((struct Deci2_TIF_data *)opt)->IncomingDECI2Header.len, ((struct Deci2_TIF_data *)opt)->IncomingDECI2Header.proto, ((struct Deci2_TIF_data *)opt)->IncomingDECI2Header.dest);
                }
            } while (ContinueFlag);
            result = 0;
            break;
        case IFD_RCV_OFF:
            ((struct Deci2_TIF_data *)opt)->flags = (((struct Deci2_TIF_data *)opt)->flags | IF_RECV_DISABLED) & ~IF_RECV_ENABLED;
            result = 0;
            break;
        case IFD_RCV_ON:
            ((struct Deci2_TIF_data *)opt)->flags = (((struct Deci2_TIF_data *)opt)->flags & ~IF_RECV_DISABLED) | IF_RECV_ENABLED;
            result = 0;
            break;
        case IFD_SEND_OFF:
            ((struct Deci2_TIF_data *)opt)->flags = (((struct Deci2_TIF_data *)opt)->flags | IF_SEND_DISABLED) & ~IF_SEND_ENABLED;
            result = 0;
            break;
        case IFD_SEND_ON:
            ((struct Deci2_TIF_data *)opt)->flags = (((struct Deci2_TIF_data *)opt)->flags & ~IF_SEND_DISABLED) | IF_SEND_ENABLED;
            result = 0;
            break;
        case IFD_DEBUG:
            ((struct Deci2_TIF_data *)opt)->DebugFlags = param1;
            break;
        case IFD_SHUTDOWN:
            // Do nothing.
            break;
    }

    return result;
}

static int tif_open(void *private)
{
    return 0;
}

static int tif_close(void *private)
{
    sceDeci2IfEventHandler(IFM_DOWN, ((struct Deci2_TIF_data *)private)->interface, 0, 0, 0);
    return 0;
}

static int tif_IsReset(void *private)
{
    return (((struct Deci2_TIF_data *)private)->flags & IF_MAIN_RESET);
}

static int CheckForResetPacket(struct Deci2_TIF_data *pTIFData, const void *buffer, unsigned int length)
{
    int CopyLen, offset, DataProcessed;
    DECI2_HEADER Deci2Header;
    static const u32 EE_DCMP_Reset_packet[] = {
        0x0000000C,
        0x45450001,
        0x00000004};

    offset = 0;
    DataProcessed = 0;
    do {
        if ((CopyLen = 8 - DataProcessed) > 0) { // Copy only the first 8 bytes (DECI2 header) of the block.
            if (length < CopyLen)
                CopyLen = length;

            bcopy(buffer, &((unsigned char *)&Deci2Header)[offset], CopyLen);
            (unsigned char *)buffer += CopyLen;
            length -= CopyLen;
            offset += CopyLen;
            DataProcessed += CopyLen;
        }

        if (DataProcessed >= 8) { // Only if there's a complete DECI2 header read in. At this point, skipping this whole block of code won't cause an infinite loop because there won't be any data left (length==0).
            // Check for a magic packet.
            if (Deci2Header.proto == DECI2_PROTO_NETMP &&
                Deci2Header.src == 'H' &&
                Deci2Header.dest == 'H' &&
                ((NETMP_HEADER *)buffer)->code == NETMP_CODE_RESET) {
                sceSifSetMSFlag(0x00010000);
                sceSifSetMSFlag(0x00020000);
                sceSifSetMSFlag(0x00040000);
                tif_send(&TIFData, EE_DCMP_Reset_packet, sizeof(EE_DCMP_Reset_packet));
                pTIFData->flags |= IF_MAIN_RESET;
                return -1;
            } else {
                // Advance to the next packet, if any.
                if ((CopyLen = Deci2Header.len - DataProcessed) > 0) {
                    if (length < CopyLen)
                        CopyLen = length;

                    (unsigned char *)buffer += CopyLen;
                    length -= CopyLen;
                    offset += CopyLen;
                    DataProcessed += CopyLen;
                }

                // End of packet hit.
                if (DataProcessed >= Deci2Header.len) {
                    DataProcessed = 0;
                    offset = 0;
                }
            }
        }
    } while (length > 0);

    return 0;
}

static int tif_send(void *private, const void *buffer, unsigned int length)
{
    int result;
    struct Deci2_TIF_TR_data *TRData;

    if ((result = CheckForResetPacket(private, buffer, length)) >= 0) {
        TRData = &((struct Deci2_TIF_data *)private)->TxData;
        while (TRData->BufferLevel >= 0x4000) {
            WaitEventFlag(TRData->EventFlag, 1, WEF_CLEAR | WEF_OR, NULL);
        }

        if ((result = WriteData(TRData, buffer, length, 1)) > 0) {
            SignalTIFActivity(private, IF_RECV_ENABLED);
        }
    }

    return result;
}

static int tif_recv(void *private, void *buffer, unsigned int length)
{
    struct Deci2_TIF_TR_data *RxData;
    int result;

    RxData = &((struct Deci2_TIF_data *)private)->RxData;
    while (RxData->BufferLevel <= 0) {
        WaitEventFlag(RxData->EventFlag, 1, WEF_CLEAR | WEF_OR, NULL);
    }

    if ((result = ReadData(RxData, buffer, length, 1)) > 0) {
        SignalTIFActivity(private, IF_SEND_ENABLED);
    }

    return result;
}

static void DummyPrintRoutine(char **string, int c)
{
    // Does nothing.
}

static void IKTTYPSocketHandler(int event, int param, void *opt)
{
    unsigned char buffer[4];

    if (event == IFD_RCV_READ) {
        sceDeci2ExRecv(((struct Deci2_TIF_data *)opt)->IKTTYPSocket, buffer, sizeof(buffer));
    }
}

int _start(int argc, char *argv[])
{
    iop_event_t EventFlagData;
    static const unsigned int DCMP_H_I_HelloPacket[] = {
        0x00000020,
        0x49480001,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000};

    sceDeci2DbgPrintStatus(&DummyPrintRoutine, NULL);
    TIFData.IKTTYPSocket = sceDeci2Open(DECI2_PROTO_IKTTYP, &TIFData, &IKTTYPSocketHandler);

    EventFlagData.attr = 0;
    EventFlagData.bits = 0;
    EventFlagData.option = 0;
    TIFData.flags |= IF_RECV_ENABLED;
    if ((TIFData.TxData.EventFlag = CreateEventFlag(&EventFlagData)) < 0) {
        Kprintf("\tCreateEventFlag -> %d\n", TIFData.TxData.EventFlag);
        return MODULE_NO_RESIDENT_END;
    }

    if ((TIFData.RxData.EventFlag = CreateEventFlag(&EventFlagData)) < 0) {
        Kprintf("\tCreateEventFlag -> %d\n", TIFData.RxData.EventFlag);
        return MODULE_NO_RESIDENT_END;
    }

    sceDeci2SetTifHandlers(&TIFHandlers);

    TIFData.interface = sceDeci2IfCreate('H', &TIFData, &HostIFHandler, &HostIFActivityPollCallback);
    sceDeci2IfEventHandler(IFM_UP, TIFData.interface, 0, 0, 0);

    tif_send(&TIFData, DCMP_H_I_HelloPacket, sizeof(DCMP_H_I_HelloPacket));

    return MODULE_RESIDENT_END;
}
