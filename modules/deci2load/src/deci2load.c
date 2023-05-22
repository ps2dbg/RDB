/*	DECI2LOAD.IRX
	Deci2_Load_Manager v2.03 */

#include <deci2.h>
#include <irx.h>
#include <intrman.h>
#include "intrman_add.h"
#include <kerr.h>
#include <loadcore.h>
#include "loadcore_add.h"
#include <modload.h>
#include "modload_add.h"
#include <sysmem.h>
#include "sysmem_add.h"
#include <thbase.h>

#define MODNAME "Deci2_Load_Manager"
IRX_ID(MODNAME, 2, 3);

#define NEW_MODLOAD_VERSION	0x203	//Anything older than this doesn't seem to use a module status flags field.

struct Deci2LoadManagerPrivateData{
	int ILOADPSocket;
	int ThreadID;
	unsigned short int ModloadVersion;
	unsigned short int PacketRecvOffset;
	unsigned char RecvBuffer[176];
	unsigned char IncomingPacketHeader[16];
	unsigned char RecvError;
	unsigned char IsSending;
	unsigned short int PacketsInSendBuffer;
	unsigned short int SendBufferWrPtr;		//In 32-bit words
	unsigned short int SendBufferRdPtr;		//In 32-bit words
	unsigned short int SendBufferLevel;		//In 32-bit words
	unsigned short int PacketSendOffset;
	unsigned int SendBuffer[64];
	ModuleInfo_t *CurrentModule;
	void *ModloadInternalData;
	unsigned int IndexInModuleChain;
	int CurrentModuleID;
	int SysmemMemBlockList[8];
	unsigned char TxBuffer[128];		//Used for actually sending data out of SendBuffer.
};

struct Deci2LoadManagerDbgpData{
	void *ModloadInternalData;		//0x00
	ModuleInfo_t *CurrentModule;		//0x04
	unsigned int IndexInModuleChain;	//0x08
	int CurrentModuleID;			//0x0C
	unsigned int SysmemMemBlockList[8];	//0x10
};

static struct Deci2LoadManagerPrivateData PrivateData;	//0x00001a60
static struct Deci2LoadManagerDbgpData DbgpData;	//0x00001d10

static void MainThread(void *arg);
static int InitILOADP(iop_init_entry_t* next, int delayed);
static void SendResponseToPacket(void *private, int result);
static void SendModuleStateReport(void *private, int ModuleID, int result, int ModuleStartResult);
static void request_send(void *private, int mode);
static ModuleInfo_t *GetModuleInformation(int ModuleID);
static void SendPacket(void *private);
static void CopyToSendBuffer(void *private, const void *buffer, int length);
static void CopyFromSendBuffer(void *private, void *buffer, int length);
static void ILOADPSocketHandler(int event, int param, void* opt);
static void InitDbgGroup(void);

static void DebugGroupHandler_HeaderDone(sceDbgpGroupEvent* evt);
static void DebugGroupHandler_Read(sceDbgpGroupEvent* evt);
static void DebugGroupHandler_ReadDone(sceDbgpGroupEvent* evt);
static void DebugGroupHandler_Write(sceDbgpGroupEvent* evt);
static void DebugGroupHandler_WriteDone(sceDbgpGroupEvent* evt);
static void DebugGroupHandler_ReqSend(sceDbgpGroupEvent* evt);

static unsigned short int GetModloadVersion(void){
	unsigned short int ModloadVersion;
	ModuleInfo_t *ModInfo;

	ModloadVersion=0;
	ModInfo=GetLoadcoreInternalData()->image_info;
	while(ModInfo!=NULL){
		if(!strcmp(ModInfo->name, "Moldule_File_loader")){
			ModloadVersion=ModInfo->version;
			break;
		}

		ModInfo=ModInfo->next;
	}

	return ModloadVersion;
}

//0x00000000
int _start(int argc, char *argv[]){
	iop_thread_t ThreadData;

	memset(&PrivateData, 0, sizeof(PrivateData));
	PrivateData.ModloadVersion=GetModloadVersion();

	ThreadData.attr=TH_C;
	ThreadData.thread=&MainThread;
	ThreadData.priority=8;
	ThreadData.stacksize=0x800;
	ThreadData.option=0;
	PrivateData.ThreadID=CreateThread(&ThreadData);
	StartThread(PrivateData.ThreadID, NULL);

	PrivateData.ModloadInternalData=NULL;
	GetModloadInternalData(&PrivateData.ModloadInternalData);	//SDK v3.00
	RegisterInitCompleteHandler(&InitILOADP, 2, 0);

	return MODULE_RESIDENT_END;
}

//0x00000098
static int InitILOADP(iop_init_entry_t* next, int delayed){
	int result;

	if((PrivateData.ILOADPSocket=sceDeci2Open(DECI2_PROTO_ILOADP, &PrivateData, &ILOADPSocketHandler))>=0){
		InitDbgGroup();
		result=0;
	}
	else{
		result=1;
	}

	return result;
}

