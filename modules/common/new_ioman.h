//---------------------------------------------------------------------------
// File name:   ioman.h
// Copyright:   2003 Marcus R. Brown <mrbrown@0xd6.org>
// License:     Licenced under Academic Free License version 2.0
// Information: Review ps2sdk README & LICENSE files for further details.
//---------------------------------------------------------------------------
// $Id: ioman.h 1354 2006-09-04 06:49:00Z Herben $
// New changes 2007-05-06 by Ronald Andersson (mailto:dlanor@bredband.net)
// IOMAN definitions and imports.
//---------------------------------------------------------------------------
#ifndef IOP_IOMAN_H
#define IOP_IOMAN_H

#include <types.h>
#include <irx.h>

#include "io_common.h" /* Added for compatibility */
#include <sys/fcntl.h>

//---------------------------------------------------------------------------
// Definitions for device drivers

/* Device types.  */
#define IOP_DT_CHAR  0x01
#define IOP_DT_CONS  0x02
#define IOP_DT_BLOCK 0x04
#define IOP_DT_RAW   0x08
#define IOP_DT_FS    0x10
/* Added from iomanX; Supported by newer IOMAN modules */
#define IOP_DT_FSEXT 0x10000000 /* Supports calls after chstat().  */

/* File objects passed to driver operations.  */
typedef struct _iop_file
{
    int mode;                   /* File open mode.  */
    int unit;                   /* HW device unit number.  */
    struct _iop_device *device; /* Device driver.  */
    void *privdata;             /* The device driver can use this however it
                           wants.  */
} iop_file_t;

typedef struct _iop_device
{
    const char *name;
    unsigned int type;
    unsigned int version; /* Not so sure about this one.  */
    const char *desc;
    struct _iop_device_ops *ops;
} iop_device_t;

typedef struct _iop_device_ops
{
    int (*init)(iop_device_t *);
    int (*deinit)(iop_device_t *);
    int (*format)(iop_file_t *, ...);
    int (*open)(iop_file_t *, const char *, int, int mode);
    int (*close)(iop_file_t *);
    int (*read)(iop_file_t *, void *, int);
    int (*write)(iop_file_t *, const void *, int);
    int (*lseek)(iop_file_t *, long, int);
    int (*ioctl)(iop_file_t *, long, void *);
    int (*remove)(iop_file_t *, const char *);
    int (*mkdir)(iop_file_t *, const char *, int mode);
    int (*rmdir)(iop_file_t *, const char *);
    int (*dopen)(iop_file_t *, const char *);
    int (*dclose)(iop_file_t *);
    int (*dread)(iop_file_t *, io_dirent_t *);               /* changed *void to *io_dirent_t */
    int (*getstat)(iop_file_t *, const char *, io_stat_t *); /* changed *void to *io_stat_t */
    int (*chstat)(iop_file_t *, const char *, io_stat_t *, unsigned int);
    // RA NB: The table should be extended here, but I'm unsure of the exact specs
    // RA NB: Functions should correspond to iomanX, but may differ in arguments
    // RA NB: For such cases the definitions here may be wrong, being iomanX copies
    /* Extended ops start here.  */
    int (*rename)(iop_file_t *, const char *, const char *);
    int (*chdir)(iop_file_t *, const char *);
    int (*sync)(iop_file_t *, const char *, int);
    int (*mount)(iop_file_t *, const char *, const char *, int, void *, unsigned int);
    int (*umount)(iop_file_t *, const char *);
    int (*lseek64)(iop_file_t *, long long, int);
    int (*devctl)(iop_file_t *, const char *, int, void *, unsigned int, void *, unsigned int);
    int (*symlink)(iop_file_t *, const char *, const char *);
    int (*readlink)(iop_file_t *, const char *, char *, unsigned int);
    int (*ioctl2)(iop_file_t *, int, void *, unsigned int, void *, unsigned int);
} iop_device_ops_t;

//---------------------------------------------------------------------------
// Macros for start and end of imports table

#define ioman_IMPORTS_start DECLARE_IMPORT_TABLE(ioman, 1, 1)
#define ioman_IMPORTS_end   END_IMPORT_TABLE

