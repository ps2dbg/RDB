/*++

  Module name:
	deci2pkt.h

  Description:
	DECI2 Packet Definitions.

--*/

#ifndef DECI2PKT_H
#define DECI2PKT_H

//
// General DECI2 Packet header
//

typedef struct _DECI2_HEADER {
	unsigned short							len;
	unsigned short							unused;
	unsigned short							proto;
	char									src;
	char									dest;
} DECI2_HEADER;


#define DECI2_PROTO_DCMP					0x0001
#define DECI2_PROTO_I0TTYP					0x0110
#define DECI2_PROTO_I1TTYP					0x0111
#define DECI2_PROTO_I2TTYP					0x0112
#define DECI2_PROTO_I3TTYP					0x0113
#define DECI2_PROTO_I4TTYP					0x0114
#define DECI2_PROTO_I5TTYP					0x0115
#define DECI2_PROTO_I6TTYP					0x0116
#define DECI2_PROTO_I7TTYP					0x0117
#define DECI2_PROTO_I8TTYP					0x0118
#define DECI2_PROTO_I9TTYP					0x0119
#define DECI2_PROTO_IKTTYP					0x011F
#define DECI2_PROTO_DRFP0					0x0120
#define DECI2_PROTO_ISDBGP					0x0130
#define DECI2_PROTO_ILOADP					0x0150
#define DECI2_PROTO_E0TTYP					0x0210
#define DECI2_PROTO_NETMP					0x0400


//
// DCMP
//

typedef struct _DCMP_HEADER {
	char									type;
	char									code;
	unsigned short							extra;
} DCMP_HEADER;

typedef struct _DCMP_MSG_HEADER {
	DECI2_HEADER							deci2;
	DCMP_HEADER								dcmp;
} DCMP_MSG_HEADER;

typedef struct _DCMP_CONNECT_MSG {
	DECI2_HEADER							deci2;
	DCMP_HEADER								dcmp;
	u8										result;
	u8										unused[3];
	u32										ee_bootp[2];
	u32										iop_bootp[2];
} DCMP_CONNECT_MSG;

typedef struct _DCMP_CONNECTR_MSG {
	DECI2_HEADER							deci2;
	DCMP_HEADER								dcmp;
	u8										result;
	u8										unused[3];
} DCMP_CONNECTR_MSG;

typedef struct _DCMP_STATUS_MSG {
	DECI2_HEADER							deci2;
	DCMP_HEADER								dcmp;
	u32										proto_node;
} DCMP_STATUS_MSG;

typedef struct _DCMP_ERROR_MSG {
	DECI2_HEADER							deci2;
	DCMP_HEADER								dcmp;
	DECI2_HEADER							errhdr;
	u8										data[16];
} DCMP_ERROR_MSG;

typedef struct _DCMP_MSG {
	DECI2_HEADER							deci2;
	DCMP_HEADER								dcmp;
	union {
		struct {
			u8								result;
			u8								unused[3];
			u32								ee_bootp[2];
			u32								iop_bootp[2];
		} connect;
		struct {
			u16								id;
			u16								seq;
			u8								data[32];
		} echo;
		struct {
			int								proto_node;
		} status;
		struct {
			DECI2_HEADER					hdr;
			u8								data[16];
		} error;
	};
} DCMP_MSG;

#define DCMP_ERR_INVALDEST					1
#define DCMP_ERR_ALREADYCONN				2
#define DCMP_ERR_NOTCONNECT					3

#define DCMP_TYPE_CONNECT					0
#define DCMP_CODE_CONNECT					0	// connect request
#define DCMP_CODE_CONNECTR					1	// connect reply
#define DCMP_CODE_DISCONNECT				2	// disconnect request
#define DCMP_CODE_DISCONNECTR				3	// disconnect reply

#define DCMP_TYPE_ECHO						1
#define DCMP_CODE_ECHO						0	// echo
#define DCMP_CODE_ECHOR						1	// echo reply

#define DCMP_TYPE_STATUS					2
#define DCMP_CODE_CONNECTED					0	// !NOCONNECT
#define DCMP_CODE_PROTO						1	// !NOPROTO
#define DCMP_CODE_UNLOCKED					2	// !LOCKED
#define DCMP_CODE_SPACE						3	// !NOSPACE
#define DCMP_CODE_ROUTE						4	// !NOROUTE

