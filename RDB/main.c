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

#include "ioprstctrl.h"

extern unsigned char DECI2_img_start[];
extern unsigned int DECI2_img_size;

extern unsigned char DEV9_irx_start[];
extern unsigned int DEV9_irx_size;

extern unsigned char SMAP_irx_start[];
extern unsigned int SMAP_irx_size;

extern unsigned char NETMAN_irx_start[];
extern unsigned int NETMAN_irx_size;

extern unsigned char PS2IP_irx_start[];
extern unsigned int PS2IP_irx_size;

extern unsigned char DRVTIF_irx_start[];
extern unsigned int DRVTIF_irx_size;

extern unsigned char TIFINET_irx_start[];
extern unsigned int TIFINET_irx_size;

int main(int argc, char *argv[]){
	static const char *SmapLinkModeArgs[4]={
		"-no_auto\0""0x500",	//100Mbit FDX
		"-no_auto\0""0x480",	//100Mbit HDX
		"-no_auto\0""0x440",	//10Mbit FDX
		"-no_auto\0""0x420"	//10Mbit HDX
	};
	static const char *SmalLinkModeDescriptions[]={
		"10Mbit HDX",
		"10Mbit FDX",
		"100Mbit HDX",
		"100Mbit FDX"
	};
	char ip_address_str[16], subnet_mask_str[16], gateway_str[16], PS2IP_ModuleArgs[64], UDNL_Args[16];
	const char *SmapArgs;
	int result, SmapArgsLen, PS2IPArgsLen, EthernetLinkMode, size_IOPRP_img;
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

	scr_printf(	"========================================\n"
			"| Retail Debugging startup card v0.9.5 |\n"
			"========================================\n\n"
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
	switch(EthernetLinkMode){
		case NETMAN_NETIF_ETH_LINK_MODE_10M_HDX:
			SmapArgs=SmapLinkModeArgs[3];
			SmapArgsLen=15;
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_10M_FDX:
			SmapArgs=SmapLinkModeArgs[2];
			SmapArgsLen=15;
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_100M_HDX:
			SmapArgs=SmapLinkModeArgs[1];
			SmapArgsLen=15;
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_100M_FDX:
			SmapArgs=SmapLinkModeArgs[0];
			SmapArgsLen=15;
			break;
		default:
			SmapArgs=NULL;
			SmapArgsLen=0;
	}

	scr_printf(	"\nNetwork configuration:\n"
			"\tIP address:\t%s\n"
			"\tSubnet mask:\t%s\n"
			"\tGateway:\t\t%s\n"
			"\tEthernet link mode: %s\n\n", ip_address_str, subnet_mask_str, gateway_str, EthernetLinkMode>=0 && EthernetLinkMode<NETMAN_NETIF_ETH_LINK_MODE_COUNT?SmalLinkModeDescriptions[EthernetLinkMode]:"AUTO");

	PS2IPArgsLen=0;
	strcpy(PS2IP_ModuleArgs, ip_address_str);
	PS2IPArgsLen+=strlen(ip_address_str)+1;
	strcpy(&PS2IP_ModuleArgs[PS2IPArgsLen], subnet_mask_str);
	PS2IPArgsLen+=strlen(subnet_mask_str)+1;
	strcpy(&PS2IP_ModuleArgs[PS2IPArgsLen], gateway_str);
	PS2IPArgsLen+=strlen(gateway_str)+1;

	while(!SifIopSync()){};

	SifInitRpc(0);
	SifLoadFileInit();
	SifInitIopHeap();

	sbv_patch_enable_lmb();

	scr_printf("Loading DEV9...");
	if((result=SifExecModuleBuffer(DEV9_irx_start, DEV9_irx_size, 0, NULL, NULL))>=0){
		scr_printf("done!\n");
		SifExecModuleBuffer(NETMAN_irx_start, NETMAN_irx_size, 0, NULL, NULL);

		scr_printf("Loading SMAP...");
		if((result=SifExecModuleBuffer(SMAP_irx_start, SMAP_irx_size, SmapArgsLen, SmapArgs, NULL))>=0){
			scr_printf("done!\n");

			SifExecModuleBuffer(PS2IP_irx_start, PS2IP_irx_size, PS2IPArgsLen, PS2IP_ModuleArgs, NULL);
			SifExecModuleBuffer(DRVTIF_irx_start, DRVTIF_irx_size, 0, NULL, NULL);
			SifExecModuleBuffer(TIFINET_irx_start, TIFINET_irx_size, 0, NULL, NULL);

			scr_printf("\nInitialization complete.\n");

			RDBBootKernel();
		}else{
			scr_printf("failed!\nPlease check your network adaptor installation.\n");
		}
	}else{
		scr_printf("failed!\nPlease check your network adaptor installation.\n");
	}

	SifExitRpc();

	return 0;
}
