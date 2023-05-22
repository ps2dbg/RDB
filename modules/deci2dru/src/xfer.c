#include <errno.h>
#include <stdio.h>
#include <dmacman.h>
#include <dev9.h>
#include <intrman.h>
#include <loadcore.h>
#include <modload.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <irx.h>

#include <smapregs.h>
#include <speedregs.h>

#include "lwip/pbuf.h"

#include "smap_internal.h"
#include "ps2ip_internal.h"

extern void *_gp;
extern struct SmapDriverData SmapDriverData;

static inline void CopyFromFIFO(volatile u8 *smap_regbase, void *buffer, unsigned int length, unsigned short int RxBdPtr)
{
    int i;

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = RxBdPtr;

    for (i = 0; i < length; i += 4)
        ((u32 *)buffer)[i / 4] = SMAP_REG32(SMAP_R_RXFIFO_DATA);
}

int SMAPHandleIncomingPackets(void)
{
    USE_SMAP_RX_BD;
    int NumPacketsReceived;
    volatile smap_bd_t *PktBdPtr;
    volatile u8 *smap_regbase;
    unsigned short int ctrl_stat, data_len;
    struct pbuf *pbuf;

    NumPacketsReceived = 0;
    if (SmapDriverData.SmapIsInitialized) {
        smap_regbase = SmapDriverData.smap_regbase;

        while (1) {
            PktBdPtr = &rx_bd[SmapDriverData.RxBDIndex & (SMAP_BD_MAX_ENTRY - 1)];
            if (!((ctrl_stat = PktBdPtr->ctrl_stat) & SMAP_BD_RX_EMPTY)) {
                data_len = PktBdPtr->length;

                if ((!(ctrl_stat & (SMAP_BD_RX_INRANGE | SMAP_BD_RX_OUTRANGE | SMAP_BD_RX_FRMTOOLONG | SMAP_BD_RX_BADFCS | SMAP_BD_RX_ALIGNERR | SMAP_BD_RX_SHORTEVNT | SMAP_BD_RX_RUNTFRM | SMAP_BD_RX_OVERRUN)) && data_len <= MAX_FRAME_SIZE)) {
                    if ((pbuf = pbuf_alloc(PBUF_RAW, data_len, PBUF_POOL)) != NULL) {
                        CopyFromFIFO(SmapDriverData.smap_regbase, pbuf->payload, pbuf->len, PktBdPtr->pointer);

                        // Inform ps2ip that we've received data.
                        SMapLowLevelInput(pbuf);

                        NumPacketsReceived++;
                    }
                }

                SMAP_REG8(SMAP_R_RXFIFO_FRAME_DEC) = 0;
                PktBdPtr->ctrl_stat = SMAP_BD_RX_EMPTY;
                SmapDriverData.RxBDIndex++;
            } else
                break;
        }
    }

    return NumPacketsReceived;
}

int SMAPSendPacket(const void *data, unsigned int length)
{
    int result, i;
    USE_SMAP_TX_BD;
    volatile u8 *smap_regbase;
    volatile u8 *emac3_regbase;
    volatile smap_bd_t *BD_ptr;
    u16 BD_data_ptr;
    unsigned int SizeRounded;

    if (SmapDriverData.SmapIsInitialized) {
        SizeRounded = (length + 3) & ~3;
        smap_regbase = SmapDriverData.smap_regbase;
        emac3_regbase = SmapDriverData.emac3_regbase;

        while (SMAP_EMAC3_GET(SMAP_R_EMAC3_TxMODE0) & SMAP_E3_TX_GNP_0) {};

        BD_data_ptr = SMAP_REG16(SMAP_R_TXFIFO_WR_PTR);
        BD_ptr = &tx_bd[SmapDriverData.TxBDIndex & (SMAP_BD_MAX_ENTRY - 1)];

        for (i = 0; i < length; i += 4)
            SMAP_REG32(SMAP_R_TXFIFO_DATA) = ((u32 *)data)[i / 4];

        BD_ptr->length = length;
        BD_ptr->pointer = BD_data_ptr;
        SMAP_REG8(SMAP_R_TXFIFO_FRAME_INC) = 0;
        BD_ptr->ctrl_stat = SMAP_BD_TX_READY | SMAP_BD_TX_GENFCS | SMAP_BD_TX_GENPAD;
        SmapDriverData.TxBDIndex++;

        SMAP_EMAC3_SET(SMAP_R_EMAC3_TxMODE0, SMAP_E3_TX_GNP_0);

        result = 1;
    } else
        result = -1;

    return result;
}