#define DCMP_TYPE_ERROR						3
#define DCMP_CODE_NOROUTE					0	// no route to node
#define DCMP_CODE_NOPROTO					1	// protocol unreachable
#define DCMP_CODE_LOCKED					2	// locked
#define DCMP_CODE_NOSPACE					3	// deci2 manager/dsnetm buffer full
#define DCMP_CODE_INVALHEAD					4	// invalid header
#define DCMP_CODE_NOCONNECT					5	// not connected

//Special case for the TDB startup card. Both the source and destination nodes are set to 'E'.
#define DCMP_TYPE_RESET						4
#define DCMP_CODE_RESET						0	// RESET

//
// DBGP
//

typedef struct _DBGP_HEADER {
	u8										id;
	u8										group;
	u8										type;
	u8										code;
	u8										result;
	u8										count;
	u16										unused;
} DBGP_HEADER;

typedef struct _DBGP_MSG_HEADER {
	DECI2_HEADER							deci2;
	DBGP_HEADER								dbgp;
} DBGP_MSG_HEADER;

#define DBGP_GROUP_ENTIRE_SYSTEM			0

#define DBGP_CODE_CONT						0x00
#define DBGP_CODE_STEP						0x01
#define DBGP_CODE_NEXT						0x02
#define DBGP_CODE_OTHER						0xFF

#define DBGP_TYPE_GETCONF					0x00
#define DBGP_TYPE_GETCONFR					0x01
#define DBGP_TYPE_GETREG					0x04
#define DBGP_TYPE_GETREGR					0x05
#define DBGP_TYPE_PUTREG					0x06
#define DBGP_TYPE_PUTREGR					0x07
#define DBGP_TYPE_RDMEM						0x08
#define DBGP_TYPE_RDMEMR					0x09
#define DBGP_TYPE_WRMEM						0x0A
#define DBGP_TYPE_WRMEMR					0x0B
#define DBGP_TYPE_GETBRKPT					0x10
#define DBGP_TYPE_GETBRKPTR					0x11
#define DBGP_TYPE_PUTBRKPT					0x12
#define DBGP_TYPE_PUTBRKPTR					0x13
#define DBGP_TYPE_BREAK						0x14
#define DBGP_TYPE_BREAKR					0x15
#define DBGP_TYPE_CONTINUE					0x16
#define DBGP_TYPE_CONTINUER					0x17
#define DBGP_TYPE_RUN						0x18
#define DBGP_TYPE_RUNR						0x19
#define DBGP_TYPE_XGKTCTL					0x20
#define DBGP_TYPE_XGKTCTLR					0x21
#define DBGP_TYPE_XGKTDATAR					0x23
#define DBGP_TYPE_DBGCTL					0x24
#define DBGP_TYPE_DBGCTLR					0x25
#define DBGP_TYPE_RDIMG						0x28
#define DBGP_TYPE_RDIMGR					0x29
#define DBGP_TYPE_SETBPFUNC					0x2E
#define DBGP_TYPE_SETBPFUNCR				0x2F

//Alternate (LOADP) commands, for use when debugging.
enum DBGP_MODULE_TYPE{
	DBGP_MODULE_TYPE_LIST	= 0x40,
	DBGP_MODULE_TYPE_LISTR,
	DBGP_MODULE_TYPE_INFO,
	DBGP_MODULE_TYPE_INFOR,
	DBGP_MODULE_TYPE_MEMLIST,
	DBGP_MODULE_TYPE_MEMLISTR
};

#define DBGP_RESULT_GOOD					0x00
#define DBGP_RESULT_INVALREQ				0x01
#define DBGP_RESULT_UNIMPREQ				0x02
#define DBGP_RESULT_ERROR					0x03
#define DBGP_RESULT_INVALCONT				0x04
#define DBGP_RESULT_TLBERR					0x10
#define DBGP_RESULT_ADRERR					0x11
#define DBGP_RESULT_BUSERR					0x12
#define DBGP_RESULT_INVALSTATE				0x20
#define DBGP_RESULT_BREAKED					0x21
#define DBGP_RESULT_BRKPT					0x22
#define DBGP_RESULT_STEPNEXT				0x23
#define DBGP_RESULT_EXCEPTION				0x24
#define DBGP_RESULT_PROGEND					0x25
#define DBGP_RESULT_BUSYSTATE				0x26
#define DBGP_RESULT_DEBUG_EXCEPTION			0x27
#define DBGP_RESULT_TIMEOUT					0x28
#define DBGP_RESULT_NOMOD				0x40

#define DBGP_CPUID_CPU	0
#define DBGP_CPUID_VU0	1	//ESDBGP only
#define DBGP_CPUID_VU1	1	//ESDBGP only

