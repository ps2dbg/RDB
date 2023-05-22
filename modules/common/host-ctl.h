/*++

  Module name:
	host-ctl.c

  Description:
	Control Codes for host device over DECI2 Remote File Protocol.

--*/

#ifndef _HOST_CTL_H
#define _HOST_CTL_H

// devctl commands for DECI2FILE's host device
#define HOSTDEV_GET_MAX_READ		0x00007201
#define HOSTDEV_SET_MAX_READ		0x00007202
#define HOSTDEV_GET_MAX_WRITE		0x00007203
#define HOSTDEV_SET_MAX_WRITE		0x00007204

#endif
