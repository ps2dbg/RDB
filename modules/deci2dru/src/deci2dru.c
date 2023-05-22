/*	DECI2DRU.IRX
	DECI2 Universal InterFace (UIF) driver v1.3 */
#include <deci2.h>
#include <intrman.h>
#include "intrman_add.h"
#include <loadcore.h>
#include <modload.h>
#include <sifman.h>
#include <sysmem.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <timrman.h>

#include <dev9regs.h>
#include <speedregs.h>
#include <smapregs.h>

#include "lwip/sys.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/prot/dhcp.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/inet.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "lwip/pbuf.h"

#include "arch/sys_arch.h"

#include "smap_internal.h"
#include "ps2ip_internal.h"
#include "UIFConfig.h"

#define MODNAME "Deci2_UIF_interface_driver"
IRX_ID(MODNAME, 1, 4);

struct Deci2_UIF_TR_data{
	unsigned short int WrPtr;
	unsigned short int RdPtr;
	unsigned short int BufferLevel;
	unsigned char buffer[0x4000];	//A ring buffer.
};

struct Deci2_UIF_data{
	sceDeci2Interface *interface;
//	int IKTTYPSocket;
	unsigned int DebugFlags;
	unsigned short int flags;
	unsigned short int activity;
	struct Deci2_UIF_TR_data TxData;
	struct Deci2_UIF_TR_data RxData;
	DECI2_HEADER IncomingDECI2Header;
	unsigned short int BytesCopied;
	unsigned short int RecvLength;
	unsigned short int SendLength;
	unsigned int RemainingDataInPbuf;
	struct pbuf *LastIncompletePbuf;

	struct tcp_pcb *uif_svr_pcb, *uif_pcb;
	int RebootThreadID;
};

//Interface flags.
#define IF_SEND_PENDING		0x0001
#define IF_SEND_DISABLED	0x0002
#define IF_SEND_ENABLED		0x0004
#define IF_CAN_SEND		0x0010
#define IF_SEND_COMPLETE	0x0020

#define IF_RECV_PENDING		0x0100
#define IF_RECV_DISABLED	0x0200
#define IF_RECV_ENABLED		0x0400
#define IF_CAN_RECV		0x1000
#define IF_RECV_COMPLETE	0x2000

#define IF_MAIN_RESET		0x4000

static struct Deci2_UIF_data UIFData;

int HostIFActivityPollCallback(void);

//Internal functions.
static int tif_open(void *private);
static int tif_close(void *private);
static int tif_send(void *private, const void *buffer, unsigned int length);
static int tif_recv(void *private, void *buffer, unsigned int length);
static int tif_callback(void *private);
static int tif_IsReset(void *private);
static int ReadData(struct Deci2_UIF_TR_data *pData, void *buffer, int length, int mode);

static char RebootPath[12];

//Unlike the TIF driver, most of the callbacks will not be used. Only the callback and IsReset will matter.
static struct Deci2_TIF_handlers TIFHandlers={
	&UIFData,
	&tif_open,
	&tif_close,
	&tif_send,
	&tif_recv,
	&tif_callback,
	&tif_IsReset
};

static int tif_callback(void *private){
	//The TIF used this function to check on interface activity, but this is now used to handle reset requests.
	if(tif_IsReset(private))
		iWakeupThread(((struct Deci2_UIF_data*)private)->RebootThreadID);

	return 0;
}

static int SignalUIFActivity(struct Deci2_UIF_data *pUIFData, unsigned int bits){
	pUIFData->activity=1;
	//This signals the code running within the DECI2 context of the activity, but this driver's design does not deal with multiple contexts (i.e. threaded and interrupt) anymore.
//	if(pUIFData->flags&bits) sceDeci2_28();

	return 0;
}