//0x000000ec
static int ConvertErrorCode(int code){
	int result;

	switch(code){
		case KE_LINKERR:
			result=1;
			break;
		case KE_ILLEGAL_OBJECT:
			result=3;
			break;
		case KE_UNKNOWN_MODULE:
			result=4;
			break;
		case KE_NOFILE:
			result=7;
			break;
		case KE_FILEERR:
			result=8;
			break;
		case KE_MEMINUSE:
			result=9;
			break;
		case -206:	//KE_ALREADY_STARTED
			result=10;
			break;
		case KE_NO_MEMORY:
			result=2;
			break;
		default:
			result=0;
	}

	return result;
}

//0x00000194
static void MainThread(void *arg){
	int i, StringLength, ModuleID, result, ModuleStartResult;
	unsigned short int ModuleFlags;
	char *ptr, *ptrs[20];
	ModuleInfo_t *ModuleData;

	while(1){
		do{
			ChangeThreadPriority(0, 8);
			SleepThread();
		}while(((volatile DECI2_HEADER*)PrivateData.IncomingPacketHeader)->len==0);

		if(!(((LOADP_HEADER*)&PrivateData.IncomingPacketHeader[sizeof(DECI2_HEADER)])->action)){
			SendResponseToPacket(&PrivateData, 5);
			continue;
		}

		StringLength=((DECI2_HEADER*)PrivateData.IncomingPacketHeader)->len-(sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER));
		ptr=&PrivateData.RecvBuffer[sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)];
		for(i=0; i<20; i++){
			if((unsigned int)((unsigned char*)ptr-(unsigned char*)&PrivateData.RecvBuffer[sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)])>=StringLength){
				break;
			}

			ptrs[i]=ptr;
			ptr+=(strlen(ptr)+1);
		}
		ptrs[i]=NULL;
		if(((LOADP_HEADER*)&PrivateData.IncomingPacketHeader[sizeof(DECI2_HEADER)])->action&1){
			//0x0000028c
			if((ModuleID=LoadModule(ptrs[0]))>=0){
				ModuleData=GetModuleInformation(ModuleID);
				((LOADP_HEADER*)&PrivateData.IncomingPacketHeader[sizeof(DECI2_HEADER)])->module_id=ModuleID;
			}
			else{
				result=ConvertErrorCode(ModuleID);
				SendResponseToPacket(&PrivateData, result);
				continue;
			}
		}
		else{
			//0x000002d4
			ModuleData=GetModuleInformation(((LOADP_HEADER*)&PrivateData.IncomingPacketHeader[sizeof(DECI2_HEADER)])->module_id);
			if(ModuleData==NULL){
				SendResponseToPacket(&PrivateData, 4);
				continue;
			}
			ModuleID=((LOADP_HEADER*)&PrivateData.IncomingPacketHeader[sizeof(DECI2_HEADER)])->module_id;
		}

		//0x00000300
		if(!(((LOADP_HEADER*)&PrivateData.IncomingPacketHeader[sizeof(DECI2_HEADER)])->action&2)){
			SendResponseToPacket(&PrivateData, 0);
			continue;
		}

		if(PrivateData.ModloadVersion>=NEW_MODLOAD_VERSION){
			//0x00000324 - If the module is loaded.
			if((ModuleFlags=ModuleData->newflags&0xF)==1){
				SendResponseToPacket(&PrivateData, 0);
				if((result=StartModule(ModuleID, ptrs[0], StringLength-strlen(ptrs[0])-1, ptrs[1], &ModuleStartResult))<0){
					sceDeci2ExPanic("ILOAD START unexpected error 0x%x\n", result);
					continue;
				}

				if(!(((LOADP_HEADER*)&PrivateData.IncomingPacketHeader[sizeof(DECI2_HEADER)])->action&8)){
					continue;
				}

				//0x000003b8
				result=ModuleStartResult&3;
				if(result == ModuleFlags){
					SendModuleStateReport(&PrivateData, ModuleID, 3, ModuleStartResult);
				}
				else if((result==0) || (result==2)){
					SendModuleStateReport(&PrivateData, ModuleID, 4, ModuleStartResult);
					ModuleData->newflags|=0x8000;
				}
			}
			else{
				SendResponseToPacket(&PrivateData, 10);
			}
		}
		else{
			SendResponseToPacket(&PrivateData, 0);
			if((result=StartModule(ModuleID, ptrs[0], StringLength-strlen(ptrs[0])-1, ptrs[1], &ModuleStartResult))<0){
				sceDeci2ExPanic("ILOAD START unexpected error 0x%x\n", result);
				continue;
			}

			if(!(((LOADP_HEADER*)&PrivateData.IncomingPacketHeader[sizeof(DECI2_HEADER)])->action&8)){
				continue;
			}

			result=ModuleStartResult&3;
			if(result!=0){
				SendModuleStateReport(&PrivateData, ModuleID, 3, ModuleStartResult);
			}
			else{
				SendModuleStateReport(&PrivateData, ModuleID, 4, ModuleStartResult);
			}
		}
	}	
}

