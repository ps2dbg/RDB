/*++

  Module name:
	deci2.h

  Description:
	DECI2 Definitions.

--*/

#ifndef IOP_DECI2_H
#define IOP_DECI2_H

#include <types.h>
#include <irx.h>
#include <stdarg.h>

#include "sysclib.h"

#include "deci2log.h"
#include "deci2err.h"
#include "deci2pkt.h"


//
// Indexes for QueryBootMode/RegisterBootMode used by the DECI2 subsystem
//

// DECI manager present
// The header u32 contains the protocol version (2 = DECI2) in the lower
// 16 bits
#define BOOTMODE_ID_DECI					1

// EE boot parameters
// The header u32 is ignored. The following two u32 values contain ee_bootp
#define BOOTMODE_ID_EE_PARAMS				2

// IOP boot parameters
// The header u32 is ignored. The following two u32 values contain iop_bootp
#define BOOTMODE_ID_IOP_PARAMS				3



//
// DECI2 export table
//

#define deci2api_IMPORTS_start				DECLARE_IMPORT_TABLE(deci2api, 1, 1)
#define deci2api_IMPORTS_end				END_IMPORT_TABLE

#define I_sceDeci2Shutdown					DECLARE_IMPORT(2, sceDeci2Shutdown)
#define I_sceDeci2GetStatus					DECLARE_IMPORT(3, sceDeci2GetStatus)
#define I_sceDeci2Open						DECLARE_IMPORT(4, sceDeci2Open)
#define I_sceDeci2Close						DECLARE_IMPORT(5, sceDeci2Close)
#define I_sceDeci2ExRecv					DECLARE_IMPORT(6, sceDeci2ExRecv)
#define I_sceDeci2ExSend					DECLARE_IMPORT(7, sceDeci2ExSend)
#define I_sceDeci2ReqSend					DECLARE_IMPORT(8, sceDeci2ReqSend)
#define I_sceDeci2ExReqSend					DECLARE_IMPORT(9, sceDeci2ExReqSend)
#define I_sceDeci2ExLock					DECLARE_IMPORT(10, sceDeci2ExLock)
#define I_sceDeci2ExUnLock					DECLARE_IMPORT(11, sceDeci2ExUnLock)
#define I_sceDeci2ExPanic					DECLARE_IMPORT(12, sceDeci2ExPanic)
#define I_sceDeci2Poll						DECLARE_IMPORT(13, sceDeci2Poll)
#define I_sceDeci2ExPoll					DECLARE_IMPORT(14, sceDeci2ExPoll)
#define I_sceDeci2ExRecvSuspend				DECLARE_IMPORT(15, sceDeci2ExRecvSuspend)
#define I_sceDeci2ExRecvUnSuspend			DECLARE_IMPORT(16, sceDeci2ExRecvUnSuspend)
#define I_sceDeci2ExWakeupThread			DECLARE_IMPORT(17, sceDeci2ExWakeupThread)
#define I_sceDeci2ExSignalSema				DECLARE_IMPORT(18, sceDeci2ExSignalSema)
#define I_sceDeci2ExSetEventFlag			DECLARE_IMPORT(19, sceDeci2ExSetEventFlag)
#define I_sceDeci2AddDbgGroup				DECLARE_IMPORT(20, sceDeci2AddDbgGroup)
#define I_sceDeci2IfEventHandler			DECLARE_IMPORT(21, sceDeci2IfEventHandler)
#define I_sceDeci2IfCreate					DECLARE_IMPORT(22, sceDeci2IfCreate)
#define I_sceDeci2DbgPrintStatus			DECLARE_IMPORT(23, sceDeci2DbgPrintStatus)
#define I_sceDeci2SetPollCallback			DECLARE_IMPORT(24, sceDeci2SetPollCallback)