static int ReadData(struct Deci2_UIF_TR_data *pData, void *buffer, int length, int mode){
	int CopyLength, CopyLength2;

	CopyLength=(length<pData->BufferLevel)?length:pData->BufferLevel;
	if(0x4000-(pData->RdPtr&0x3FFF)<CopyLength){
		CopyLength=0x4000-(pData->RdPtr&0x3FFF);
	}

	bcopy(pData->buffer+(pData->RdPtr&0x3FFF), buffer, CopyLength);

	if((CopyLength2=length-CopyLength)>0){
		//If the ring buffer wrapped around, copy data from the other end if there is data there.
		if(pData->BufferLevel-CopyLength>0){
			if(pData->BufferLevel-CopyLength<CopyLength2) CopyLength2=pData->BufferLevel-CopyLength;

			bcopy((const void*)((unsigned char*)pData->buffer+((pData->RdPtr+CopyLength)&0x3FFF)), (void*)((unsigned char*)buffer+CopyLength), CopyLength2);
		}
		else CopyLength2=0;
	}
	else CopyLength2=0;

	if(mode>=0){
		pData->RdPtr+=(CopyLength+CopyLength2);
		pData->BufferLevel-=(CopyLength+CopyLength2);
	}

	return(CopyLength+CopyLength2);
}

static int WriteData(struct Deci2_UIF_TR_data *pData, const void *buffer, int length, int mode){
	int CopyLength, SpaceAvailable;

	SpaceAvailable=0x4000-pData->BufferLevel;
	CopyLength=(length<SpaceAvailable)?length:SpaceAvailable;
	if(0x4000-(pData->WrPtr&0x3FFF)<CopyLength){	//Amount of space available before the end of the buffer.
		CopyLength=0x4000-(pData->WrPtr&0x3FFF);
	}

	bcopy(buffer, pData->buffer+(pData->WrPtr&0x3FFF), CopyLength);

	if((length=length-CopyLength)>0){
		//If the ring buffer wrapped around, copy data to the other end if there is space.
		if((SpaceAvailable=0x4000-CopyLength-pData->BufferLevel)>0){
			if(SpaceAvailable<length) length=SpaceAvailable;

			bcopy((const void*)((unsigned char*)buffer+CopyLength), (void*)((unsigned char*)pData->buffer+((pData->WrPtr+CopyLength)&0x3FFF)), length);
		}
		else length=0;
	}
	else length=0;

	if(mode>=0){
		pData->WrPtr+=(CopyLength+length);
		pData->BufferLevel+=(CopyLength+length);
	}

	return(CopyLength+length);
}

static void HandleSendEvent(struct Deci2_UIF_data *pUIFData){
	if(pUIFData->flags&IF_SEND_PENDING){
		if(pUIFData->RxData.BufferLevel<0x4000){
			pUIFData->flags=(pUIFData->flags|IF_CAN_SEND)&~IF_SEND_ENABLED;
		}
		else{
			pUIFData->flags|=IF_SEND_ENABLED;
		}
	}
}

static void HandleRecvEvent(struct Deci2_UIF_data *pUIFData){
	struct Deci2_UIF_TR_data *pTRData;

	if(pUIFData->TxData.BufferLevel>0){
		pTRData=&pUIFData->TxData;
		pUIFData->flags=(pUIFData->flags&~IF_RECV_ENABLED)|IF_CAN_RECV;
		if(pUIFData->IncomingDECI2Header.len==0){
			if(pTRData->BufferLevel<8){
				pUIFData->flags&=~IF_CAN_RECV;
			}
			else{
				ReadData(&pUIFData->TxData, &pUIFData->IncomingDECI2Header, sizeof(pUIFData->IncomingDECI2Header), -1);
			}
		}
	}
}

extern int PS2IPTimerIntrNum;

static int HandleIntrRegs(void){
	USE_SPD_REGS;
	USE_SMAP_REGS;
	USE_SMAP_EMAC3_REGS;
	unsigned short int result;

	result=SPD_REG16(SPD_R_INTR_STAT)&(DEV9_SMAP_INTR_MASK);
	if(result&SMAP_INTR_EMAC3){
		SMAP_REG16(SMAP_R_INTR_CLR)=SMAP_INTR_EMAC3;
		SMAP_EMAC3_SET32(SMAP_R_EMAC3_INTR_STAT, SMAP_E3_INTR_TX_ERR_0|SMAP_E3_INTR_SQE_ERR_0|SMAP_E3_INTR_DEAD_0);
	}
	SMAP_REG16(SMAP_R_INTR_CLR)=result;
	*(volatile u32*)0xBF801070 = (~((1<<IOP_IRQ_DEV9) | (1<<PS2IPTimerIntrNum)) & *(volatile u32*)0xBF801074);

	return result;
}

