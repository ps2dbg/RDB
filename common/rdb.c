#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <syscallnr.h>
#include <debug.h>
#include <errno.h>
#include <libcdvd.h>
#include "libcdvd_add.h"
#include <malloc.h>
#include <osd_config.h>

#include "ipconfig.h"
#include "OSDConfig.h"
#include "rdb.h"

struct OSDConfig{
	ConfigParam config;
	Config2Param config2;
};

static unsigned char StockKernelLoaded;
static int NewKernelSize;
static void *NewKernel;

static void InitializeKernelMemory(void){
	unsigned int i;

	for (i = 0xA0000000; i < 0xA0100000; i += 128) {
		asm volatile(
			"\tsq $0, 0(%0)\n"
			"\tsq $0, 16(%0)\n"
			"\tsq $0, 32(%0)\n"
			"\tsq $0, 48(%0)\n"
			"\tsq $0, 64(%0)\n"
			"\tsq $0, 80(%0)\n"
			"\tsq $0, 96(%0)\n"
			"\tsq $0, 112(%0)\n"
			:: "r" (i)
		);
	}
}

static void InstallKernel(const void *kernel, int size){
	memcpy((void*)0xA0000000, kernel, size);
}

static void InvalidateWholeInstructionCache(void){
	unsigned int i;

	for (i = 0; i < 0x2000; i += 64) {
		asm volatile(
			"\tsync.p\n"
			"\tcache 0x07, 0(%0)\n"
			"\tcache 0x07, 1(%0)\n"
			"\tsync.p\n"
			:: "r" (i)
		);
	}

	asm volatile(
		"\tcache 0x0C, 0($zero)\n"
		"\tsync.p\n"
		::
	);
}

static void WriteBackWholeDataCache(void){
	unsigned int i;

	for (i = 0; i < 0x1000; i += 64) {
		asm volatile(
			"\tsync\n"
			"\tcache 0x14, 0(%0)\n"
			"\tsync\n"
			"\tcache 0x14, 1(%0)\n"
			"\tsync\n"
			:: "r" (i)
		);
	}
}

static void UpdateKernel(const void *kernel, int size){
	*(volatile s32*)0x70003FF0=GetMemorySize();

	SifInitCmd();
	SifExitRpc();

	DIntr();
	ee_kmode_enter();

	WriteBackWholeDataCache();
	InvalidateWholeInstructionCache();

	InitializeKernelMemory();
	InstallKernel(kernel, size);
}

extern void BootKernel(void);

int RDBInitNetworkConfiguration(char *ip_address_str, char *subnet_mask_str, char *gateway_str, int *EthernetLinkMode)
{
	int result;

	*EthernetLinkMode=-1;
	if((result = ParseConfig("mc0:/SYS-CONF/IPCONFIG.DAT", ip_address_str, subnet_mask_str, gateway_str, EthernetLinkMode))!=0)
	{
		if((result = ParseConfig("mc1:/SYS-CONF/IPCONFIG.DAT", ip_address_str, subnet_mask_str, gateway_str, EthernetLinkMode))!=0)
		{
			strcpy(ip_address_str, "192.168.0.10");
			strcpy(subnet_mask_str, "255.255.255.0");
			strcpy(gateway_str, "192.168.0.1");
		}
	}

	return result;
}

#define GETJADDR(addr)	(((addr)&0x03FFFFFF)<<2)
#define JAL(addr)	(0x0c000000|(0x3ffffff&((addr)>>2)))

/*	Unlike the normal EE kernel, a TDB startup card kernel has slightly different behaviour.
	So make these patches to give the CEX kernel these behaviour. */