#define I_sceDeci2SetTifHandlers			DECLARE_IMPORT(25, sceDeci2SetTifHandlers)
#define I_sceDeci2GetTifHandlers			DECLARE_IMPORT(26, sceDeci2GetTifHandlers)
#define I_sceDeci2_27					DECLARE_IMPORT(27, sceDeci2_27)
#define I_sceDeci2_28					DECLARE_IMPORT(28, sceDeci2_28)


//
// DECI2 Manager Status
//

typedef void (*deci2_socket_handler_t)(int event, int param, void* opt);

typedef struct _sceDeci2Socket {
	//
	// Protocol handler
	//
	deci2_socket_handler_t					handler;				// 0x00

	//
	// Handler parameter
	//
	void*									opt;					// 0x04

	//
	// Protocol
	//
	int										proto;					// 0x08

	//
	// Flags
	//
	u32										Flags;					// 0x0C

	//
	// Target node of packet being sent
	//
	int										SendNode;				// 0x10

	//
	// Type of pending operation
	//
	int										PendingOperation;		// 0x14

	//
	// Target of pending operation.
	// This is a thread id for DECI2_PENDING_WAKEUP_THREAD, a semaphore id for
	// DECI2_PENDING_SIGNAL_SEMA, and an event flag id for
	// DECI2_PENDING_SET_EVENT_FLAG.
	//
	int										PendingTarget;			// 0x18

	//
	// Information required for completing the pending operation. This is the
	// number of calls to iWakeupThread/iSetEventFlag for DECI2_PENDING_WAKEUP_-
	// THREAD or DECI2_PENDING_SIGNAL_SEMA, respectively. For DECI2_PENDING_-
	// SET_EVENT_FLAG, its the bitmask.
	//
	int										PendingInformation;		// 0x1C

	//
	// Interface used to send the current packet
	//
	struct _sceDeci2Interface*				SendIf;					// 0x20

	//
	// Interface used to receive the current packet
	//
	struct _sceDeci2Interface*				RecvIf;					// 0x24
} sceDeci2Socket;

// No pending operation
#define DECI2_PENDING_NONE					0

// Wakeup thread. PendingTarget=tid, PendingInformation=count
#define DECI2_PENDING_WAKEUP_THREAD			1

// Signal semaphore. PendingTarget=semid, PendingInformation=count
#define DECI2_PENDING_SIGNAL_SEMA			2

// Set event flag. PendingTarget=evfid, PendingInformation=bitmask
#define DECI2_PENDING_SET_EVENT_FLAG		3


// Socket is implemented by DECI2 manager and receives DECI2Ex events.
#define DECI2_SOCK_FLG_INTERNAL				0x00000001

typedef int (*deci2_if_handler_t)(int event, void* opt, int param1, int param2);

typedef struct _sceDeci2Interface {
	//
	// Destination node
	//
	int										dest;					// 0x00

	//
	// Interface handler
	//
	deci2_if_handler_t						handler;				// 0x04

	//
	// Handler parameter
	//
	void*									opt;					// 0x08

	//
	// Interface flags
	//
	int										OutFlags;				// 0x0C

	//
	// Transmitting socket
	//
	sceDeci2Socket*							SendSocket;				// 0x10

	//
	// Total length of packet to send
	//
	int										TotalSendLength;		// 0x14

	//
	// Length of packet sent so far
	//
	int										SendLength;				// 0x18

	//
	// Send flags
	//
	int										InFlags;				// 0x1C

	//
	// Receiving socket
	//
	sceDeci2Socket*							RecvSocket;				// 0x20

	//
	// Total length of packet to receive
	//
	int										TotalRecvLength;		// 0x24

	//
	// Length of packet received so far
	//
	int										RecvLength;				// 0x28

	//
	// Pointer to received packet's header. This is the buffer passed in the
	// first call to sceDeci2ExRecv for the current packet.
	//
	struct _DECI2_HEADER*					RecvPacket;				// 0x2C
} sceDeci2Interface;

// Interface is active
#define DECI2_IF_FLG_ACTIVE					0x00000004