static int UIF_SendData(void){
	int MaxLen, AmountToSend, result;
	static unsigned char buffer[1540];

	AmountToSend=0;
	if(UIFData.uif_pcb!=NULL){
		if((MaxLen=tcp_sndbuf(UIFData.uif_pcb))>0){
			if(MaxLen>sizeof(buffer)) MaxLen=sizeof(buffer);

			if((AmountToSend=tif_recv(&UIFData, buffer, MaxLen))>0){
				//Carry on, if there is more data to send.
				do{
					result=tcp_write(UIFData.uif_pcb, buffer, AmountToSend, (AmountToSend<MaxLen?0:TCP_WRITE_FLAG_MORE)|TCP_WRITE_FLAG_COPY);
					tcp_output(UIFData.uif_pcb);
				}while(result!=ERR_OK);
			}
		}
	}

	return AmountToSend;
}

static int HostIFHandler(int event, void* opt, int param1, int param2){
	int result, ContinueFlag, CopyLen;
	static const char *FunctionNames[]={
		"RCV_START",
		"RCV_READ",
		"RCV_END",
		"SEND_START",
		"SEND_WRITE",
		"SEND_END",
		"POLL",
		"RCV_OFF",
		"RCV_ON",
		"SEND_OFF",
		"SEND_ON",
		"DEBUG"
	};

	result=0;	//The original does not seem to have this variable initialized.
	if((((struct Deci2_UIF_data*)opt)->DebugFlags&0x200) && event!=IFD_POLL){
		sceDeci2ExPanic("\ttif ifc func %s flag=%x p1=%x p2=%x\n", FunctionNames[event], ((struct Deci2_UIF_data*)opt)->flags, param1, param2);
	}

	switch(event){
		case IFD_RCV_START:
			((struct Deci2_UIF_data*)opt)->flags|=IF_RECV_PENDING;
			((struct Deci2_UIF_data*)opt)->BytesCopied=0;
			result=0;
			break;
		case IFD_RCV_READ:
			CopyLen=((struct Deci2_UIF_data*)opt)->IncomingDECI2Header.len-((struct Deci2_UIF_data*)opt)->BytesCopied;
			if(param2<CopyLen) CopyLen=param2;

			if((result=ReadData(&((struct Deci2_UIF_data*)opt)->TxData, (void*)param1, CopyLen, 0))>0){
				((struct Deci2_UIF_data*)opt)->RecvLength=result;
				((struct Deci2_UIF_data*)opt)->BytesCopied+=result;
				((struct Deci2_UIF_data*)opt)->flags|=IF_RECV_COMPLETE;
				HandleRecvEvent(opt);
			}

			break;
		case IFD_RCV_END:
			((struct Deci2_UIF_data*)opt)->IncomingDECI2Header.len=0;
			((struct Deci2_UIF_data*)opt)->flags&=~(IF_CAN_RECV|IF_RECV_PENDING);
			if(!(((struct Deci2_UIF_data*)opt)->flags&IF_RECV_DISABLED)){
				((struct Deci2_UIF_data*)opt)->flags|=IF_RECV_ENABLED;
				HandleRecvEvent(opt);
			}
			result=0;
			break;
		case IFD_SEND_START:
			((struct Deci2_UIF_data*)opt)->flags|=IF_SEND_PENDING;
			HandleSendEvent(opt);
			result=0;
			break;
		case IFD_SEND_WRITE:
			SignalUIFActivity(opt, IF_SEND_ENABLED);

			if((result=WriteData(&((struct Deci2_UIF_data*)opt)->RxData, (const void*)param1, param2, 0))>0){
				while(UIF_SendData()>0){};

				((struct Deci2_UIF_data*)opt)->SendLength=result;
				((struct Deci2_UIF_data*)opt)->flags|=IF_SEND_COMPLETE;
			}

			break;
		case IFD_SEND_END:
			((struct Deci2_UIF_data*)opt)->flags&=~(IF_CAN_SEND|IF_SEND_ENABLED|IF_SEND_PENDING);
			result=0;
			break;
		case IFD_POLL:
			HandleIntrRegs();
			SMAPHandleIncomingPackets();

			do{
				sys_check_timeouts();

				if(((struct Deci2_UIF_data*)opt)->activity){
					((struct Deci2_UIF_data*)opt)->activity=0;
					HandleSendEvent(opt);
					HandleRecvEvent(opt);
				}

				do{
					ContinueFlag=0;
					if(((struct Deci2_UIF_data*)opt)->flags&IF_SEND_COMPLETE){
						((struct Deci2_UIF_data*)opt)->flags&=~IF_SEND_COMPLETE;
						sceDeci2IfEventHandler(IFM_OUTDONE, ((struct Deci2_UIF_data*)opt)->interface, ((struct Deci2_UIF_data*)opt)->SendLength, 0, 0);
						ContinueFlag=1;

						if(((struct Deci2_UIF_data*)opt)->flags&IF_SEND_PENDING){
							HandleSendEvent(opt);
						}
					}

					if((((struct Deci2_UIF_data*)opt)->flags&(IF_CAN_SEND|IF_SEND_DISABLED|IF_SEND_PENDING))==(IF_CAN_SEND|IF_SEND_PENDING)){
						((struct Deci2_UIF_data*)opt)->flags&=~IF_CAN_SEND;
						ContinueFlag=1;
						sceDeci2IfEventHandler(IFM_OUT, ((struct Deci2_UIF_data*)opt)->interface, 0, 0, 0);
					}

					if(((struct Deci2_UIF_data*)opt)->flags&IF_RECV_COMPLETE){
						((struct Deci2_UIF_data*)opt)->flags&=~IF_RECV_COMPLETE;
						sceDeci2IfEventHandler(IFM_INDONE, ((struct Deci2_UIF_data*)opt)->interface, ((struct Deci2_UIF_data*)opt)->RecvLength, 0, 0);
						ContinueFlag=1;
						((struct Deci2_UIF_data*)opt)->flags|=IF_RECV_ENABLED;
					}

					if((((struct Deci2_UIF_data*)opt)->flags&(IF_CAN_RECV|IF_RECV_DISABLED))==IF_CAN_RECV){
						((struct Deci2_UIF_data*)opt)->flags&=~IF_CAN_RECV;
						ContinueFlag=1;
						sceDeci2IfEventHandler(IFM_IN, ((struct Deci2_UIF_data*)opt)->interface, ((struct Deci2_UIF_data*)opt)->TxData.BufferLevel<((struct Deci2_UIF_data*)opt)->IncomingDECI2Header.len?((struct Deci2_UIF_data*)opt)->TxData.BufferLevel:((struct Deci2_UIF_data*)opt)->IncomingDECI2Header.len, ((struct Deci2_UIF_data*)opt)->IncomingDECI2Header.proto, ((struct Deci2_UIF_data*)opt)->IncomingDECI2Header.dest);
					}
				}while(ContinueFlag);
			}while(SMAPHandleIncomingPackets());

			result=0;
			break;
		case IFD_RCV_OFF:
			((struct Deci2_UIF_data*)opt)->flags=(((struct Deci2_UIF_data*)opt)->flags|IF_RECV_DISABLED)&~IF_RECV_ENABLED;
			result=0;
			break;
		case IFD_RCV_ON:
			((struct Deci2_UIF_data*)opt)->flags=(((struct Deci2_UIF_data*)opt)->flags&~IF_RECV_DISABLED)|IF_RECV_ENABLED;
			result=0;
			break;
		case IFD_SEND_OFF:
			((struct Deci2_UIF_data*)opt)->flags=(((struct Deci2_UIF_data*)opt)->flags|IF_SEND_DISABLED)&~IF_SEND_ENABLED;
			result=0;
			break;
		case IFD_SEND_ON:
			((struct Deci2_UIF_data*)opt)->flags=(((struct Deci2_UIF_data*)opt)->flags&~IF_SEND_DISABLED)|IF_SEND_ENABLED;
			result=0;
			break;
		case IFD_DEBUG:
			((struct Deci2_UIF_data*)opt)->DebugFlags=param1;
			break;
		case IFD_SHUTDOWN:
			while(UIF_SendData()>0){};
			tcp_close(UIFData.uif_pcb);
		//	tcp_close(UIFData.uif_svr_pcb);	//Is this already deallocated? Closing this connection causes a freeze.
			SMAPStop();
			break;
	}

	return result;
}