//0x00000430
static void iSendResponseToPacket(void *private, int result){	//Packet = the incoming packet whose header is in IncomingPacketHeader.
	unsigned char temp[16];

	memset(temp, 0, sizeof(temp));

	((unsigned int*)temp)[0]=((unsigned int*)((struct Deci2LoadManagerPrivateData*)private)->IncomingPacketHeader)[0];
	((unsigned int*)temp)[1]=((unsigned int*)((struct Deci2LoadManagerPrivateData*)private)->IncomingPacketHeader)[1];
	((unsigned int*)temp)[2]=((unsigned int*)((struct Deci2LoadManagerPrivateData*)private)->IncomingPacketHeader)[2];
	((unsigned int*)temp)[3]=((unsigned int*)((struct Deci2LoadManagerPrivateData*)private)->IncomingPacketHeader)[3];
	((DECI2_HEADER*)temp)->len=sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER);
	((DECI2_HEADER*)temp)->src=((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->IncomingPacketHeader)->dest;
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->result=result;
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->cmd=LOADP_CMD_STARTR;
	((DECI2_HEADER*)temp)->dest=((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->IncomingPacketHeader)->src;

	memset(((struct Deci2LoadManagerPrivateData*)private)->IncomingPacketHeader, 0, 8);

	CopyToSendBuffer(private, temp, 4);
	((struct Deci2LoadManagerPrivateData*)private)->PacketsInSendBuffer++;
	request_send(private, 1);
}

//0x000004e8
static void SendResponseToPacket(void *private, int result){
	CpuInvokeInKmode(&iSendResponseToPacket, private, result);
}

//0x00000518
static void SendResult(void *private, void *buffer, int command, int result){
	unsigned char temp[16];

	memset(temp, 0, sizeof(temp));

	((unsigned int*)temp)[0]=((unsigned int*)buffer)[0];
	((unsigned int*)temp)[1]=((unsigned int*)buffer)[1];
	((unsigned int*)temp)[2]=((unsigned int*)buffer)[2];
	((unsigned int*)temp)[3]=((unsigned int*)buffer)[3];
	((DECI2_HEADER*)temp)->len=sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER);
	((DECI2_HEADER*)temp)->src=((DECI2_HEADER*)buffer)->dest;
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->result=result;
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->cmd=command;
	((DECI2_HEADER*)temp)->dest=((DECI2_HEADER*)buffer)->src;
	CopyToSendBuffer(private, temp, 4);
	SendPacket(private);
}

//0x000005c4
static void SendReport(void *private, int ModuleID, int result, int arg4){
	unsigned char temp[16];

	memset(temp, 0, sizeof(temp));
	((DECI2_HEADER*)temp)->len=sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)+4;
	((DECI2_HEADER*)temp)->src='I';
	((DECI2_HEADER*)temp)->dest='H';
	((DECI2_HEADER*)temp)->proto=DECI2_PROTO_ILOADP;

	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->result=result;
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->cmd=LOADP_CMD_REPORT;
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->module_id=ModuleID;
	CopyToSendBuffer(private, temp, 4);
	CopyToSendBuffer(private, &arg4, 1);
	SendPacket(private);
}

//0x00000668
static void iSendModuleStateReport(void *private, int *args){
	unsigned char temp[16];

	memset(temp, 0, sizeof(temp));

	((DECI2_HEADER*)temp)->len=sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)+4;
	((DECI2_HEADER*)temp)->src='I';
	((DECI2_HEADER*)temp)->dest='H';
	((DECI2_HEADER*)temp)->proto=DECI2_PROTO_ILOADP;
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->cmd=LOADP_CMD_REPORT;
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->result=args[1];
	((LOADP_HEADER*)&temp[sizeof(DECI2_HEADER)])->module_id=args[0];
	CopyToSendBuffer(private, temp, 4);
	CopyToSendBuffer(private, &args[2], 1);
	((struct Deci2LoadManagerPrivateData*)private)->PacketsInSendBuffer++;
	request_send(private, 1);
}

//0x00000714
static void SendModuleStateReport(void *private, int ModuleID, int result, int ModuleStartResult){
	int args[3];

	args[0]=ModuleID;
	args[1]=result;
	args[2]=ModuleStartResult;
	CpuInvokeInKmode(&iSendModuleStateReport, private, args);
}

//0x00000750
static ModuleInfo_t *GetModuleInformation(int ModuleID){
	ModuleInfo_t *ModInfo, *result;

	result=NULL;
	ModInfo=GetLoadcoreInternalData()->image_info;
	while(ModInfo!=NULL){
		if(ModInfo->id==ModuleID){
			result=ModInfo;
			break;
		}

		ModInfo=ModInfo->next;
	}

	return result;
}

static void request_send_mod_stat_response(void *private, int mode);
static void request_send_response(void *private, int mode);
static void request_send_listr(void *private, int mode);
static void request_send_infor(void *private, int mode);

