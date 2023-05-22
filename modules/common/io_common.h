/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: io_common.h 577 2004-09-14 14:41:46Z pixel $
# Shared IO structures and definitions
*/

#define O_RDONLY	0x0001
#define O_WRONLY	0x0002
#define O_RDWR		0x0003
#define O_NBLOCK	0x0010
#define O_APPEND	0x0100
#define O_CREAT		0x0200
#define O_TRUNC		0x0400
#define O_NOWAIT	0x8000

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Note! I translated (And added the "private" array) from the original Japanese SCE PS2 SDK 3.0.
	There may be errors in translation */

typedef struct {
	unsigned int mode;	/* File type + access attributes */

	unsigned int attr;	/* Device access attributes */
	unsigned int size;	/* Size of the file (Lower 32-bits) */
	unsigned char ctime[8];	/* File's creation time stamp */
	unsigned char atime[8];	/* File's last access time stamp */
	unsigned char mtime[8];	/* File's last modification time stamp */
	unsigned int hisize;	/* Size of the file (Upper 32-bits) */

	unsigned int private[6];/* Other data. Should be usable by the device driver. */
} io_stat_t;

typedef struct {
	io_stat_t stat;	/* File's statistics */
	char name[256];		/* File's name */
	void *private;		/* Can be used by the device driver */
} io_dirent_t;
