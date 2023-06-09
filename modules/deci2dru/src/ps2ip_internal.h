/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#ifndef IOP_PS2IP_INTERNAL_H
#define IOP_PS2IP_INTERNAL_H

#include <types.h>

#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#else
#define dbgprintf(args...)
#endif

#define netif_dhcp_data(netif) ((struct dhcp *)(netif)->client_data[LWIP_NETIF_CLIENT_DATA_INDEX_DHCP])

int InitPS2IP(const unsigned char *ip_address, const unsigned char *subnet_mask, const unsigned char *gateway);
void SMapLowLevelInput(struct pbuf *pBuf);
void ps2ip_InitTimer(void);
void PS2IPLinkStateUp(void);
void PS2IPLinkStateDown(void);

#endif // !defined(IOP_PS2IP_INTERNAL_H)