//0x00000e4c
static void request_send_mod_stat_response(void *private, int mode){
	unsigned int value;

	value=*(unsigned int*)&((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)];

	if((value&0x00FF0400)!=0x400){	//Result!=OK && (action&LOADP_ACT_INFO)
		request_send_response(private, mode);
	}
	else{
		request_send_infor(private, mode);
	}
}

//0x00000f80
static void request_send_response(void *private, int mode){
	int result;

	((struct Deci2LoadManagerPrivateData*)private)->PacketSendOffset=0;
	((struct Deci2LoadManagerPrivateData*)private)->IsSending=1;
	((struct Deci2LoadManagerPrivateData*)private)->PacketsInSendBuffer--;
	if(mode==0){
		result=sceDeci2ExReqSend(((struct Deci2LoadManagerPrivateData*)private)->ILOADPSocket, ((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->TxBuffer)->dest);
	}
	else{
		result=sceDeci2ReqSend(((struct Deci2LoadManagerPrivateData*)private)->ILOADPSocket, ((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->TxBuffer)->dest);
	}

	if(result<0){
		sceDeci2ExPanic("ILOAD request_send: ReqSend error %d\n", result);
		((struct Deci2LoadManagerPrivateData*)private)->IsSending=0;
	}
}

//0x00000dd4
static void request_send_listr(void *private, int mode){
	((struct Deci2LoadManagerPrivateData*)private)->IndexInModuleChain=0;
	((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->TxBuffer)->len=GetLoadcoreInternalData()->module_count*4+sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER);
	((struct Deci2LoadManagerPrivateData*)private)->CurrentModule=GetLoadcoreInternalData()->image_info;

	request_send_response(private, mode);
}

//0x00000e68
static void request_send_infor(void *private, int mode){
	ModuleInfo_t *ModuleData;
	char *ModuleName;
	LOADP_MOD_INFO *ModInfo;

	((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->TxBuffer)->len=sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER);

	if((ModuleData=GetModuleInformation(((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)])->module_id))!=NULL){
		((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->TxBuffer)->len+=sizeof(LOADP_MOD_INFO)+1;

		ModInfo=(LOADP_MOD_INFO*)&((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)];
		if((ModuleName=ModuleData->name)!=NULL){
			((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->TxBuffer)->len+=strlen(ModuleName);
		}

		memset(ModInfo, 0, sizeof(LOADP_MOD_INFO));

		ModInfo->version=ModuleData->version;
		ModInfo->flags=ModuleData->newflags;
		ModInfo->module_top_addr=ModuleData->text_start;
		ModInfo->text_size=ModuleData->text_size;
		ModInfo->data_size=ModuleData->data_size;
		ModInfo->bss_size=ModuleData->bss_size;
		ModInfo->entry=ModuleData->entry;
		((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)+sizeof(LOADP_MOD_INFO)]='\0';
		ModInfo->gp=ModuleData->gp;

		if(ModuleData->name!=NULL){
			strcpy(&((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)+sizeof(LOADP_MOD_INFO)], ModuleData->name);
		}

		request_send_response(private, mode);
	}
	else{
		((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)])->result=4;
		request_send_response(private, mode);
	}
}

//0x00000d5c
static void request_send(void *private, int mode){
	if(((struct Deci2LoadManagerPrivateData*)private)->PacketsInSendBuffer!=0 && ((struct Deci2LoadManagerPrivateData*)private)->IsSending==0){
		((struct Deci2LoadManagerPrivateData*)private)->PacketSendOffset=0;
		CopyFromSendBuffer(private, ((struct Deci2LoadManagerPrivateData*)private)->TxBuffer, 4);
		switch(((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)])->cmd){
			case LOADP_CMD_STARTR:
				request_send_mod_stat_response(private, mode);
				break;
			case LOADP_CMD_WATCHR:
			case LOADP_CMD_REMOVER:
				request_send_response(private, mode);
				break;
			case LOADP_CMD_LISTR:
				request_send_listr(private, mode);
				break;
			case LOADP_CMD_INFOR:
				request_send_infor(private, mode);
				break;
			case LOADP_CMD_MEMLISTR:
				sysmem_11(1, ((struct Deci2LoadManagerPrivateData*)private)->SysmemMemBlockList);	//SDK v3.00
				((struct Deci2LoadManagerPrivateData*)private)->CurrentModule=GetLoadcoreInternalData()->image_info;

				if(((struct Deci2LoadManagerPrivateData*)private)->SysmemMemBlockList[0]>0){
					((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)private)->TxBuffer)->len=((struct Deci2LoadManagerPrivateData*)private)->SysmemMemBlockList[0]*4+sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)+4;
					request_send_response(private, mode);
				}
				else{
					((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)])->result=5;
					request_send_response(private, mode);
				}
				break;
			case LOADP_CMD_REPORT:
				CopyFromSendBuffer(private, &((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)+sizeof(LOADP_HEADER)], 1);
				request_send_response(private, mode);
				break;
			default:
				sceDeci2ExPanic("ILOAD request_send: unknown reply code 0x%x\n", ((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)private)->TxBuffer[sizeof(DECI2_HEADER)])->cmd);
		}
	}
}

