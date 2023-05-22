#include "common.h"

struct SmapDriverData{
	volatile u8 *smap_regbase;
	volatile u8 *emac3_regbase;
	u8 TxBDIndex;
	u8 RxBDIndex;
	u8 SmapIsInitialized;
	u8 LinkStatus;
	u8 LinkMode;
	u8 EnableLinkCheckTimer;
	iop_sys_clock_t LinkCheckTimer;
};

#define DEV9_SMAP_ALL_INTR_MASK	(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_TXEND|SMAP_INTR_RXDNV|SMAP_INTR_TXDNV)
#define DEV9_SMAP_INTR_MASK	(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV)//|SMAP_INTR_TXDNV)
//The Tx interrupt events are handled separately
#define DEV9_SMAP_INTR_MASK2	(SMAP_INTR_EMAC3|SMAP_INTR_RXEND|SMAP_INTR_RXDNV)

/* Function prototypes */
int smap_init(u16 noAuto, u16 smapConf);
int SMAPStart(void);
void SMAPStop(void);
int SMAPGetMACAddress(unsigned char *buffer);

#include "xfer.h"
