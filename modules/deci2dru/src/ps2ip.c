/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

/**
 * @file
 * PS2 TCP/IP STACK FOR IOP
 */

#include <types.h>
#include <stdio.h>
#include <loadcore.h>
#include <thbase.h>
#include <sysclib.h>
#include <sysmem.h>

#include "smap_internal.h"

#include "lwip/init.h"
#include "lwip/sys.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/inet.h"
#include "netif/etharp.h"

#include "ps2ip_internal.h"

err_t ps2ip_input(struct pbuf *pInput, struct netif *pNetIF)
{
    err_t result;

    if ((result = pNetIF->input(pInput, pNetIF)) != ERR_OK)
        pbuf_free(pInput);

    return result;
}

static struct netif NIF;

void SMapLowLevelInput(struct pbuf *pBuf)
{
    // When we receive data, the interrupt-handler will invoke this function, which means we are in an interrupt-context. Pass on
    // the received data to ps2ip.

    ps2ip_input(pBuf, &NIF);
}

static err_t
SMapLowLevelOutput(struct netif *pNetIF, struct pbuf *pOutput)
{
    err_t result;
    struct pbuf *pbuf;

    result = ERR_OK;
    if (pOutput->tot_len > pOutput->len) {
        pbuf_ref(pOutput); // Increment reference count because LWIP must free the PBUF, not the driver!
        if ((pbuf = pbuf_coalesce(pOutput, PBUF_RAW)) != pOutput) {
            SMAPSendPacket(pbuf->payload, pbuf->len);
            pbuf_free(pbuf);
        } else
            result = ERR_MEM;
    } else {
        SMAPSendPacket(pOutput->payload, pOutput->len);
    }

    return result;
}

// Should be called at the beginning of the program to set up the network interface.
static err_t SMapIFInit(struct netif *pNetIF)
{
    pNetIF->name[0] = 's';
    pNetIF->name[1] = 'm';
    pNetIF->output = &etharp_output; // For LWIP 1.3.0 and later.
    pNetIF->linkoutput = &SMapLowLevelOutput;
    pNetIF->hwaddr_len = NETIF_MAX_HWADDR_LEN;
    pNetIF->flags |= (NETIF_FLAG_ETHARP | NETIF_FLAG_BROADCAST); // For LWIP v1.3.0 and later.
    pNetIF->mtu = 1500;

    // Get MAC address.
    SMAPGetMACAddress(pNetIF->hwaddr);
    SMAPStart();

    return ERR_OK;
}

void PS2IPLinkStateUp(void)
{
    netif_set_link_up(&NIF);
}

void PS2IPLinkStateDown(void)
{
    netif_set_link_down(&NIF);
}

int InitPS2IP(const unsigned char *ip_address, const unsigned char *subnet_mask, const unsigned char *gateway)
{
    struct ip4_addr IP, NM, GW;

    // Set some defaults.
    IP4_ADDR(&IP, ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    IP4_ADDR(&NM, subnet_mask[0], subnet_mask[1], subnet_mask[2], subnet_mask[3]);
    IP4_ADDR(&GW, gateway[0], gateway[1], gateway[2], gateway[3]);

    lwip_init();
    netif_add(&NIF, &IP, &NM, &GW, &NIF, &SMapIFInit, &ethernet_input);
    netif_set_default(&NIF);
    netif_set_up(&NIF); // For LWIP v1.3.0 and later.

    return 0;
}