// DECI2_WRITE handler of sending socket is active
#define DECI2_IF_FLG_SOCKET_WRITE			0x00000008

#define DECI2_IF_OFLG_UP					0x00000001
#define DECI2_IF_OFLG_REACTIVATED			0x00000002
#define DECI2_IF_OFLG_CAN_SEND				0x00000008
#define DECI2_IF_OFLG_NEW_BIND				0x00000010

#define DECI2_IF_IFLG_REACTIVATED			0x00000002
#define DECI2_IF_IFLG_CAN_RECV				0x00000004

// Total number of socket descriptors. For compatibility with the SCEI DECI2
// manager, this constant should always be set to 35.
#define DECI2_MAX_SOCK						35

// Total number of interface descriptors. For compatibility with the SCEI
// DECI2 manager, this constant should always be set to 2.
// This also controls the number of pseudo-sockets (internal receivers) and
// interface poll handlers in the IOP's hot interrupt path. All changes to
// this constant should also be reflected in /iop/deci2/deci2/sdbhook.S.
#define DECI2_MAX_IF						2

// Socket numbers of first receiver and user socket. Socket 0 is DCMP,
// [1..DECI2_MAX_IF] are the interface-specific internal receivers (for packet
// relay and error messages). These constants should never be changed.
#define DECI2_FIRST_RECV_SOCKET				1
#define DECI2_FIRST_USER_SOCKET				(DECI2_MAX_IF + 1)

typedef int (*deci2_poll_callback_t)(void);

typedef struct _sceDeci2ManagerStatus {
	//
	// Identifier of socket that has locked the local node
	//
	int										LockSocket;				// 0x000

	//
	// Various status flags
	//
	int										StatusFlags;			// 0x004

	//
	// Initialization complete flag
	//
	int										InitComplete;			// 0x008

	//
	// Debug flag
	//
	int										DebugFlags;				// 0x00C

	//
	// Context parameters for sceDeci2ExPanic callback
	//
	print_callback_t						PrintRoutine;			// 0x010
	void*									PrintParam;				// 0x014

	//
	// Callback for sceDeci2Poll
	//
	void									(*PollCallback)(void);	// 0x018

	//
	// Interface which received the connect message from the host
	//
	sceDeci2Interface*						HostIfConnected;		// 0x01C

	//
	// Interface registered for the host route
	//
	sceDeci2Interface*						HostIf;					// 0x020

	//
	// Identifier of debugger socket
	//
	int										DebugSocket;			// 0x024

	//
	// Sockets of locally-registered protocols.
	//
	sceDeci2Socket							Socket[DECI2_MAX_SOCK];	// 0x028

	//
	// Interfaces.
	//
	sceDeci2Interface						Interface[DECI2_MAX_IF];// 0x5A0

	//
	// Pointer to array of interface-specific poll handlers.
	//
	deci2_poll_callback_t*					intrpoll;				// 0x600

	//
	// Optional format routine for sceDeci2ExPanic. If this pointer is set to
	// NULL, the DECI2 manager uses prnt instead.
	//
	format_callback_t						FormatRoutine;			// 0x604
} sceDeci2ManagerStatus;

// DCMP_CODE_LOCKED has been sent while the node was locked.
#define DECI2_STAT_FLG_LOCKERR				0x00000001

// At least one socket has a pending thread-related operation.
#define DECI2_STAT_FLG_PENDOP				0x00000002


// IFM-related events
#define DECI2_DEBUG_FLG_IFM					0x00000001

// IFM- and socket-related events
#define DECI2_DEBUG_FLG_IFM_SOCK			0x00000002

// PIF (up to 4 words)
#define DECI2_DEBUG_FLG_PIF_DATA_4			0x00000004

// PIF (up to 16 data words)
#define DECI2_DEBUG_FLG_PIF_DATA_16			0x00000008

// PIF (all data words)
#define DECI2_DEBUG_FLG_PIF_DATA_ALL		0x00000010

