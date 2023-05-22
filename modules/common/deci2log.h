/*++

  Module name:
	deci2log.h

  Description:
	DECI2 Logger API Definitions.

--*/

#ifndef IOP_DECI2LOG_H
#define IOP_DECI2LOG_H

#include <types.h>
#include <irx.h>
#include <stdarg.h>

#include "sysclib.h"

//
// DECI2LOG export table
//

#define deci2log_IMPORTS_start DECLARE_IMPORT_TABLE(deci2log, 1, 1)
#define deci2log_IMPORTS_end   END_IMPORT_TABLE

#define I_sceDeci2SetDebugFormatRoutine DECLARE_IMPORT(4, sceDeci2SetDebugFormatRoutine)
#define I_sceDeci2SetDebugFlags         DECLARE_IMPORT(5, sceDeci2SetDebugFlags)

//
// API routines
//

typedef int (*format_callback_t)(print_callback_t, void *, const char *, va_list);

void sceDeci2SetDebugFormatRoutine(format_callback_t FormatRoutine);
void sceDeci2SetDebugFlags(u32 DebugFlags);

#endif // ndef(IOP_DECI2LOG_H)