static void PatchCEXKernel(void)
{
	int (*pSifSetReg)(u32 register_num, int register_value);
	int i;
	vu32 *patch;
	void (*pInitialize)(void);
	void (*pInitializeEEGS)(void);
	void (*pInitializeMoreStuff)(void);
	const u32 ResetPattern[] = {
		0xac000000,	//sw xx, $0000(xx)
		0x34001000,	//ori xx, xx, $1000
		0x24000200,	//addiu xx, zero, $0200		RESET = 1
		0xfc000000,	//sd xx, $0000(xx)		GS CSR
	};
	const u32 ResetPattern_mask[] = {
		0xFC00FFFF,
		0xFC00FFFF,
		0xFFE0FFFF,
		0xFC00FFFF,
	};
	const u32 SMODE1ResetPattern[] = {
		0x0c000000,	//jal ????	<- jump to function that resets various things, including writing to SMODE1
		0x00000000,	//nop
		0x3c028002,	//lui v0, $8002
		0x8c420000,	//lw v0, ????(v0)
	};
	const u32 SMODE1ResetPattern_mask[] = {
		0xFC000000,
		0xFFFFFFFF,
		0xFFFFFFFF,
		0xFFFF0000,
	};

	patch = (vu32*)0xA0001098;

	//Set the remote RPC initialized status because it is already initialized.
	//The EE kernel wipes the SIF register buffer at initialization, so it isn't possible to set the value directly from here.
	pSifSetReg = GetSyscallHandler(__NR_SifSetReg);
	patch[0] = 0x3c048000;			//lui a0,0x8000
	patch[1] = 0x34840002;			//ori a0,a0,0x2
	patch[2] = JAL((u32)pSifSetReg);	//jal	SifSetReg
	patch[3] = 0x24050001;			//li	a1,1

	//Add this break at startup, so that the debugger will be broken into at boot.
	patch[4] = 0x3c028000;	//lui v0, $8000
	patch[5] = 0x244210a8;	//addiu v0, v0, $10a8
	patch[6] = 0x40827000;	//mtc0 v0, EPC
	patch[7] = 0x0000040f;	//sync.p
	patch[8] = 0x0000000d;	//break (00000)

	//Change the Exit() syscall to a break
	patch = (vu32*)0xA0000d80;
	patch[0] = 0x03ffffcd;	//break (fffff)

	//To keep the text displayed on the screen, SCE also avoided calling SetGsCrt within Initialize Start.
	pInitialize = (void*)GETJADDR(*(vu32*)0xA0001038);	//Jump to initialize from the kernel entry point.
	patch = (vu32*)(((u32)pInitialize + 0x34) | 0xA0000000);
	*patch = 0x00000000;	//NOP

	//...and it didn't write 1 to the RESET bit of the GS CSR (within Initialize EEGS) either.
	pInitializeEEGS = (void*)GETJADDR(*(vu32*)(((u32)pInitialize + 0x24) | 0xA0000000));
	patch = (vu32*)((u32)pInitializeEEGS | 0xA0000000);
	for(i = 0; i < 64; i++,patch++)
	{
		if((patch[0] & ResetPattern_mask[0]) == ResetPattern[0]
			&& (patch[1] & ResetPattern_mask[1]) == ResetPattern[1]
			&& (patch[2] & ResetPattern_mask[2]) == ResetPattern[2]
			&& (patch[3] & ResetPattern_mask[3]) == ResetPattern[3])
		{
			patch[3] = 0x00000000;	//NOP
			break;
		}
	}

	/*	Later kernels (ROM v1.70 and later) also had additional initialization for various devices, which also included a write to SMODE1.
		The call to the function that performs these additional steps occurs after the RESET bit of the CSR is written to (above).
		This is also required to prevent the screen from being cleared. */
	for(i = 0; i < 32; i++,patch++)
	{
		if((patch[0] & SMODE1ResetPattern_mask[0])  == SMODE1ResetPattern[0]
			&& (patch[1] & SMODE1ResetPattern_mask[1])  == SMODE1ResetPattern[1]
			&& (patch[2] & SMODE1ResetPattern_mask[2])  == SMODE1ResetPattern[2]
			&& (patch[3] & SMODE1ResetPattern_mask[3])  == SMODE1ResetPattern[3])
		{
			pInitializeMoreStuff = (void*)GETJADDR(*patch);
			patch = (vu32*)(((u32)pInitializeMoreStuff + 0x2C) | 0xA0000000);
			if(*patch == 0xfc440000)	//sd a0, $0000(v0)
				*patch = 0x00000000;	//NOP
			break;
		}
	}	
}

/*	The TDB startup kernel will retrieve the system configuration from the scratchpad,
	while TOOL kernels have the values hardcoded.
	CEX/DEX kernels have the values set by the browser and it is desirable to
	not take up additional space for a patch, hence this hack has to be done here. */
