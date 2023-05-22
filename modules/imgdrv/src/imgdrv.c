#include <stdio.h>
#include <sysclib.h>
#include <loadcore.h>
#include <ioman.h>

struct ImageInfo
{
    const void *image;
    s32 size;
};

static struct ImageInfo images[2] = {
    {(void *)0xDEC1DEC1, 0xDEC2DEC2},
    {(void *)0xDEC1DEC1, 0xDEC2DEC2},
};

int dummy_fs()
{
    return 0;
}

int open_fs(iop_file_t *fd, const char *path, int flags)
{
    fd->privdata = &images[fd->unit];
    return fd->unit;
}

int lseek_fs(iop_file_t *fd, int offset, int whence)
{
    if (whence == SEEK_END) {
        return (((struct ImageInfo *)fd->privdata)->size);
    } else {
        return 0;
    }
}

int read_fs(iop_file_t *fd, void *buffer, int size)
{
    memcpy(buffer, ((struct ImageInfo *)fd->privdata)->image, size);
    return size;
}

iop_device_ops_t my_device_ops =
    {
        dummy_fs, // init
        dummy_fs, // deinit
        NULL,     // dummy_fs,//format
        open_fs,  // open
        dummy_fs, // close_fs,//close
        read_fs,  // read
        NULL,     // dummy_fs,//write
        lseek_fs, // lseek
        /*dummy_fs,//ioctl
        dummy_fs,//remove
        dummy_fs,//mkdir
        dummy_fs,//rmdir
        dummy_fs,//dopen
        dummy_fs,//dclose
        dummy_fs,//dread
        dummy_fs,//getstat
        dummy_fs,//chstat*/
};

const u8 name[] = "img";
iop_device_t my_device = {
    name,
    IOP_DT_FS,
    1,
    name,
    &my_device_ops};

int _start(int argc, char **argv)
{
    // DelDrv("img");
    AddDrv((iop_device_t *)&my_device);

    return MODULE_RESIDENT_END;
}
