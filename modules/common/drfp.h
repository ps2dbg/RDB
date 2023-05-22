/*++

  Module name:
	drfp.h

  Description:
	DRFP Definitions.

--*/

#ifndef DRFP_H
#define DRFP_H

//
// General DRFP Packet Header
//

typedef struct _DRFP_HEADER {
	//
	// Request/Reply Code
	//
	u32										Code;

	//
	// Request Sequence Number
	//
	u32										SequenceNumber;
} DRFP_HEADER;

#define DRFP_CODE_OPEN						(0)
#define DRFP_CODE_OPENR						(1)
#define DRFP_CODE_CLOSE						(2)
#define DRFP_CODE_CLOSER					(3)
#define DRFP_CODE_READ						(4)
#define DRFP_CODE_READR						(5)
#define DRFP_CODE_WRITE						(6)
#define DRFP_CODE_WRITER					(7)
#define DRFP_CODE_SEEK						(8)
#define DRFP_CODE_SEEKR						(9)
#define DRFP_CODE_IOCTL						(10)
#define DRFP_CODE_IOCTLR					(11)
#define DRFP_CODE_REMOVE					(12)
#define DRFP_CODE_REMOVER					(13)
#define DRFP_CODE_MKDIR						(14)
#define DRFP_CODE_MKDIRR					(15)
#define DRFP_CODE_RMDIR						(16)
#define DRFP_CODE_RMDIRR					(17)
#define DRFP_CODE_DOPEN						(18)
#define DRFP_CODE_DOPENR					(19)
#define DRFP_CODE_DCLOSE					(20)
#define DRFP_CODE_DCLOSER					(21)
#define DRFP_CODE_DREAD						(22)
#define DRFP_CODE_DREADR					(23)
#define DRFP_CODE_GETSTAT					(24)
#define DRFP_CODE_GETSTATR					(25)
#define DRFP_CODE_CHSTAT					(26)
#define DRFP_CODE_CHSTATR					(27)
#define DRFP_CODE_FORMAT					(28)
#define DRFP_CODE_FORMATR					(29)
#define DRFP_CODE_RENAME					(30)
#define DRFP_CODE_RENAMER					(31)
#define DRFP_CODE_CHDIR						(32)
#define DRFP_CODE_CHDIRR					(33)
#define DRFP_CODE_SYNC						(34)
#define DRFP_CODE_SYNCR						(35)
#define DRFP_CODE_MOUNT						(36)
#define DRFP_CODE_MOUNTR					(37)
#define DRFP_CODE_UMOUNT					(38)
#define DRFP_CODE_UMOUNTR					(39)
#define DRFP_CODE_SEEK64					(40)
#define DRFP_CODE_SEEK64R					(41)
#define DRFP_CODE_DEVCTL					(42)
#define DRFP_CODE_DEVCTLR					(43)
#define DRFP_CODE_SYMLINK					(44)
#define DRFP_CODE_SYMLINKR					(45)
#define DRFP_CODE_READLINK					(46)
#define DRFP_CODE_READLINKR					(47)

//
// DRFP Error Codes
// These codes are identical to the ones used by the C library. In order to
// convert them to an IOP kernel error code, simply change their sign.
//

#include "errno.h"

#define DRFP_ERR(e)							(e)

#define DRFP_UNDEF							0xFFFF
#define DRFP_UNDEF_MASK						0xFFFF

#endif // ndef(DRFP_H)
