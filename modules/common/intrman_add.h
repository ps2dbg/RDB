#ifndef _INTRMAN_ADD_H_
#define _INTRMAN_ADD_H_

int CpuInvokeInKmode(void *function, ...);
#define I_CpuInvokeInKmode DECLARE_IMPORT(14, CpuInvokeInKmode);

void DisableDispatchIntr(int irq);
#define I_DisableDispatchIntr DECLARE_IMPORT(15, DisableDispatchIntr);

#endif