//#define DECI2_DEBUG_FLG_?					0x00000020
//#define DECI2_DEBUG_FLG_?					0x00000040
//#define DECI2_DEBUG_FLG_?					0x00000080

// PIF packet-related events
#define DECI2_DEBUG_FLG_PIF_PKT				0x00000100

// PIF DMA-related events
#define DECI2_DEBUG_FLG_PIF_DMA				0x00000200

// SIF2 packet-related events
#define DECI2_DEBUG_FLG_SIF2_PKT			0x00000400

// SIF2 DMA- and socket-related events
#define DECI2_DEBUG_FLG_SIF2_DMA			0x00000800

// DECI2 Interrupt start/end
#define DECI2_DEBUG_FLG_IRQ					0x00001000

// System Debugger-related events
#define DECI2_DEBUG_FLG_SDB					0x00002000


struct _sceDeci2ManagerStatus* sceDeci2GetStatus();


//
// DECI2 Manager Protocol Driver Definitions
//

// Standard Events. These are described in the official documentation and
// available to all protocol drivers.

// Packet data can be received. The protocol driver must invoke sceDeci2ExRecv
// to initiate a DMA operation. When the packet is read completely, the next
// event delivered will be DECI2_READDONE; otherwise, the protocol driver gets
// another DECI2_READ event once the next DMA operation can be started.
#define DECI2_READ			1		// param: length of input fragment

// The packet has been received completely.
#define DECI2_READDONE		2		// param: always 0 (actually: debug flags)

// Packet data can be sent. The protocol driver must invoke sceDeci2ExSend to
// initiate a DMA operation. When the packet has been transmitted completely,
// the protocol driver will get a DECI2_WRITEDONE event; otherwise, another
// DECI2_WRITE event signals to start the next DMA operation.
#define DECI2_WRITE			3		// param: always 0

// The packet has been transmitted completely.
#define DECI2_WRITEDONE		4		// param: always 0

// Notify protocol driver of inbound DCMP_TYPE_STATUS message. Besides
// DCMP_CODE_PROTO, all these messages are broadcast to all drivers.
#define DECI2_CHSTATUS		5

// Notify protocol driver of inbound DCMP_TYPE_ERROR message. 
#define DECI2_ERROR			6


// Extended Events. These are only sent to internal protocol sockets with a set
// DECI2_SOCK_FLG_INTERNAL. As there is no exported method to set this flag for
// user sockets, these events can normally only be received by protocols
// implemented by the manager itself.

// Called by sceDeci2IfEventHandler once an IFM_INDONE event is reported. Can
// be used to invoke IFD_RCV_OFF and prevent further receive operations for the
// same input packet, for example in case of insufficient buffer space in the
// driver (see the packet relay code for an example).
#define DECI2Ex_RflagDone	7		// param: length of input fragment

// Called by bind_socket_interface once the socket has been selected to send a
// packet via an interface. Can be used to call IFD_SEND_OFF to prevent write
// operations, in case of insufficient input data (see relay code).
#define DECI2Ex_WriteStart	8

// Called by sceDeci2IfEventHandler once an IFM_OUTDONE event is reported. This
// is similar to DECI2Ex_RflagDone described above, as it can be used to invoke
// IFD_SEND_OFF to prevent further transmit operations. The packet relay code
// uses this event if it needs to wait for more data from the inbound interface.
#define DECI2Ex_WflagDone	9		// param: length of output fragment

// Only called for the ISDBGP socket when a pending operation is started.
// Currently, this event is triggered only for thread wakeup, signal semaphore
// and set event flag operations from protocol drivers using the appropriate
// sceDeci2Ex... function.
#define DECI2Ex_PendStart	10		// (ISDBGP socket only)