static void CEXKernelInitSettings(void)
{
	struct OSDConfig *config = (struct OSDConfig *)0x70000000;

	void (*pSetOsdConfigParam)(void *config);
	void (*pGetOsdConfigParam)(void* addr);
	void (*pSetOsdConfigParam2)(void* config, s32 size, s32 offset);
	void (*pSetGsVParam)(s32 param);

	pSetOsdConfigParam = GetSyscallHandler(__NR_SetOsdConfigParam);
	pGetOsdConfigParam = GetSyscallHandler(__NR_GetOsdConfigParam);
	pSetOsdConfigParam2 = GetSyscallHandler(__NR_SetOsdConfigParam2);
	pSetGsVParam = GetSyscallHandler(__NR_SetGsVParam);

	pSetOsdConfigParam(&config->config);
	pGetOsdConfigParam(&config->config);
	if(config->config.version!=0)
		pSetOsdConfigParam2(&config->config2, 4, 0);
	pSetGsVParam(config->config.videoOutput?1:0);
}

static unsigned char ctoi(char char_in1, char char_in2)
{
	if ((char_in1 > 0x39) || (char_in1 < 0x30))
	{
		char_in1 = 0x30;
	}
	if ((char_in2 > 0x39) || (char_in2 < 0x30))
	{
		char_in2 = 0x30;
	}

	return ((char_in1 - 0x30)*10) + (char_in2 - 0x30);
}

static int LoadReplacementKernel(const char *path, void **buffer){
	FILE *file;
	int size;

	size=0;
	*buffer=NULL;
	if((file=fopen(path, "rb"))!=NULL)
	{
		fseek(file, 0, SEEK_END);
		size=ftell(file);
		rewind(file);

		if((*buffer=memalign(64, size))!=NULL)
		{
			if(fread(*buffer, 1, size, file)!=size)
			{
				free(*buffer);
				*buffer = NULL;
				size=0;
			}
		}
		else size=0;

		fclose(file);
	}

	return size;
}


int RDBLoadKernel(void)
{
	int result;

	StockKernelLoaded = 0;
	if((NewKernelSize=LoadReplacementKernel("KERNEL.bin", &NewKernel))>0)
	{
		scr_printf("Loaded replacement EE kernel.\n");
		result = 0;
	} else {
		if((NewKernelSize=LoadReplacementKernel("rom0:KERNEL", &NewKernel))>0)
		{
			StockKernelLoaded = 1;
			result = 1;
			scr_printf("Loaded stock EE kernel.\n");
		} else {
			scr_printf("Error: Could not not any EE kernel.\n");
			result = -ENOENT;
		}
	}

	return result;
}

int RDBLoadIOPRP(void **buffer, int *size)
{
	FILE *file;
	int result;

	*size = 0;
	*buffer = NULL;
	result = 0;
	if((file = fopen("IOPRP.img", "rb")) != NULL)
	{
		fseek(file, 0, SEEK_END);
		*size = ftell(file);
		rewind(file);

		if((*buffer=memalign(64, *size))!=NULL)
		{
			if(fread(*buffer, 1, *size, file)!=*size)
			{
				free(*buffer);
				*size = 0;
				*buffer = NULL;
				result = -EIO;
			}
		} else {
			*size = 0;
			result = -ENOMEM;
		}

		fclose(file);
	} else
		result = -ENOENT;

	return result;
}

void RDBInitConsole(void)
{
	struct OSDConfig *config = (struct OSDConfig *)0x70000000;
	char romver[16], RomName[4];
	FILE *file;

	scr_printf("Initializing console...");

	if((file = fopen("rom0:ROMVER", "r")) != NULL)
	{
		fread(romver, 1, sizeof(romver), file);
		fclose(file);

		// e.g. 0160HC = 1,60,'H','C'
		RomName[0]=ctoi(romver[0], romver[1]);
		RomName[1]=ctoi(romver[2], romver[3]);
		RomName[2]=romver[4];
		RomName[3]=romver[5];

		sceCdAltBootCertify(RomName);
	}
	//Store a copy of the system configuration in the scratchpad. The TDB startup card's kernel will retrieve them.
	OSDLoadConfigFromNVM(&config->config, &config->config2);

	scr_printf("done!\n");
}

int RDBBootKernel(void)
{
	if(NewKernel!=NULL)
	{
		UpdateKernel(NewKernel, NewKernelSize);
		if(StockKernelLoaded)
		{
			PatchCEXKernel();
			CEXKernelInitSettings();
		}
		BootKernel();
	}

	return -1;
}