//---------------------------------------------------------------------------
// Functions valid for all IOMAN modules, including those in the PS2 BIOS ROM

int open(const char *name, int flag, int mode);
#define I_open DECLARE_IMPORT(4, open)
int close(int fd);
#define I_close DECLARE_IMPORT(5, close)
int read(int fd, void *ptr, size_t size);
#define I_read DECLARE_IMPORT(6, read)
int write(int fd, const void *ptr, size_t size);
#define I_write DECLARE_IMPORT(7, write)
int lseek(int fd, long pos, int mode);
#define I_lseek DECLARE_IMPORT(8, lseek)

int format(const char *dev);
#define I_format DECLARE_IMPORT(18, format)

int AddDrv(iop_device_t *device);
#define I_AddDrv DECLARE_IMPORT(20, AddDrv);
int DelDrv(const char *name);
#define I_DelDrv DECLARE_IMPORT(21, DelDrv);

//---------------------------------------------------------------------------
// Functions valid for all iomanX, but also present in some IOMAN modules
// RA NB: These are mostly copied from iomanX.h, so some arguments may be wrong
// RA NB: but I have corrected the argument of 'mkdir' to ioman standard

int ioctl(int fd, long cmd, void *param);
#define I_ioctl DECLARE_IMPORT(9, ioctl)
int remove(const char *name);
#define I_remove DECLARE_IMPORT(10, remove)

int mkdir(const char *path, int mode);
#define I_mkdir DECLARE_IMPORT(11, mkdir)
int rmdir(const char *path);
#define I_rmdir DECLARE_IMPORT(12, rmdir)
int dopen(const char *path);
#define I_dopen DECLARE_IMPORT(13, dopen)
int dclose(int fd);
#define I_dclose DECLARE_IMPORT(14, dclose)
int dread(int fd, void *buf);
#define I_dread DECLARE_IMPORT(15, dread)

int getstat(const char *name, void *stat);
#define I_getstat DECLARE_IMPORT(16, getstat)
int chstat(const char *name, void *stat, unsigned int statmask);
#define I_chstat DECLARE_IMPORT(17, chstat)

// The format function below is probably not valid for any ioman modules, so
// I'm commenting it out, but keeping it in this form for information only.
// The function really used is defined further above.
///* This can take take more than one form.  */
// int format(const char *dev, const char *blockdev, void *arg, size_t arglen);
//#define I_format DECLARE_IMPORT(18, format)

//---------------------------------------------------------------------------
// Functions valid for newer iomanX, but also in some new SCE IOMAN modules
// RA NB: These are mostly copied from iomanX.h, so some arguments may be wrong

/* The newer calls - these are NOT supported by the older IOMAN.  */
int rename(const char *old, const char *new);
#define I_rename DECLARE_IMPORT(25, rename)
int chdir(const char *name);
#define I_chdir DECLARE_IMPORT(26, chdir)
int sync(const char *dev, int flag);
#define I_sync DECLARE_IMPORT(27, sync)
int mount(const char *fsname, const char *devname, int flag, void *arg, size_t arglen);
#define I_mount DECLARE_IMPORT(28, mount)
int umount(const char *fsname);
#define I_umount DECLARE_IMPORT(29, umount)
int lseek64(int fd, long long offset, int whence);
#define I_lseek64 DECLARE_IMPORT(30, lseek64)
int devctl(const char *name, int cmd, void *arg, size_t arglen, void *buf, size_t buflen);
#define I_devctl DECLARE_IMPORT(31, devctl)
int symlink(const char *old, const char *new);
#define I_symlink DECLARE_IMPORT(32, symlink)
int readlink(const char *path, char *buf, size_t buflen);
#define I_readlink DECLARE_IMPORT(33, readlink)
int ioctl2(int fd, int cmd, void *arg, size_t arglen, void *buf, size_t buflen);
#define I_ioctl2 DECLARE_IMPORT(34, ioctl2)

//---------------------------------------------------------------------------
#endif /* IOP_IOMAN_H */
//---------------------------------------------------------------------------
// End of file: ioman.h
//---------------------------------------------------------------------------
