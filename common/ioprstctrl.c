#include <kernel.h>
#include <stdio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <libcdvd.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifdma.h>
#include <sifrpc.h>
#include <string.h>

#include "ioprstctrl.h"
#include "modutils.h"

extern unsigned char IMGDRV_irx_start[];
extern unsigned int IMGDRV_irx_size;

/*----------------------------------------------------------------------------------------*/
/* Copy the specified IOPRP image into IOP RAM and executes IMGDRV.			  */
/*----------------------------------------------------------------------------------------*/
static void LoadIMGDRV(const void *image1, int size1, const void *image2, int size2)
{
	SifDmaTransfer_t dmat[2];
	void *iop_buffer1, *iop_buffer2;
	int dmat_id, count;

	iop_buffer1=SifAllocIopHeap((size1+3)&~3);

	count = 1;
	dmat[0].src=(void*)image1;
	dmat[0].dest=iop_buffer1;
	dmat[0].size=size1;
	dmat[0].attr=0;

	if(image2 != NULL)
	{
		iop_buffer2=SifAllocIopHeap((size2+3)&~3);
		dmat[1].src=(void*)image2;
		dmat[1].dest=iop_buffer2;
		dmat[1].size=size2;
		dmat[1].attr=0;
		count++;
	}

	while((dmat_id=SifSetDma(dmat, count))==0){};

	/* Configure IMGDRV. It's more efficient to hack the parameters directly into IMGDRV than to pass them through the command line
 	If IMGDRV is modified(Or the offsets somehow changed), the offsets used here must be adjusted accordingly */
	*(void **)UNCACHED_SEG(&IMGDRV_irx_start[0x1A0])=iop_buffer1;
	*(unsigned int *)UNCACHED_SEG(&IMGDRV_irx_start[0x1A4])=size1;
	*(void **)UNCACHED_SEG(&IMGDRV_irx_start[0x1A8])=iop_buffer2;
	*(unsigned int *)UNCACHED_SEG(&IMGDRV_irx_start[0x1AC])=size2;

	while(SifDmaStat(dmat_id)>=0){};

	SifExecModuleBuffer(IMGDRV_irx_start, IMGDRV_irx_size, 0, NULL, NULL);
}

struct _iop_reset_pkt {
	struct t_SifCmdHeader header;
	int	arglen;
	int	mode;
	char	arg[80];
} __attribute__((aligned(16)));

/*--------------------------------------------------------------------------------------------------------------*/
/* A function that creates and send IOP reset SIF DMA packets                                                   */
/* containing commands to reset the IOP with a modified IOPRP image.                                            */
/*--------------------------------------------------------------------------------------------------------------*/
void SifIopResetBuffer(const void *buffer1, int size1, const void *buffer2, int size2)
{
	char *pArgs;
	int arglen;

	while(!SifIopReset(NULL, 0)){};

	if(buffer2 != NULL)
	{
		arglen = 11;
		pArgs = "img1:\0img0:";
	} else {
		arglen = 5;
		pArgs = "img0:";
	}

	while(!SifIopSync()){};

	SifInitRpc(0);
	SifInitIopHeap();
	SifLoadFileInit();

	sbv_patch_enable_lmb();

	LoadIMGDRV(buffer1, size1, buffer2, size2);

	SifSetReg(SIF_REG_SMFLAG, 0x40000);

	/* Load the IOPRP image asynchronously */
	LoadModuleAsync("rom0:UDNL", arglen, pArgs);

	SifSetReg(SIF_REG_SMFLAG, 0x10000);
	SifSetReg(SIF_REG_SMFLAG, 0x20000);
	SifSetReg(0x80000002, 0);
	SifSetReg(0x80000000, 0);

	/* Keep the RPC services on the EE side in sync with the IOP side after IOP reset */
	/* The EE would have to wait for the IOP to complete the reset anyway... so the code is placed here for efficiency */
	SifExitRpc();
	SifLoadFileExit();
	SifExitIopHeap();
}