static int tif_open(void *private){
	return 0;
}

static int tif_close(void *private){
	sceDeci2IfEventHandler(IFM_DOWN, ((struct Deci2_UIF_data*)private)->interface, 0, 0, 0);
	return 0;
}

static int tif_IsReset(void *private){
	return(((struct Deci2_UIF_data*)private)->flags&IF_MAIN_RESET);
}

static int CheckForResetPacket(struct Deci2_UIF_data *pUIFData, const void *buffer, unsigned int length){
	int CopyLen, offset, DataProcessed;
	DECI2_HEADER Deci2Header;
	static const u32 EE_DCMP_Reset_packet[]={
		0x0000000C,
		0x45450001,
		0x00000004
	};

	offset=0;
	DataProcessed=0;
	do{
		if((CopyLen=8-DataProcessed)>0){	//Copy only the first 8 bytes (DECI2 header) of the block.
			if(length<CopyLen) CopyLen=length;

			bcopy(buffer, &((unsigned char*)&Deci2Header)[offset], CopyLen);
			(unsigned char*)buffer+=CopyLen;
			length-=CopyLen;
			offset+=CopyLen;
			DataProcessed+=CopyLen;
		}

		if(DataProcessed>=8){	//Only if there's a complete DECI2 header read in. At this point, skipping this whole block of code won't cause an infinite loop because there won't be any data left (length==0).
			//Check for a magic packet.
			if(Deci2Header.proto==DECI2_PROTO_NETMP &&
				Deci2Header.src=='H' &&
				Deci2Header.dest=='H' &&
				((NETMP_HEADER*)buffer)->code==NETMP_CODE_RESET){
					//Stuff for resetting the EE, used by the TDB startup card's special EE kernel.
					sceSifSetMSFlag(0x00010000);
					sceSifSetMSFlag(0x00020000);
					sceSifSetMSFlag(0x00040000);
					tif_send(&UIFData, EE_DCMP_Reset_packet, sizeof(EE_DCMP_Reset_packet));

					pUIFData->flags|=IF_MAIN_RESET;
					sceDeci2_27();
					return -1;
			}
			else{
				//Advance to the next packet, if any.
				if((CopyLen=Deci2Header.len-DataProcessed)>0){
					if(length<CopyLen) CopyLen=length;

					(unsigned char*)buffer+=CopyLen;
					length-=CopyLen;
					offset+=CopyLen;
					DataProcessed+=CopyLen;
				}

				//End of packet hit.
				if(DataProcessed>=Deci2Header.len){
					DataProcessed=0;
					offset=0;
				}
			}
		}
	}while(length>0);

	return 0;
}