//0x000007a4
static int func_000007a4(void *ModloadInternalData, void *MemoryBlock){
	//FIXME	- information required on Modload export 3.

	while((void*)((unsigned int*)ModloadInternalData)[7]!=&((unsigned int*)ModloadInternalData)[7]){
		if((void*)((unsigned int*)ModloadInternalData)[7]==MemoryBlock){
			return 0;
		}

		(void*)((unsigned int*)ModloadInternalData)[7]=*(void**)((unsigned int*)ModloadInternalData)[7];
	}

	return -1;
}

//0x000007dc
static void ILOADPSocketHandler(int event, int param, void* opt){
	ModuleInfo_t *ModInfo;

	switch(event){
		case DECI2_READ:	//0x0000081c
			if(((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset<0x10){
				((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset+=sceDeci2ExRecv(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, &((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset], 0x10-((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset);
			}
			else{
				//0x0000083c
				if(((struct Deci2LoadManagerPrivateData*)opt)->RecvError==0){
					if(((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)])->cmd==LOADP_CMD_START){
						if(((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)opt)->IncomingPacketHeader)->len!=0){
							((struct Deci2LoadManagerPrivateData*)opt)->RecvError=1;
							SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, LOADP_CMD_STARTR, 6);
						}
						else{
							if((((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer)->len)<0xB1){
								((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset+=sceDeci2ExRecv(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, &((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset], 0xB0-((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset);
							}
							else{
								((struct Deci2LoadManagerPrivateData*)opt)->RecvError=1;
								SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, LOADP_CMD_STARTR, 5);
							}
						}
					}
					else{
						((struct Deci2LoadManagerPrivateData*)opt)->RecvError=1;
					}
				}
				else{
					((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset+=sceDeci2ExRecv(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, &((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)], 8);
					break;
				}
			}

			//0x000008f8
			if(((struct Deci2LoadManagerPrivateData*)opt)->RecvError!=0){
				((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset+=sceDeci2ExRecv(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, &((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)], 8);
			}

			break;
		case DECI2_READDONE:	//0x0000092c
			((struct Deci2LoadManagerPrivateData*)opt)->PacketRecvOffset=0;
			if(((struct Deci2LoadManagerPrivateData*)opt)->RecvError==0){
				switch(((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)])->cmd){
					case LOADP_CMD_START:
						((unsigned int*)((struct Deci2LoadManagerPrivateData*)opt)->IncomingPacketHeader)[0]=((unsigned int*)((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer)[0];
						((unsigned int*)((struct Deci2LoadManagerPrivateData*)opt)->IncomingPacketHeader)[1]=((unsigned int*)((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer)[1];
						((unsigned int*)((struct Deci2LoadManagerPrivateData*)opt)->IncomingPacketHeader)[2]=((unsigned int*)((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer)[2];
						((unsigned int*)((struct Deci2LoadManagerPrivateData*)opt)->IncomingPacketHeader)[3]=((unsigned int*)((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer)[3];
						sceDeci2ExWakeupThread(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, ((struct Deci2LoadManagerPrivateData*)opt)->ThreadID);
						break;
					case LOADP_CMD_REMOVE:
						SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, LOADP_CMD_REMOVER, 5);
						break;
					case LOADP_CMD_LIST:
						SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, LOADP_CMD_LISTR, 0);
						break;
					case LOADP_CMD_INFO:
						SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, LOADP_CMD_INFOR, 0);
						break;
					case LOADP_CMD_WATCH:	//0x000009f4
						if((ModInfo=GetModuleInformation(((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)])->module_id))!=NULL){
							if(((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)])->action){
								ModInfo->newflags|=0x8000;
							}
							else{
								ModInfo->newflags&=0x7FFF;
							}

							SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, LOADP_CMD_WATCHR, 0);

							((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)])->stamp=0;
							if(((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)])->action){
								if(PrivateData.ModloadVersion>=NEW_MODLOAD_VERSION){
									switch(ModInfo->newflags&0xF){
										case 1:
											SendReport(opt, ModInfo->id, 1, 0);
											break;
										case 2:
											SendReport(opt, ModInfo->id, 2, 0);
											break;
										case 3:
											SendReport(opt, ModInfo->id, 4, 0);
											break;
										default:
											sceDeci2ExPanic("LoadHandler: MODC_WATCH unknown module status 0x%x\n", ModInfo->newflags&0xF);
									}
								}
								else{	//This was copied from DECI2LOAD v1.01, but I don't see the old MODLOAD module setting any of these bits.
									if(ModInfo->newflags&0x4000){
										SendReport(opt, ModInfo->id, 1, 0);
									}
									else if(ModInfo->newflags&0x2000){
										SendReport(opt, ModInfo->id, 2, 0);
									}
									else{
										SendReport(opt, ModInfo->id, 4, 0);
									}
								}
							}
						}
						else{	//0x00000ae8
							SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, LOADP_CMD_WATCHR, 4);
						}
						break;
					case LOADP_CMD_MEMLIST:
						SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, LOADP_CMD_MEMLISTR, 0);
						break;
					case LOADP_CMD_STARTR:
					case LOADP_CMD_REMOVER:
					case LOADP_CMD_LISTR:
					case LOADP_CMD_INFOR:
					case LOADP_CMD_WATCHR:
						SendResult(opt, ((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer, ((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->RecvBuffer[sizeof(DECI2_HEADER)])->cmd, 5);
				}
			}
			else{
				((struct Deci2LoadManagerPrivateData*)opt)->RecvError=0;
			}
			break;
		case DECI2_WRITE:	//0x00000b1c
			if(((struct Deci2LoadManagerPrivateData*)opt)->PacketSendOffset<0x10){
				((struct Deci2LoadManagerPrivateData*)opt)->PacketSendOffset+=sceDeci2ExSend(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, &((struct Deci2LoadManagerPrivateData*)opt)->TxBuffer[((struct Deci2LoadManagerPrivateData*)opt)->PacketSendOffset], 0x10-((struct Deci2LoadManagerPrivateData*)opt)->PacketSendOffset);
			}
			else{
				//0x00000b44
				switch(((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->TxBuffer[sizeof(DECI2_HEADER)])->cmd){
					case LOADP_CMD_INFOR:
					case LOADP_CMD_REPORT:
					case LOADP_CMD_STARTR:
						((struct Deci2LoadManagerPrivateData*)opt)->PacketSendOffset+=sceDeci2ExSend(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, &((struct Deci2LoadManagerPrivateData*)opt)->TxBuffer[((struct Deci2LoadManagerPrivateData*)opt)->PacketSendOffset], ((DECI2_HEADER*)((struct Deci2LoadManagerPrivateData*)opt)->TxBuffer)->len-((struct Deci2LoadManagerPrivateData*)opt)->PacketSendOffset);
						break;
					case LOADP_CMD_LISTR:
						if(((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule!=NULL && (GetLoadcoreInternalData()->module_count>=((struct Deci2LoadManagerPrivateData*)opt)->IndexInModuleChain)){
							//0x00000bac
							//The Sony original loads a full word here, although the module ID is only 16-bits wide. Most other functions treat this field as a 16-bit number. The upper 16-bits appear to be uninitialized by the old MODLOAD module, which causes compatibility problems.
							((struct Deci2LoadManagerPrivateData*)opt)->CurrentModuleID=((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule->id;
							((struct Deci2LoadManagerPrivateData*)opt)->IndexInModuleChain++;
							((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule=((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule->next;
						}
						else{
							((struct Deci2LoadManagerPrivateData*)opt)->CurrentModuleID=-1;
						}

						sceDeci2ExSend(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, &((struct Deci2LoadManagerPrivateData*)opt)->CurrentModuleID, 4);
						break;
					case LOADP_CMD_MEMLISTR:	//0x00000be4
						sysmem_11(0, ((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList);	//SDK v3.00
						if(((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[0]>=0){
							if(((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule!=NULL){
								while((void*)((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule<(void*)((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[0]){
									//0x00000c14
									if((((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule=((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule->next)==NULL){
										break;
									}
								}
							}

							//0x00000c38
							void *address;
							((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[0]|=(((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[1]&1);
							address=(void*)((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[0];
							if(((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[1]!=1){
								if((((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[1]&3)!=2){
									if(((struct Deci2LoadManagerPrivateData*)opt)->CurrentModule==address){
										//0x00000c90
										if(((struct Deci2LoadManagerPrivateData*)opt)->ModloadInternalData!=NULL){
											if(func_000007a4(((struct Deci2LoadManagerPrivateData*)opt)->ModloadInternalData, address)==0){
												((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[0]|=8;
											}
										}
									}
									else{
										((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[0]|=6;
									}
								}
								else{
									((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[0]|=4;
								}
							}
							else{
								((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList[0]|=2;
							}
						}

						//0x00000cc0
						((struct Deci2LoadManagerPrivateData*)opt)->PacketSendOffset+=sceDeci2ExSend(((struct Deci2LoadManagerPrivateData*)opt)->ILOADPSocket, ((struct Deci2LoadManagerPrivateData*)opt)->SysmemMemBlockList, 4);
						break;
					default:
						sceDeci2ExPanic("LoadHandler: WRITE unknown case 0x%x\n", ((LOADP_HEADER*)&((struct Deci2LoadManagerPrivateData*)opt)->TxBuffer[sizeof(DECI2_HEADER)])->cmd);
				}
			}
			break;
		case DECI2_WRITEDONE:	//0x00000d20
			((struct Deci2LoadManagerPrivateData*)opt)->IsSending=0;
			request_send(opt, 0);
			break;
		case DECI2_CHSTATUS:
		case DECI2_ERROR:	//0x00000d48
			//Do nothing.
			break;
		default:	//0x00000d38
			sceDeci2ExPanic("LoadHandler: unknown event 0x%x\n", event);
	}

}

//0x00000ffc
static void CopyToSendBuffer(void *private, const void *buffer, int length){
	int i;

	if(((struct Deci2LoadManagerPrivateData*)private)->SendBufferLevel+length<0x41){
		for(i=0; i<length; i++){
			((struct Deci2LoadManagerPrivateData*)private)->SendBuffer[((struct Deci2LoadManagerPrivateData*)private)->SendBufferWrPtr]=((const unsigned int*)buffer)[i];
			((struct Deci2LoadManagerPrivateData*)private)->SendBufferWrPtr++;
			((struct Deci2LoadManagerPrivateData*)private)->SendBufferLevel++;

			if(((struct Deci2LoadManagerPrivateData*)private)->SendBufferWrPtr>=0x40){
				((struct Deci2LoadManagerPrivateData*)private)->SendBufferWrPtr=0;
			}
		}
	}
	else{
		sceDeci2ExPanic("ILOAD send buffer overflow\n");
	}
}

//0x00001094
static void SendPacket(void *private){
	((struct Deci2LoadManagerPrivateData*)private)->PacketsInSendBuffer++;
	request_send(private, 0);
}

//0x000010c0
static void CopyFromSendBuffer(void *private, void *buffer, int length){
	int i;

	if(((struct Deci2LoadManagerPrivateData*)private)->SendBufferLevel<length){
		sceDeci2ExPanic("ILOAD send buffer underflow\n");
		return;
	}

	for(i=0; length>0; length--,((struct Deci2LoadManagerPrivateData*)private)->SendBufferLevel--,i++){
		((unsigned int*)buffer)[i]=((struct Deci2LoadManagerPrivateData*)private)->SendBuffer[((struct Deci2LoadManagerPrivateData*)private)->SendBufferRdPtr];

		((struct Deci2LoadManagerPrivateData*)private)->SendBufferRdPtr++;
		if(((struct Deci2LoadManagerPrivateData*)private)->SendBufferRdPtr>=0x40){
			((struct Deci2LoadManagerPrivateData*)private)->SendBufferRdPtr=0;
		}
	}
}

//0x00001160
static void InitDbgGroup(void){
	static sceDbgpGroup DebugGroupHandlers={	//0x00001a40
		&DbgpData,
		&DebugGroupHandler_HeaderDone,
		&DebugGroupHandler_Read,
		&DebugGroupHandler_ReadDone,
		&DebugGroupHandler_Write,
		&DebugGroupHandler_WriteDone,
		&DebugGroupHandler_ReqSend
	};

	sceDeci2AddDbgGroup(2, &DebugGroupHandlers);
	DbgpData.ModloadInternalData=NULL;
	GetModloadInternalData(&DbgpData.ModloadInternalData);	//SDK v3.00
}

//0x00001198
static void DebugGroupHandler_HeaderDone(sceDbgpGroupEvent* evt){
	evt->retlen=(evt->pkt->dbgp.type==DBGP_MODULE_TYPE_INFO)?0x14:0x10;

	switch(evt->pkt->dbgp.type){
		case DBGP_MODULE_TYPE_LIST:
		case DBGP_MODULE_TYPE_INFO:
		case DBGP_MODULE_TYPE_MEMLIST:
			break;
		default:
			evt->pkt->dbgp.result=DBGP_RESULT_INVALREQ;
	}
}

//0x000011f8
static void DebugGroupHandler_Read(sceDbgpGroupEvent* evt){
	evt->pkt->dbgp.type|=1;
	evt->pkt->dbgp.result=12;
}

//0x00001220
static void DebugGroupHandler_ReadDone(sceDbgpGroupEvent* evt){
	ModuleInfo_t *ModInfo;
	LOADP_MOD_INFO *LOADP_ModInfo;

	switch(evt->pkt->dbgp.type){
		case DBGP_MODULE_TYPE_LIST:
			evt->pkt->deci2.len=0x10;
			evt->pkt->dbgp.type=DBGP_MODULE_TYPE_LISTR;
			evt->pkt->dbgp.result=0;

			evt->pkt->deci2.len+=GetLoadcoreInternalData()->module_count*4;
			((struct Deci2LoadManagerDbgpData*)evt->opt)->IndexInModuleChain=0;
			((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule=GetLoadcoreInternalData()->image_info;
			break;
		case DBGP_MODULE_TYPE_INFO:	//0x000012a0
			evt->pkt->deci2.len=0x14;
			evt->pkt->dbgp.type=DBGP_MODULE_TYPE_INFOR;
			evt->pkt->dbgp.result=0;

			if((ModInfo=GetModuleInformation(*(int*)((u8*)evt->pkt + sizeof(DBGP_MSG_HEADER))))!=NULL){
				evt->pkt->deci2.len=0x35;
				if(ModInfo->name!=NULL){
					evt->pkt->deci2.len+=strlen(ModInfo->name);
				}

				LOADP_ModInfo=(LOADP_MOD_INFO*)((u8*)evt->pkt + sizeof(DBGP_MSG_HEADER)+4);
				memset(LOADP_ModInfo, 0, sizeof(LOADP_MOD_INFO));
				
				LOADP_ModInfo->version=ModInfo->version;
				LOADP_ModInfo->flags=ModInfo->newflags;
				LOADP_ModInfo->module_top_addr=ModInfo->text_start;
				LOADP_ModInfo->text_size=ModInfo->text_size;
				LOADP_ModInfo->data_size=ModInfo->data_size;
				LOADP_ModInfo->bss_size=ModInfo->bss_size;

				if(ModInfo->name!=NULL){
					strcpy((char*)((u8*)evt->pkt + sizeof(DBGP_MSG_HEADER)+4+sizeof(LOADP_MOD_INFO)), ModInfo->name);
				}
			}
			else{
				evt->pkt->dbgp.result=DBGP_RESULT_NOMOD;
			}
			break;
		//FIXME: MEMLISTR or MEMLIST?
//		case DBGP_MODULE_TYPE_MEMLISTR:	//0x000013c4
		case DBGP_MODULE_TYPE_MEMLIST:	//0x000013c4
			evt->pkt->dbgp.type=DBGP_MODULE_TYPE_MEMLISTR;
			evt->pkt->dbgp.result=0;
			((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]=-1;
			sysmem_11(1, &((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList);
			((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule=GetLoadcoreInternalData()->image_info;

			if(((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]>0){
				evt->pkt->deci2.len+=4*((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0];
			}
			else{
				evt->pkt->dbgp.result=2;
			}

			break;
	}
}

//0x00001458
static void DebugGroupHandler_Write(sceDbgpGroupEvent* evt){
	void *address;

	switch(evt->pkt->dbgp.type){
		case DBGP_MODULE_TYPE_LISTR:	//0x0000147c
			if(((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule!=NULL && GetLoadcoreInternalData()->module_count>=((struct Deci2LoadManagerDbgpData*)evt->opt)->IndexInModuleChain){
				//0x000014c0
				//As above: the Sony original loads a full word here, although the module ID is only 16-bits wide. Most other functions treat this field as a 16-bit number. The upper 16-bits appear to be uninitialized by the old MODLOAD module, which causes compatibility problems.
				((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModuleID=((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule->id;
				((struct Deci2LoadManagerDbgpData*)evt->opt)->IndexInModuleChain++;
				((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule=((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule->next;
			}
			else{
				((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModuleID=-1;
			}
			//0x000014e8
			evt->retbuf=&((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModuleID;
			evt->retlen=4;
			break;
		case DBGP_MODULE_TYPE_MEMLISTR:	//0x000014f8
			sysmem_11(0, &((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]);
			//0x0000150c
			if(((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]>=0){
				if(((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule!=NULL){
					do{
						if((void*)((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule>=(void*)((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]){
							break;
						}

						((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule=((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule->next;
					}while(((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule!=NULL);
				}

				//0x00001554
				address=(void*)(((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]=((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]|(((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[1]&1));

				if(((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[1]!=1){
					if((((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[1]&3)!=2){
						if(((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule==(void*)((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]){
							((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]|=6;

							((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule=((struct Deci2LoadManagerDbgpData*)evt->opt)->CurrentModule->next;
						}
						else{
							if(((struct Deci2LoadManagerDbgpData*)evt->opt)->ModloadInternalData!=NULL){
								if(func_000007a4((void*)((struct Deci2LoadManagerDbgpData*)evt->opt)->ModloadInternalData, address)==0){
									((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]|=8;
								}
							}
						}
					}
					else{
						((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]|=4;
					}
				}
				else{
					((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0]|=2;
				}

			}

			evt->retbuf=&((struct Deci2LoadManagerDbgpData*)evt->opt)->SysmemMemBlockList[0];
			evt->retlen=4;
			break;
		default:	//0x000015ec
			evt->retbuf=evt->len+evt->pkt;
			evt->retlen=evt->pkt->deci2.len-evt->len;
	}
}

//0x00001624
static void DebugGroupHandler_WriteDone(sceDbgpGroupEvent* evt){
	//Does nothing.
}

//0x0000162c
static void DebugGroupHandler_ReqSend(sceDbgpGroupEvent* evt){
	switch(evt->pkt->dbgp.type){
		case DBGP_MODULE_TYPE_LISTR:
		case DBGP_MODULE_TYPE_MEMLISTR:
			evt->retlen=0x10;
			break;
		case DBGP_MODULE_TYPE_INFOR:
			evt->retlen=evt->pkt->deci2.len;
			break;
	}
}
