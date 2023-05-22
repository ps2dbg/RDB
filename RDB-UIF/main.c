#include <kernel.h>
#include <fileio.h>
#include <loadfile.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <sifrpc.h>
#include <netman.h>
#include <libcdvd.h>
#include "libcdvd_add.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <debug.h>
#include <sbv_patches.h>

#include "rdb.h"
#include "ioprstctrl.h"
#include "UIFConfig.h"

extern unsigned char DECI2_img_start[];
extern unsigned int DECI2_img_size;

extern unsigned char DEV9_irx_start[];
extern unsigned int DEV9_irx_size;

extern unsigned char DECI2DRU_irx_start[];
extern unsigned int DECI2DRU_irx_size;

static int ConfigureImage(void *start, int size, const char *ip_address, const char *subnet_mask, const char *gateway, int EthernetLinkMode, int slot){
	static const struct UIFBootConfiguration BootConfiguration={
		0,
		{0, 0x5E0},
		{192, 168, 0, 10},
		{255, 255, 255, 0},
		{192, 168, 0, 1},
	};
	struct UIFBootConfiguration *pBootConfiguration;
	unsigned int offset;
	char *pBootSmapArgs;
	u16 SMAPConf;
	u8 SMAPNoAuto;
	int result;

	for(pBootConfiguration=NULL,offset=0; offset<size; offset+=4){
		if(!memcmp(&((u8*)start)[offset], &BootConfiguration, sizeof(BootConfiguration))){
			pBootConfiguration=(struct UIFBootConfiguration*)&((u8*)start)[offset];
			break;
		}
	}

	if(pBootConfiguration!=NULL){
		pBootConfiguration->BootupMcSlot = (u8)slot;

		ParseNetAddr(ip_address, pBootConfiguration->ip_address);
		ParseNetAddr(subnet_mask, pBootConfiguration->subnet_mask);
		ParseNetAddr(gateway, pBootConfiguration->gateway);

		switch(EthernetLinkMode){
			case NETMAN_NETIF_ETH_LINK_MODE_10M_HDX:
				SMAPConf = 0x420;
				SMAPNoAuto = 1;
				break;
			case NETMAN_NETIF_ETH_LINK_MODE_10M_FDX:
				SMAPConf = 0x440;
				SMAPNoAuto = 1;
				break;
			case NETMAN_NETIF_ETH_LINK_MODE_100M_HDX:
				SMAPConf = 0x480;
				SMAPNoAuto = 1;
				break;
			case NETMAN_NETIF_ETH_LINK_MODE_100M_FDX:
				SMAPConf = 0x500;
				SMAPNoAuto = 1;
				break;
			default:
				SMAPConf = 0x5E0;
				SMAPNoAuto = 0;
		}

		pBootConfiguration->smap.conf = SMAPConf;
		pBootConfiguration->smap.noAuto = SMAPNoAuto;

		result=0;
	}
	else result=-1;

	return result;
}

int main(int argc, char *argv[]){
	static const char *SmalLinkModeDescriptions[]={
		"10Mbit HDX",
		"10Mbit FDX",
		"100Mbit HDX",
		"100Mbit FDX"
	};
	char ip_address_str[16], subnet_mask_str[16], gateway_str[16], PS2IP_ModuleArgs[64], UDNL_Args[16];
	int result, PS2IPArgsLen, EthernetLinkMode, size_IOPRP_img;
	void *IOPRP_img;

	SifInitRpc(0);
	while(!SifIopReset(NULL, 0)){};
	while(!SifIopSync()){};

	SifInitRpc(0);
	SifLoadFileInit();
	sceCdInit(SCECdINoD);
	InitLibcdvd_addOnFunctions();
	fioInit();

	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:MCMAN", 0, NULL);

	init_scr();

	scr_printf(	"==============================================\n"
			"| Retail Debugging startup card v0.9.5 (UIF) |\n"
			"==============================================\n\n"
			"Build " __DATE__ " " __TIME__"\n"
			"Copyright(C) SP193, 2014-2017\n"
			"Copyright(C) SilverBull/ASSEMblergames forums, 2006-2017\n\n");


	RDBInitConsole();

	scr_printf("Loading network configuration...");
	result=RDBInitNetworkConfiguration(ip_address_str, subnet_mask_str, gateway_str, &EthernetLinkMode);
	scr_printf(result==0?"done!\n":"failed!\nDefault settings loaded.\n");

	RDBLoadKernel();
	if(RDBLoadIOPRP(&IOPRP_img, &size_IOPRP_img) == 0)
		scr_printf("IOPRP.img loaded.\n");

	SifIopResetBuffer(DECI2_img_start, DECI2_img_size, IOPRP_img, size_IOPRP_img);

	//Do something useful, while the IOP resets.
	ConfigureImage(DECI2DRU_irx_start, DECI2DRU_irx_size, ip_address_str, subnet_mask_str, gateway_str, EthernetLinkMode, 0);

	scr_printf(	"\nNetwork configuration:\n"
			"\tIP address:\t%s\n"
			"\tSubnet mask:\t%s\n"
			"\tGateway:\t\t%s\n"
			"\tEthernet link mode: %s\n\n", ip_address_str, subnet_mask_str, gateway_str, EthernetLinkMode>=0 && EthernetLinkMode<NETMAN_NETIF_ETH_LINK_MODE_COUNT?SmalLinkModeDescriptions[EthernetLinkMode]:"AUTO");

	while(!SifIopSync()){};

	SifInitRpc(0);
	SifLoadFileInit();
	SifInitIopHeap();

	sbv_patch_enable_lmb();

	scr_printf("Loading DEV9...");
	if((result=SifExecModuleBuffer(DEV9_irx_start, DEV9_irx_size, 0, NULL, NULL))>=0){
		scr_printf(	"done!\n"
				"Loading DECI2DRU...");

		SifExecModuleBuffer(DECI2DRU_irx_start, DECI2DRU_irx_size, 0, NULL, NULL);

		scr_printf(	"Done!\n"
				"Initialization complete.\n");

		RDBBootKernel();
	}else{
		scr_printf("failed!\nPlease check your network adaptor installation.\n");
	}

	SifExitRpc();

	return 0;
}
