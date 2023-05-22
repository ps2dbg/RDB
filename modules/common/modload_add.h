#ifndef _MODLOAD_ADD_H_
#define _MODLOAD_ADD_H_

void *GetModloadInternalData(void **pInternalData);
#define I_GetModloadInternalData DECLARE_IMPORT(3, GetModloadInternalData);

#endif