int sceDeci2Open(unsigned short protocol, void* opt, deci2_socket_handler_t handler);
int sceDeci2Close(int s);
int sceDeci2ExRecv(int s, void* buf, unsigned short len);
int sceDeci2ExSend(int s, void* buf, unsigned short len);
int sceDeci2ReqSend(int s, char dest);
int sceDeci2ExReqSend(int s, char dest);
int sceDeci2ExLock(int s);
int sceDeci2ExUnLock(int s);
int sceDeci2ExPanic(const char* format, ...);
void sceDeci2Poll();
void sceDeci2ExPoll();
int sceDeci2ExRecvSuspend(int s);
int sceDeci2ExRecvUnSuspend(int s);
int sceDeci2ExWakeupThread(int s, int tid);
int sceDeci2ExSignalSema(int s, int semid);
int sceDeci2ExSetEventFlag(int s, int evfid, long bitmask);


//
// DECI2 Manager Interface Driver Definitions
//

sceDeci2Interface* sceDeci2IfCreate(char dest, void* opt, deci2_if_handler_t handler, deci2_poll_callback_t intrpoll);

#define IFD_RCV_START		0		//	-			-
#define IFD_RCV_READ		1		//	buf			len
#define IFD_RCV_END			2		//	-			-
#define IFD_SEND_START		3		//	protocol	node
#define IFD_SEND_WRITE		4		//	buf			len
#define IFD_SEND_END		5		//	-			-
#define IFD_POLL			6		//	-			-
#define IFD_RCV_OFF			7		//	-			-
#define IFD_RCV_ON			8		//	-			-
#define IFD_SEND_OFF		9		//	-			-
#define IFD_SEND_ON			10		//	-			-
#define IFD_DEBUG			11		//	flag		-
#define IFD_SHUTDOWN		12		//	-			-



void sceDeci2IfEventHandler(int event, sceDeci2Interface* If, int len, u16 protocol, char node);

#define IFM_IN				1		//	len			protocol	node
#define IFM_INDONE			2		//	len			-			-
#define IFM_OUT				3		//	-			-			-
#define IFM_OUTDONE			4		//	len			-			-
#define IFM_UP				5		//	-			-			-
#define IFM_DOWN			6		//	-			-			-


//
// DECI2 System Debugger Definitions
//

typedef struct _sceDbgpGroupEvent {
	void*									opt;
	int										len;
	DBGP_MSG_HEADER*						pkt;
	int										retlen;
	void*									retbuf;
} sceDbgpGroupEvent;

typedef void (*dbgp_group_callback)(sceDbgpGroupEvent* evt);

typedef struct _sceDbgpGroup {
	void*									opt;
	dbgp_group_callback						HeaderDone;
	dbgp_group_callback						Read;
	dbgp_group_callback						ReadDone;
	dbgp_group_callback						Write;
	dbgp_group_callback						WriteDone;
	dbgp_group_callback						ReqSend;
} sceDbgpGroup;

int sceDeci2AddDbgGroup(int group, sceDbgpGroup* dispatch);


//
// Miscellaneous calls
//

void sceDeci2DbgPrintStatus(print_callback_t PrintRoutine, void* param);
void* sceDeci2SetPollCallback(void (*PollCallback)(void));

//For the TIF.
struct Deci2_TIF_handlers{
	void *private;				//0x00
	int (*tif_open)(void *private);		//0x04
	int (*tif_close)(void *private);		//0x08
	int (*tif_send)(void *private, const void *buffer, unsigned int length);	//0x0C
	int (*tif_recv)(void *private, void *buffer, unsigned int length);		//0x10
	//Called by the DECI2 manager's pending event handler. Allows the TIF driver to update its own status outside of the weird DECI2 exception context.
	int (*tif_callback)(void *private);	//0x14
	//Returns true if the TIF has received a RESET packet.
	int (*tif_IsReset)(void *private);	//0x18
};

void sceDeci2SetTifHandlers(struct Deci2_TIF_handlers *TIFHandlers);
struct Deci2_TIF_handlers *sceDeci2GetTifHandlers(void);
int sceDeci2_27(void);
int sceDeci2_28(void);

#endif  // ndef(IOP_DECI2_H)
