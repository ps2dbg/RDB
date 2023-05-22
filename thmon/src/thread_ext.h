/* ThreadInfo.status */
/* #define	THS_RUN		0x01
#define THS_READY	0x02
#define THS_WAIT	0x04
#define THS_SUSPEND	0x08
#define THS_WAITSUSPEND	0x0c
#define THS_DORMANT	0x10 */
#define THS_DEAD 0x20

/* 17x4 = 68 */
typedef struct ThreadInfo
{
    unsigned int attr;
    unsigned int option;
    int status;
    void *entry;
    void *stack;
    int stackSize;
    void *gpReg;
    int initPriority;
    int currentPriority;
    int waitType;
    int waitId;
    int wakeupCount;
    unsigned long int *regContext;
    int reserved1;
    int reserved2;
    int reserved3;
    int reserved4;
} t_ThreadInfo;

struct ThbaseLinkedList
{
    struct ThbaseLinkedList *next;
    struct ThbaseLinkedList *prev;
};

#define LINKED_LIST_TAG_UNKNOWN 0x0000
#define LINKED_LIST_TAG_SYS     0x7F01
#define LINKED_LIST_TAG_SEMA    0x7F02
#define LINKED_LIST_TAG_THREAD  0x7F03
#define LINKED_LIST_TAG_MSGBX   0x7F04
#define LINKED_LIST_TAG_VPOOL   0x7F05
#define LINKED_LIST_TAG_FPOOL   0x7F06

/* 0x50 */
struct Thbase_TCB
{
    struct ThbaseLinkedList link; /* 0x00 - 0x04 */
    unsigned short int tag;       /* 0x08 */
    unsigned short int ThreadID;  /* 0x0A */
    unsigned char status;         /* 0x0C */
    unsigned char unknown4;
    unsigned short int priority; /* 0x0E */

    unsigned long int *regs; /* 0x10 */
    unsigned int unknown6;
    unsigned int unknown7;

    unsigned short int WaitType; /* 0x1C */
    unsigned short int unknown8;
    int WaitID;              /* 0x20 */
    struct Thbase_TCB *next; /* 0x24 */
    unsigned int unknown9;
    unsigned short int unknown10;
    unsigned short int init_priority; /* 0x2E */
    unsigned int clock_high;          /* 0x30 */
    unsigned int clock_low;           /* 0x34 */
    void *EntryPoint;                 /* 0x38 */
    void *stack;                      /* 0x3C */
    unsigned int StackSize;           /* 0x40 */
    void *gp;                         /* 0x44 */
    unsigned int attr;                /* 0x48 */
    unsigned int option;              /* 0x4C */
};

struct Thbase_AlarmInfo
{                                 /* 0x20 */
    struct ThbaseLinkedList link; /* 0x00 - 0x04 */
    unsigned short int tag;       /* 0x08 */
    unsigned short int AlarmID;   /* 0x0A */
    unsigned int unknown2;
    iop_sys_clock_t clock; /* 0x10 - 0x14: The time in absolute system clock ticks. */
    void *handler;         /* 0x18 */
    void *common;          /* 0x1C */
};

typedef struct SemaInfo
{
    unsigned int attr;
    unsigned int option;
    int initCount;
    int maxCount;
    int currentCount;
    int numWaitThreads;
    int reserved1;
    int reserved2;
} iop_SemaInfo_t;

/* Assumed to be 1220 bytes long - since THREADMAN erases 1220 bytes with memset(). */
struct Thbase_internal
{
    unsigned int unknown_1;                    /* 0x00 */
    unsigned int unknown_2;                    /* 0x04 */
    unsigned int ReadyMap[4];                  /* 0x08 to 0x14 - Ready status bits for all threads. */
    unsigned int ThreadmanAttr;                /* 0x18 */
    struct ThbaseLinkedList ReadyTCBList[128]; /* 0x1C to 0x418 */
    unsigned int unknown3_1;                   /* 0x41C */
    unsigned int unknown3_2;                   /* 0x420 */
    unsigned int unknown3_3;                   /* 0x424 */
    unsigned int unknown3_4;                   /* 0x428 */

    struct Thbase_TCB *LastTCB, *FirstTCB; /* 0x42C and 0x430 */
    struct ThbaseLinkedList SemaTCBList;   /* 0x434 */
    struct ThbaseLinkedList EventTCBList;  /* 0x43C */
    struct ThbaseLinkedList MsgbxTCBList;  /* 0x444 */
    struct ThbaseLinkedList VplTCBList;    /* 0x44C */
    struct ThbaseLinkedList FplTCBList;    /* 0x454 */

    struct ThbaseLinkedList AlarmTCBList;  /* 0x45C */
    struct ThbaseLinkedList AlarmTCBList2; /* 0x464 - not sure about the purpose of this field. */

    unsigned short int LastThreadID;    /* 0x46C */
    unsigned short int LastSemaID;      /* 0x46E */
    unsigned short int LastEventFlagID; /* 0x470 */
    unsigned short int LastMbxID;       /* 0x472 */
    unsigned int padding;
    unsigned short int LastAlarmID; /* 0x478 */
    unsigned short int unknown4;
    unsigned int unknown5;                   /* 0x47C */
    struct ThbaseLinkedList SleepTCBList;    /* 0x480 */
    struct ThbaseLinkedList DelayTCBList;    /* 0x488 */
    struct ThbaseLinkedList TCBList;         /* 0x490 */
    struct ThbaseLinkedList UnknownTCBList;  /* 0x498 */
    struct ThbaseLinkedList UnknownTCBList2; /* 0x450 */

    void *ThbaseHeap;              /* 0x454 */
    unsigned int SystemStatusFlag; /* 0x458 */

    /* There are probably more fields after here. */
};


void *GetThbaseInternalStruct(void);
#define I_GetThbaseInternalStruct DECLARE_IMPORT(3, GetThbaseInternalStruct)