static int tif_send(void *private, const void *buffer, unsigned int length){
	int result;
	struct Deci2_UIF_TR_data *TRData;

	if((result=CheckForResetPacket(private, buffer, length))>=0){
		TRData=&((struct Deci2_UIF_data*)private)->TxData;

		if((result=WriteData(TRData, buffer, length, 1))>0){
			SignalUIFActivity(private, IF_RECV_ENABLED);
		}
	}

	return result;
}

static int tif_recv(void *private, void *buffer, unsigned int length){
	struct Deci2_UIF_TR_data *RxData;
	int result;

	RxData=&((struct Deci2_UIF_data*)private)->RxData;

	if((result=ReadData(RxData, buffer, length, 1))>0){
		SignalUIFActivity(private, IF_SEND_ENABLED);
	}

	return result;
}

static void DummyPrintRoutine(char **string, int c){
	//Does nothing.
}

/* static void IKTTYPSocketHandler(int event, int param, void* opt){
	unsigned char buffer[4];

	if(event==IFD_RCV_READ){
		sceDeci2ExRecv(((struct Deci2_UIF_data*)opt)->IKTTYPSocket, buffer, sizeof(buffer));
	}
} */

static err_t uif_sent(void *arg, struct tcp_pcb *tpcb, u16_t len){
	while(UIF_SendData()>0){};

	return ERR_OK;
}