#define DBGP_GROUP_ENTIRE_SYSTEM	0	//Entire system
#define DBGP_GROUP_EE_THREAD		1
#define DBGP_GROUP_EE_MODULE		2
#define DBGP_GROUP_IOP_THREAD		1	//Reserved
#define DBGP_GROUP_IOP_MODULE		2

typedef struct _DBGP_CONF_DATA {
	u32										major_ver;
	u32										minor_ver;
	u32										target_id;
	u32										reserved1;
	u32										mem_align;
	u32										reserved2;
	u32										reg_size;
	u32										nreg;
	u32										nbrkpt;
	u32										ncont;
	u32										nstep;
	u32										nnext;
	u32										mem_limit_align;
	u32										mem_limit_size;
	u32										run_stop_state;
	u32										hdbg_area_addr;
	u32										hdbg_area_size;
} DBGP_CONF_DATA;

#define DBGP_CF_RSS_RUNNING					1
#define DBGP_CF_RSS_STOPPED					2

typedef struct _DBGP_MEM {
	u8										space;
	u8										align;
	u16										reserved;
	u32										address;
	u32										length;
} DBGP_MEM;

typedef struct _DBGP_MEMMSG_HEADER {
	DECI2_HEADER							deci2;
	DBGP_HEADER								dbgp;
	DBGP_MEM								mem;
} DBGP_MEMMSG_HEADER;

typedef struct _DBGP_REG {
	u8										kind;
	u8										num;
	u16										reserved;
} DBGP_REG;

#define ISDBGP_KIND_HL						1 // hi,lo
	#define ISDBGP_NUM_LO						0
	#define ISDBGP_NUM_HI						1
#define ISDBGP_KIND_GPR						2 // general purpose register
#define ISDBGP_KIND_SCC						3 // system control register
#define ISDBGP_KIND_GTER					6 // GTE register
#define ISDBGP_KIND_GTEC					7 // GTE control register

//
// LOADP
//

typedef struct _LOADP_HEADER {
	u8										cmd;
	u8										action;
	u8										result;
	u8										stamp;
	u32										module_id;
} LOADP_HEADER;

enum LOADP_CMD{
	LOADP_CMD_START		= 0,
	LOADP_CMD_STARTR,
	LOADP_CMD_REMOVE,
	LOADP_CMD_REMOVER,
	LOADP_CMD_LIST,
	LOADP_CMD_LISTR,
	LOADP_CMD_INFO,
	LOADP_CMD_INFOR,
	LOADP_CMD_WATCH,
	LOADP_CMD_WATCHR,
	LOADP_CMD_MEMLIST,
	LOADP_CMD_MEMLISTR,

	LOADP_CMD_REPORT	= 16
};

typedef struct _LOADP_MOD_INFO {
	u16										version;
	u16										flags;
	u32										module_top_addr;
	u32										text_size;
	u32										data_size;
	u32										bss_size;
	u32										entry;
	u32										gp;
	u8										extnumwords;
	u8										ext_type;
	u16										reserved;
} LOADP_MOD_INFO;	//Followed by extnumwords words and the module name string.

enum LOADP_MF_STAT{	//Bits 0-3 of flags.
	LOADP_MF_STAT_LOADED	= 1,
	LOADP_MF_STAT_STARTING,
	LOADP_MF_STAT_STOPPING,
	LOADP_MF_STAT_SELF_STOPPING,
	LOADP_MF_STAT_STOPPED,
	LOADP_MF_STAT_SELF_STOPPED,
};

//Other flag bits.
#define LOADP_MF_REMOVABLE	0x08
#define LOADP_MF_NO_FREE	0x10
#define LOADP_MF_CLEAR_MEM	0x20

//
// NETMP
//

typedef struct _NETMP_HEADER {
	u8								code;
	u8								result;
} NETMP_HEADER;

enum NETMP_CODE{
	NETMP_CODE_CONNECT	= 0,
	NETMP_CODE_CONNECTR,
	NETMP_CODE_RESET,
	NETMP_CODE_RESETR,
	NETMP_CODE_MESSAGE,
	NETMP_CODE_MESSAGER,
	NETMP_CODE_STATUS,
	NETMP_CODE_STATUSR,
	NETMP_CODE_KILL,
	NETMP_CODE_KILLR,
	NETMP_CODE_VERSION,
	NETMP_CODE_VERSIONR,
};

#endif  // ndef(DECI2PKT_H)
