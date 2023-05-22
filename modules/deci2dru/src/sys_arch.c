/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#include <types.h>
#include <stdio.h>
#include <sysmem.h>
#include <thbase.h>
#include <sysclib.h>
#include <intrman.h>
#include "intrman_add.h"
#include <timrman.h>

#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/debug.h"
#include "lwip/timeouts.h"
#include "lwip/pbuf.h"
#include "arch/sys_arch.h"

#include "ps2ip_internal.h"

extern int PS2IPTimerID;

void *malloc(size_t size)
{
    void *ptr;

    ptr = AllocSysMemory(ALLOC_LAST, size, NULL);

    return ptr;
}

void free(void *ptr)
{
    FreeSysMemory(ptr);
}

void *calloc(size_t n, size_t size)
{
    void *ptr;

    ptr = AllocSysMemory(ALLOC_LAST, n * size, NULL);

    if (ptr != NULL)
        memset(ptr, 0, n * size);

    return ptr;
}

int PS2IPTimerID, PS2IPTimerIntrNum;
unsigned int PS2IPTimerTicks;

static unsigned int OldTimerCounter;

#define PS2IP_COUNTER_COMPARE 1440 // Use intervals of 10msecs to reduce the strain on the IOP.

void ps2ip_InitTimer(void)
{
    /*
        Allocate and configure timer here.

        source = TC_SYSCLOCK (1)
        size = TIMER_SIZE_32 (32)
        prescale = 1/256 (256) (where 36864000/256=144 ticks per msec)

        Mode bits:
            Value	Bit	Description
            0x0001	1	Enable gate
            0x0002	2	Gate count mode
            0x0004	3	Gate count mode
            0x0008	4	Reset on target
            0x0010	5	Interrupt on target
            0x0020	6	Interrupt on overflow
            0x0040	7	??? (Repeat?)
            0x0080	8	??? (Clears interrupt bit on assertion?)
            0x0100	9	"uses hblank on counters 1 and 3, and PSXCLOCK on counter 0"
            0x0200	10	1/8 prescale (counters 0-2 only)
            0x0400	11	Interrupt	(status)
            0x0800	12	Target		(status)
            0x1000	13	Overflow	(status)
            0x2000	14	1/8 prescale (counters 3-5 only)
            0x4000	15	1/16 prescale
            0x6000	14+15	1/256 prescale

    Sources:
        TC_SYSCLOCK	1	36.864MHz
        TC_PIXEL	2	13.5MHz	(Regardless of the screen mode)
        TC_HLINE	4	NTSC 15.73426573KHz (858 pixel clock), PAL 15.625KHz (864 pixel clock)
        TC_HOLD		8	??? (The FPS2BIOS code has this value too, but it isn't documented by Sony)

    Available timers:
        Name	Source(s)		Gate signal	Width	Prescale	Notes
        ----------------------------------------------------------------------------------------
        RTC0	SYSCLOCK|PIXEL|HOLD	H-blank		16	1		In use by PADMAN
        RTC1	SYSCLOCK|HLINE|HOLD	V-blank		16	1		In use by PADMAN
        RTC2	SYSCLOCK		None		16	8
        RTC3	SYSCLOCK|HLINE		V-blank		32	1
        RTC4	SYSCLOCK		None		32	256
        RTC5	SYSCLOCK		None		32	256
*/

    PS2IPTimerTicks = 0;
    PS2IPTimerID = AllocHardTimer(1, 32, 256);
    SetTimerMode(PS2IPTimerID, 0);
    SetTimerCounter(PS2IPTimerID, 0);
    OldTimerCounter = 0;
    SetTimerCompare(PS2IPTimerID, PS2IP_COUNTER_COMPARE);
    SetTimerMode(PS2IPTimerID, 0x6058);
    PS2IPTimerIntrNum = GetHardTimerIntrCode(PS2IPTimerID);
    DisableDispatchIntr(PS2IPTimerIntrNum);
    EnableIntr(PS2IPTimerIntrNum);
}

u32_t sys_now(void)
{
    unsigned int TicksElasped, CurrentTimerCounter;

    CurrentTimerCounter = GetTimerCounter(PS2IPTimerID);
    if (CurrentTimerCounter < OldTimerCounter) {
        TicksElasped = PS2IP_COUNTER_COMPARE - OldTimerCounter + CurrentTimerCounter;
    } else
        TicksElasped = CurrentTimerCounter - OldTimerCounter;

    OldTimerCounter = CurrentTimerCounter;

    PS2IPTimerTicks += TicksElasped;

    return PS2IPTimerTicks * 10;
}