static err_t uif_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
	int result;
	err_t ReturnCode;

	if(p!=NULL){
		if(UIFData.LastIncompletePbuf==NULL){
			UIFData.LastIncompletePbuf=p;
			UIFData.RemainingDataInPbuf=p->len;
		}

		ReturnCode=ERR_OK;
		do{
			result=tif_send(&UIFData, &((unsigned char*)UIFData.LastIncompletePbuf->payload)[UIFData.LastIncompletePbuf->len-UIFData.RemainingDataInPbuf], UIFData.RemainingDataInPbuf);

			if(result>=0){
				UIFData.RemainingDataInPbuf-=result;

				if(result!=UIFData.RemainingDataInPbuf){
					ReturnCode=ERR_MEM;
					break;
				}
			}

			UIFData.LastIncompletePbuf=UIFData.LastIncompletePbuf->next;
			UIFData.RemainingDataInPbuf=UIFData.LastIncompletePbuf->len;

			if(result<0) break;	//Interface down.
		}while(UIFData.LastIncompletePbuf!=NULL);

		if(ReturnCode==ERR_OK){
			tcp_recved(tpcb, p->tot_len);
			pbuf_free(p); //If no data is left, free the whole PBUF chain.
		}
	}
	else{
		tcp_close(UIFData.uif_pcb);
		UIFData.uif_pcb=NULL;
		tif_close(&UIFData);
		ReturnCode=ERR_ABRT;
	}

	return ReturnCode;
}

static err_t uif_accept(void * arg, struct tcp_pcb * newpcb, err_t err){
	static const u32 DCMP_H_I_HelloPacket[]={
		0x00000020,
		0x49480001,
		0x00000000,
		0x00000000,
		0x00000000,
		0x00000000,
		0x00000000,
		0x00000000
	};

	UIFData.uif_pcb=newpcb;
	tcp_accepted(UIFData.uif_svr_pcb);
	tcp_sent(UIFData.uif_pcb, &uif_sent);
	tcp_recv(UIFData.uif_pcb, &uif_recv);

	sceDeci2IfEventHandler(IFM_UP, UIFData.interface, 0, 0, 0);
	tif_send(&UIFData, DCMP_H_I_HelloPacket, sizeof(DCMP_H_I_HelloPacket));

	return ERR_OK;
}

static inline void uif_init(void){
	ps2ip_InitTimer();

	UIFData.uif_svr_pcb=tcp_new();
	tcp_arg(UIFData.uif_svr_pcb, &UIFData);
	tcp_bind(UIFData.uif_svr_pcb, IP_ADDR_ANY, 8510);
	UIFData.uif_svr_pcb=tcp_listen(UIFData.uif_svr_pcb);
	UIFData.uif_pcb=NULL;
	tcp_accept(UIFData.uif_svr_pcb, &uif_accept);
}

static inline void PreRebootInit(void){
	//Synchronize with the EE after getting it to reset itself, used by the TDB startup card's special EE kernel.
	while(!(sceSifGetMSFlag()&0x00010000)){};

	*(volatile u32*)0xbf801538=0;	// Disable DMA channel 10 (SIF1, EE->IOP).
}

static void RebootThread(void *arg){
	SleepThread();

	PreRebootInit();
	ReBootStart(RebootPath, 0x80000000);
}

static inline void InitRebootThread(void){
	iop_thread_t RebootThreadData;

	RebootThreadData.attr=TH_C;
	RebootThreadData.option=0;
	RebootThreadData.thread=&RebootThread;
	RebootThreadData.stacksize=0x400;
	RebootThreadData.priority=10;
	StartThread(UIFData.RebootThreadID=CreateThread(&RebootThreadData), &UIFData);
}

int _start(int argc, char *argv[]){
	static const struct UIFBootConfiguration BootConfiguration={
		0,
		{0, 0x5E0},
		{192, 168, 0, 10},
		{255, 255, 255, 0},
		{192, 168, 0, 1},
	};

	smap_init(BootConfiguration.smap.noAuto, BootConfiguration.smap.conf);
	InitPS2IP(BootConfiguration.ip_address, BootConfiguration.subnet_mask, BootConfiguration.gateway);

	sceDeci2DbgPrintStatus(&DummyPrintRoutine, NULL);
	//DECI2KPRT is used, so do not register the dummy IKTTYP handler.
//	UIFData.IKTTYPSocket=sceDeci2Open(DECI2_PROTO_IKTTYP, &UIFData, &IKTTYPSocketHandler);

	UIFData.flags|=IF_RECV_ENABLED;

	sceDeci2SetTifHandlers(&TIFHandlers);
	InitRebootThread();

	UIFData.interface=sceDeci2IfCreate('H', &UIFData, &HostIFHandler, &HostIFActivityPollCallback);
	uif_init();

	return MODULE_RESIDENT_END;
}
