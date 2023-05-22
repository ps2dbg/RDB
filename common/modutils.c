#include <stdio.h>
#include <loadfile.h>
#include <iopheap.h>
#include <kernel.h>
#include <sifdma.h>
#include <sifrpc.h>
#include <string.h>

#include "modutils.h"

#define LF_PATH_MAX 252
#define LF_ARG_MAX  252

enum _lf_functions {
    LF_F_MOD_LOAD = 0,
    LF_F_ELF_LOAD,

    LF_F_SET_ADDR,
    LF_F_GET_ADDR,

    LF_F_MG_MOD_LOAD,
    LF_F_MG_ELF_LOAD,

    LF_F_MOD_BUF_LOAD,

    LF_F_MOD_STOP,
    LF_F_MOD_UNLOAD,

    LF_F_SEARCH_MOD_BY_NAME,
    LF_F_SEARCH_MOD_BY_ADDRESS,
};

struct _lf_module_load_arg
{
    union
    {
        int arg_len;
        int result;
    } p;
    int modres;
    char path[LF_PATH_MAX];
    char args[LF_ARG_MAX];
} __attribute__((aligned(16)));

extern SifRpcClientData_t _lf_cd; /* Declared in PS2SDK library */
/*----------------------------------------------------------------------------------------*/
/* Load an irx module from path, without waiting for the module to be loaded.   */
/*----------------------------------------------------------------------------------------*/
void LoadModuleAsync(const char *path, int arg_len, const char *args)
{
    struct _lf_module_load_arg arg;

    memset(&arg, 0, sizeof(struct _lf_module_load_arg));

    strcpy(arg.path, path);

    arg.p.arg_len = arg_len;
    memcpy(arg.args, args, arg_len);

    SifCallRpc(&_lf_cd, LF_F_MOD_LOAD, SIF_RPC_M_NOWAIT, &arg, sizeof(struct _lf_module_load_arg), NULL, 0, NULL, NULL);
}

struct _lf_module_buffer_load_arg
{
    union
    {
        void *ptr;
        int result;
    } p;
    union
    {
        int arg_len;
        int modres;
    } q;
    char unused[LF_PATH_MAX];
    char args[LF_ARG_MAX];
} ALIGNED(16);

void _SifLoadModuleBufferAsync(void *ptr, int arg_len, const char *args)
{
    struct _lf_module_buffer_load_arg arg;

    memset(&arg, 0, sizeof arg);

    arg.p.ptr = ptr;
    if (args && arg_len) {
        arg.q.arg_len = arg_len > LF_ARG_MAX ? LF_ARG_MAX : arg_len;
        memcpy(arg.args, args, arg.q.arg_len);
    } else {
        arg.q.arg_len = 0;
    }

    SifCallRpc(&_lf_cd, LF_F_MOD_BUF_LOAD, SIF_RPC_M_NOWAIT, &arg, sizeof arg, NULL, 0,
               NULL, NULL);
}

void SifExecModuleBufferAsync(void *ptr, u32 size, u32 arg_len, const char *args)
{
    SifDmaTransfer_t dmat;
    void *iop_addr;
    int res;
    unsigned int qid;

    /* Round the size up to the nearest 16 bytes. */
    size = (size + 15) & -16;

    if (!(iop_addr = SifAllocIopHeap(size)))
        return; // -E_IOP_NO_MEMORY;

    dmat.src = ptr;
    dmat.dest = iop_addr;
    dmat.size = size;
    dmat.attr = 0;
    //	SifWriteBackDCache(ptr, size);	//Caller handles the cache.
    qid = SifSetDma(&dmat, 1);

    if (!qid)
        return; // -1; // should have a better error here...

    while (SifDmaStat(qid) >= 0)
        ;

    _SifLoadModuleBufferAsync(iop_addr, arg_len, args);
    //	SifFreeIopHeap(iop_addr);	//Do not free memory because the module is still being loaded.
}
