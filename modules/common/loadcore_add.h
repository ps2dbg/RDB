#ifndef _LOADCORE_ADD_H_
#define _LOADCORE_ADD_H_

typedef struct
{ // Incomplete structure definition. Provided only for compatibility, as I don't know what the full structure is like.
    void *callback;
} iop_init_entry_t;

typedef int (*BootupCallback)(iop_init_entry_t *next, int delayed);

int RegisterInitCompleteHandler(BootupCallback func, int priority, int *pResult);
#define I_RegisterInitCompleteHandler DECLARE_IMPORT(20, RegisterInitCompleteHandler);

#endif
