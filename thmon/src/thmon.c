#include <errno.h>
#include <irx.h>
#include <intrman.h>
#include <ioman.h>
#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <types.h>
#include <thbase.h>
#include <thevent.h>
#include <thmsgbx.h>
#include <thpool.h>
#include <thsemap.h>
#include "thread_ext.h"

#define MODNAME "Thread_monitor"
IRX_ID(MODNAME, 1, 1);

static int tty_fd;
static struct Thbase_internal *ThbaseInternal;

enum HELP_STRING{
	HELP_STRING_TITLE	= 0,
	HELP_STRING_TH_LIST,
	HELP_STRING_RD_LIST,
	HELP_STRING_SL_LIST,
	HELP_STRING_DL_LIST,
	HELP_STRING_RT_LIST,
	HELP_STRING_SEM_LIST,
	HELP_STRING_EV_LIST,
	HELP_STRING_MSG_LIST,
	HELP_STRING_VP_LIST,
	HELP_STRING_FP_LIST,
	HELP_STRING_ALM_LIST,
	HELP_STRING_THLED,

	HELP_STRING_COUNT
};

static const char *HelpStrings[HELP_STRING_COUNT+1]={
	"\n\n========= simple thread monitor program =========\n",
	"-- thlist [-v]  -- thread list\n",
	"-- rdlist [-v]  -- ready thread list\n",
	"-- sllist [-v]  -- sleeping thread list\n",
	"-- dllist [-v]  -- delayed thread list\n",
	"-- rtlist [-v]  -- thread running time list\n",
	"-- semlist  -- semaphore list\n",
	"-- evlist   -- eventflag list\n",
	"-- msglist  -- messagebox list\n",
	"-- vpllist  -- Vpool list\n",
	"-- fpllist  -- Fpool list\n",
	"-- almlist  -- Alarm list\n",
	"-- thled <on/off> -- thread swiching LED display on/off\n",
	NULL
};

static const char null_character='\0';
static char PreviousCommand[0xD4];	//0x00002c3c
static char CommandBuffer[0xC4];	//0x00002d10

static inline const char *GetModuleName(void *entry_point){
	lc_internals_t *loadcoreData;
	ModuleInfo_t *module;
	const char *result;

	result=NULL;
	if((loadcoreData=GetLoadcoreInternalData())!=NULL){
		module=loadcoreData->image_info;
		while(module!=NULL){
			if(entry_point>=(void*)module->text_start && entry_point<(void*)(module->text_start+module->text_size)){
				result=module->name;
				break;
			}

			module=module->next;
		}
	}

	return result;
}

static void PrintMessage(const char *cmd){
	fdprintf(tty_fd, "%s", cmd);
}

static char *RemoveLeadingWhitespaces(char *string){
	while(*string==' ' || *string=='\x09'){
		*string='\0';
		string++;
	}

	return string;
}

static char *GetEndOfString(char *string){	//Returns a pointer to the end of the string. A whitespace character will also be considered as a end-of-string marker.
	while(*string!='\0'){
		if(*string==' ' || *string=='\x09'){
			break;
		}

		string++;
	}

	return string;
}

static void DisplayHelpMessage(void){
	const char **line;
	int i;

	i=0;
	line=HelpStrings;
	while(*line!=NULL){
		fdprintf(tty_fd, *line);

		/* The Sony original had something like this block of code instead:
			unsigned int h32, pagebreak;
			unsigned long long int result;

			result=(i+1)*0x12642C9ULL;
			h32=result>>32;
			pagebreak=((((((h32+i+1)>>4)-((i+1)>>31))<<1) + (((h32+i+1)>>4)-((i+1)>>31)))<<3)-(((h32+i+1)>>4)-((i+1)>>31));
			if(i+1==pagebreak){	//Basically: if((i+1)%23==0)
		*/
		if((i+1)%23==0){
			fdprintf(tty_fd, " more ? ");
			if(getchar()=='y'){
				fdprintf(tty_fd, "\r");
			}
			else{
				fdprintf(tty_fd, "\n");
				break;
			}
		}

		line++;
		i++;
	}
}

static int func_000020bc(const void *arg1){
	return(((*(unsigned int*)arg1)^(unsigned int)arg1)<1?1:0);
}

static void rtlist_cmd_handler(char *args){
	char *CmdPtr;
	int verbose;
	const struct Thbase_TCB *tcb;
	t_ThreadInfo info;
	u32 sec, usec;
	const char *modname, *string;
	iop_sys_clock_t	clock;

	verbose=0;
	while(args[0]!=0){
		if((CmdPtr=GetEndOfString(args))[0]!='\0'){
			CmdPtr[0]='\0';
			CmdPtr=RemoveLeadingWhitespaces(&CmdPtr[1]);
		}

		if(strcmp(args, "-v")==0){
			verbose=1;
		}

		args=CmdPtr;
	}

	fdprintf(tty_fd, "    THID   CLOCK               SEC\n");

	if((tcb=ThbaseInternal->LastTCB)!=NULL){
		do{
			if(ReferThreadStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, (iop_thread_info_t*)&info)<0){
				fdprintf(tty_fd, "thid = %x not found \n", ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1);
			}
			else{
				if(info.status!=THS_DORMANT){
					clock.lo=tcb->clock_low;
					clock.hi=tcb->clock_high;
					SysClock2USec(&clock, &sec, &usec);

					if(tcb!=ThbaseInternal->FirstTCB){
						fdprintf(tty_fd, "%3d %06x %08x_%08x %5d.%06d", tcb->ThreadID, ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, tcb->clock_high, tcb->clock_low, sec, usec);

						if(verbose){
							if((modname=GetModuleName(info.entry))!=NULL){
								fdprintf(tty_fd, "    Name=%s", modname);
							}
						}

						string="\n";
					}
					else{
						fdprintf(tty_fd, "           %08x_%08x %5d.%06d", tcb->clock_high, tcb->clock_low, sec, usec);
						string="  System Idle time\n";
					}

					fdprintf(tty_fd, string);
				}
			}
		}while((tcb=(const struct Thbase_TCB *)tcb->next)!=NULL);
	}
}

static void DumpThreads(const char *args){
	const struct Thbase_TCB *tcb;

	fdprintf(tty_fd, "======================");
	fdprintf(tty_fd, "tcb list ----\n");

	if((tcb=ThbaseInternal->LastTCB)!=NULL){
		do{
			fdprintf(tty_fd, "  %d: tcb=%x sts=0x%x pri=%d ", tcb->ThreadID, (unsigned int)tcb, tcb->status, tcb->priority);

			if(tcb->status==THS_WAIT){
				fdprintf(tty_fd, "wType = %x:%x ", tcb->WaitType, tcb->WaitID);
			}
			if(tcb->status==THS_RUN){
				fdprintf(tty_fd, "contx=%x sp=%x", tcb->regs, tcb->regs[29]);
			}

			fdprintf(tty_fd, "\n");
		}while((tcb=(const struct Thbase_TCB *)tcb->next)!=NULL);
	}

	fdprintf(tty_fd, "sleep list ----\n");

	if(func_000020bc(&ThbaseInternal->SleepTCBList)==0){
		fdprintf(tty_fd, " %x: ", ThbaseInternal->SleepTCBList);

		tcb=(struct Thbase_TCB*)&ThbaseInternal->SleepTCBList;
		while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->SleepTCBList){
			fdprintf(tty_fd, " %d(%x) ", tcb->ThreadID, (unsigned int)tcb);
		}

		fdprintf(tty_fd, "\n");
	}
}

static void DumpReadyQueue(const char *args){
	int OldState;
	unsigned int i;
	const struct Thbase_TCB *tcb;

	fdprintf(tty_fd, "ready queue ----\n");

	for(i=0; i<0x7F; i++){
		tcb=(const struct Thbase_TCB *)&ThbaseInternal->ReadyTCBList[i];
		if(func_000020bc(tcb)==0){
			CpuSuspendIntr(&OldState);

			fdprintf(tty_fd, " %3d:%x ", i, (unsigned int)tcb);
			while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->ReadyTCBList[i]){
				fdprintf(tty_fd, " %d(%x) ", tcb->ThreadID, (unsigned int)tcb);
			}

			fdprintf(tty_fd, "\n");

			CpuResumeIntr(OldState);
		}
	}

	fdprintf(tty_fd, "ready map = ");
	for(i=0; i<4; i++){
		fdprintf(tty_fd, "%08x ", ThbaseInternal->ReadyMap[i]);
	}
}

static void ToggleThLED(const char *args){
	if(strcmp(args, "on")==0){
		ThbaseInternal->ThreadmanAttr|=0x20;
	}
	if(strcmp(args, "off")==0){
		ThbaseInternal->ThreadmanAttr&=0xFFFFFFDF;
	}

	if(look_ctype_table(args[0])&4){
		ThbaseInternal->ThreadmanAttr=strtol(args, NULL, 10);
	}
}

static void ToggleWarnDisp(const char *args){
	if(strcmp(args, "on")==0){
		ThbaseInternal->ThreadmanAttr|=0x8;
	}
	if(strcmp(args, "off")==0){
		ThbaseInternal->ThreadmanAttr&=0xFFFFFFF7;
	}

	if(look_ctype_table(args[0])&4){
		ThbaseInternal->ThreadmanAttr=strtol(args, NULL, 10);
	}
}

static void ToggleThswdisp(const char *args){
	if(strcmp(args, "on")==0){
		ThbaseInternal->ThreadmanAttr|=0x1;
	}
	if(strcmp(args, "off")==0){
		ThbaseInternal->ThreadmanAttr&=0xFFFFFFFE;
	}

	if(look_ctype_table(args[0])&4){
		ThbaseInternal->ThreadmanAttr=strtol(args, NULL, 10);
	}
}

static void ShowVplList(const char *args){
	const struct Thbase_TCB *tcb;
	iop_vpl_info_t VplStatus;

	if(func_000020bc(&ThbaseInternal->FplTCBList)==0){
		fdprintf(tty_fd, "    VPLID  ATTR     OPTION   Size   Free   waitThreads\n");

		tcb=(const struct Thbase_TCB *)&ThbaseInternal->VplTCBList;
		while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->VplTCBList){
			if(ReferVplStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, &VplStatus)>=0){
				fdprintf(tty_fd, "%3d %06x %08x %08x %06x %06x   %5d\n", tcb->ThreadID, ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, VplStatus.attr, VplStatus.option, VplStatus.size, VplStatus.freeSize, VplStatus.numWaitThreads);
			}
		}
	}
}

static void ShowFplList(const char *args){
	const struct Thbase_TCB *tcb;
	iop_fpl_info_t FplStatus;

	if(func_000020bc(&ThbaseInternal->FplTCBList)==0){
		fdprintf(tty_fd, "    FPLID  ATTR     OPTION   BlkSIze AllBlocks FreeSize waitThreads\n");

		tcb=(const struct Thbase_TCB *)&ThbaseInternal->FplTCBList;
		while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->FplTCBList){
			if(ReferFplStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, &FplStatus)>=0){
				fdprintf(tty_fd, "%3d %06x %08x %08x %06x  %06x    %06x    %5d\n", tcb->ThreadID, ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, FplStatus.attr, FplStatus.option, FplStatus.blockSize, FplStatus.numBlocks, FplStatus.freeBlocks, FplStatus.numWaitThreads);
			}
		}
	}
}

/* 0x18 bytes */
struct AlarmListEnt{
	iop_sys_clock_t clock;		/* 0x00 */
	unsigned int AlarmID;		/* 0x08 */
	void *handler;			/* 0x0C */
	void *common;			/* 0x10 */
	unsigned int unknown_14;	/* 0x14 */
};

#define alloca(size) __builtin_alloca(size)	//The homebrew PS2SDK lacks alloca.h.

static void ShowAlarmList(const char *args){
	struct Thbase_AlarmInfo *AlarmInfo;
	unsigned int NumThreads, i;
	int OldState;
	struct AlarmListEnt *alarms, *alarm;

	CpuSuspendIntr(&OldState);

	alarms=NULL;	//The Sony original doesn't have this initialized.
	NumThreads=0;
	if(func_000020bc(&ThbaseInternal->AlarmTCBList)==0){
		AlarmInfo=(struct Thbase_AlarmInfo *)ThbaseInternal->AlarmTCBList.next;
		while(AlarmInfo!=(struct Thbase_AlarmInfo *)&ThbaseInternal->AlarmTCBList){
			AlarmInfo=(struct Thbase_AlarmInfo *)AlarmInfo->link.next;
			NumThreads++;
		}

		alarms=alloca(NumThreads*sizeof(struct AlarmListEnt));

		alarm=alarms;
		while((AlarmInfo=(struct Thbase_AlarmInfo *)AlarmInfo->link.next)!=(struct Thbase_AlarmInfo *)&ThbaseInternal->AlarmTCBList){
			alarm->AlarmID=AlarmInfo->AlarmID;
			alarm->clock.lo=AlarmInfo->clock.lo;
			alarm->clock.hi=AlarmInfo->clock.hi;
			alarm->handler=AlarmInfo->handler;
			alarm->common=AlarmInfo->common;

			alarm++;
		}
	}

	CpuResumeIntr(OldState);

	fdprintf(tty_fd, " NUM    TIME           HANDLER COMMON\n");
	for(i=0,alarm=alarms; i<NumThreads; i++,alarm++){
		fdprintf(tty_fd, "%3d %08x_%08x  %06x, %08x\n", alarm->AlarmID, alarm->clock.hi, alarm->clock.lo, (unsigned int)alarm->handler, (unsigned int)alarm->common);
	}
}

static void ShowMsgbxList(const char *args){
	const struct Thbase_TCB *tcb;
	iop_mbx_status_t MbxStatus;

	if(func_000020bc(&ThbaseInternal->MsgbxTCBList)==0){
		fdprintf(tty_fd, "    MSGID  ATTR     OPTION   waitThreads  messages\n");

		tcb=(const struct Thbase_TCB *)&ThbaseInternal->MsgbxTCBList;
		while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->MsgbxTCBList){
			if(ReferMbxStatus((int)((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, &MbxStatus)>=0){
				fdprintf(tty_fd, "%3d %06x %08x %08x %5d        %5d\n", tcb->ThreadID, ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, MbxStatus.attr, MbxStatus.option, MbxStatus.numWaitThreads, MbxStatus.numMessage);
			}
		}
	}
}

static void ShowEventFlagList(const char *args){
	const struct Thbase_TCB *tcb;
	iop_event_info_t EFStatus;

	if(func_000020bc(&ThbaseInternal->EventTCBList)==0){
		fdprintf(tty_fd, "    EVID   ATTR     OPTION   iPattern cPattern waitThreads\n");

		tcb=(const struct Thbase_TCB *)&ThbaseInternal->EventTCBList;
		while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->EventTCBList){
			if(ReferEventFlagStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, &EFStatus)>=0){
				fdprintf(tty_fd, "%3d %06x %08x %08x %08x %08x  %5d\n", tcb->ThreadID, ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, EFStatus.attr, EFStatus.option, EFStatus.initBits, EFStatus.currBits, EFStatus.numThreads);
			}
		}
	}
}

static void ShowSemList(const char *args3){
	const struct Thbase_TCB *tcb;
	iop_SemaInfo_t SemaStatus;

	if(func_000020bc(&ThbaseInternal->SemaTCBList)==0){
		fdprintf(tty_fd, "    SEMID  ATTR     OPTION   iCnt  cCnt  mCnt  waitThreads\n");

		tcb=(const struct Thbase_TCB *)&ThbaseInternal->SemaTCBList;
		while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->SemaTCBList){
			if(ReferSemaStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, (iop_sema_info_t*)&SemaStatus)>=0){
				fdprintf(tty_fd, "%3d %06x %08x %08x %05d %5d %5d  %5d\n", tcb->ThreadID, ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, SemaStatus.attr, SemaStatus.option, SemaStatus.initCount, SemaStatus.currentCount, SemaStatus.maxCount, SemaStatus.numWaitThreads);
			}
		}
	}
}

static const char ThreadStatus[]="000RunRdy333Wat5556667";
static const char ThreadWaitStatus[]="  SlDlSeEvMbVpFp  ";

static void DisplayDelayedThreadInformation(const struct Thbase_TCB *tcb, const t_ThreadInfo *info, int verbose){
	const char *ModuleName;
	unsigned int FreeStackSpace, *StackData;

	fdprintf(tty_fd, "%3d %06x %08x %08x %.3s ", tcb->ThreadID, ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, info->attr, info->option, &ThreadStatus[(info->status<<1)+info->status]);
	fdprintf(tty_fd, "%06x %06x %04x %06x %3d %3d %d\n", info->entry, info->stack, info->stackSize, info->gpReg, info->currentPriority, info->initPriority, info->waitId);
	if(verbose){
		if((ModuleName=GetModuleName(info->entry))!=NULL){
			fdprintf(tty_fd, "    Name=%s\n", ModuleName);
		}
	}

	if(info->regContext!=NULL){
		if(info->status!=THS_DORMANT){
			fdprintf(tty_fd, "    PC=%06x  RA=%06x  SP=%06x  Context_addr/mask=%08x/%08x\n", info->regContext[0x23], info->regContext[0x1F], info->regContext[0x1D], info->regContext, info->regContext[0]);
		}
	}
	if(verbose){
		FreeStackSpace=0;
		StackData=info->stack;
		for(; FreeStackSpace<info->stackSize && (*StackData==0xFFFFFFFF || *StackData==0x11111111); FreeStackSpace+=4,StackData++){
		}
		fdprintf(tty_fd, "    Free_Stack_Size=%06x\n", FreeStackSpace);
	}
}

static void dllist_cmd_handler(char *args){
	char *CmdPtr;
	int verbose;
	const struct Thbase_TCB *tcb;
	t_ThreadInfo info;

	verbose=0;
	while(args[0]!=0){
		if((CmdPtr=GetEndOfString(args))[0]!='\0'){
			CmdPtr[0]='\0';
			CmdPtr=RemoveLeadingWhitespaces(&CmdPtr[1]);
		}

		if(strcmp(args, "-v")==0){
			verbose=1;
		}

		args=CmdPtr;
	}

	fdprintf(tty_fd, "    THID   ATTR     OPTION   STS ENTRY  STACK  SSIZE GP     CP  IP USEC\n");

	tcb=(const struct Thbase_TCB *)&ThbaseInternal->DelayTCBList;
	while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->DelayTCBList){
		if(ReferThreadStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, (iop_thread_info_t*)&info)<0){
			fdprintf(tty_fd, "thid = %x not found \n", ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1);
		}
		else{
			if(tcb!=ThbaseInternal->FirstTCB){
				DisplayDelayedThreadInformation(tcb, &info, verbose);
			}
		}
	}
}

static void DisplayThreadInformation(const struct Thbase_TCB *tcb, const t_ThreadInfo *info, int verbose){
	const char *ModuleName;
	unsigned int FreeStackSpace, *StackData;

	fdprintf(tty_fd, "%3d %06x %08x %08x %.3s ", tcb->ThreadID, ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, info->attr, info->option, &ThreadStatus[(info->status<<1)+info->status]);
	fdprintf(tty_fd, "%06x %06x %04x %06x %3d %3d %c%c %06x %2x\n", info->entry, info->stack, info->stackSize, info->gpReg, info->currentPriority, info->initPriority, ThreadWaitStatus[info->waitType<<1], ThreadWaitStatus[(info->waitType<<1)+1], info->waitId, info->wakeupCount);
	if(verbose){
		if((ModuleName=GetModuleName(info->entry))!=NULL){
			fdprintf(tty_fd, "    Name=%s\n", ModuleName);
		}
	}

	if(info->regContext!=NULL){
		if(info->status!=THS_DORMANT){
			fdprintf(tty_fd, "    PC=%06x  RA=%06x  SP=%06x  Context_addr/mask=%08x/%08x\n", info->regContext[0x23], info->regContext[0x1F], info->regContext[0x1D], info->regContext, info->regContext[0]);
		}
	}
	if(verbose){
		FreeStackSpace=0;
		StackData=info->stack;
		for(; FreeStackSpace<info->stackSize && (*StackData==0xFFFFFFFF || *StackData==0x11111111); FreeStackSpace+=4,StackData++){
		}
		fdprintf(tty_fd, "    Free_Stack_Size=%06x\n", FreeStackSpace);
	}
}

static void sllist_cmd_handler(char *args){
	char *CmdPtr;
	int verbose;
	const struct Thbase_TCB *tcb;
	t_ThreadInfo info;

	verbose=0;
	while(args[0]!=0){
		if((CmdPtr=GetEndOfString(args))[0]!='\0'){
			CmdPtr[0]='\0';
			CmdPtr=RemoveLeadingWhitespaces(&CmdPtr[1]);
		}

		if(strcmp(args, "-v")==0){
			verbose=1;
		}

		args=CmdPtr;
	}

	fdprintf(tty_fd, "    THID   ATTR     OPTION   STS ENTRY  STACK  SSIZE GP     CP  IP WT WID   WUC\n");

	tcb=(const struct Thbase_TCB *)&ThbaseInternal->SleepTCBList;
	while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->SleepTCBList){
		if(ReferThreadStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, (iop_thread_info_t*)&info)<0){
			fdprintf(tty_fd, "thid = %x not found \n", ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1);
		}
		else{
			if(tcb!=ThbaseInternal->FirstTCB){
				DisplayThreadInformation(tcb, &info, verbose);
			}
		}
	}
}

static void rdlist_cmd_handler(char *args){
	char *CmdPtr;
	int verbose;
	const struct Thbase_TCB *tcb;
	t_ThreadInfo info;
	unsigned int i;

	verbose=0;
	while(args[0]!=0){
		if((CmdPtr=GetEndOfString(args))[0]!='\0'){
			CmdPtr[0]='\0';
			CmdPtr=RemoveLeadingWhitespaces(&CmdPtr[1]);
		}

		if(strcmp(args, "-v")==0){
			verbose=1;
		}

		args=CmdPtr;
	}

	fdprintf(tty_fd, "    THID   ATTR     OPTION   STS ENTRY  STACK  SSIZE GP     CP  IP WT WID   WUC\n");

	for(i=1; i<0x7F; i++){
		tcb=(const struct Thbase_TCB *)&ThbaseInternal->ReadyTCBList[i];
		if(func_000020bc(tcb)==0){
			while((tcb=(const struct Thbase_TCB *)tcb->link.next)!=(const struct Thbase_TCB *)&ThbaseInternal->ReadyTCBList[i]){
				if(ReferThreadStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, (iop_thread_info_t*)&info)==0){
					DisplayThreadInformation(tcb, &info, verbose);
				}
			}
		}
	}
}

static void thlist_cmd_handler(char *args){
	char *CmdPtr;
	int verbose;
	const struct Thbase_TCB *tcb;
	t_ThreadInfo info;

	verbose=0;
	while(args[0]!=0){
		if((CmdPtr=GetEndOfString(args))[0]!='\0'){
			CmdPtr[0]='\0';
			CmdPtr=RemoveLeadingWhitespaces(&CmdPtr[1]);
		}

		if(strcmp(args, "-v")==0){
			verbose=1;
		}

		args=CmdPtr;
	}

	fdprintf(tty_fd, "    THID   ATTR     OPTION   STS ENTRY  STACK  SSIZE GP     CP  IP WT WID   WUC\n");

	if((tcb=ThbaseInternal->LastTCB)!=NULL){
		do{
			if(ReferThreadStatus(((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1, (iop_thread_info_t*)&info)<0){
				fdprintf(tty_fd, "thid = %x not found \n", ((tcb->ThreadID&0x3F)<<1|(unsigned int)tcb<<5)|1);
			}
			else{
				if(tcb!=ThbaseInternal->FirstTCB){
					DisplayThreadInformation(tcb, &info, verbose);
				}
			}
		}while((tcb=(const struct Thbase_TCB *)tcb->next)!=NULL);
	}
}

static void ThmonMainThread(void *arg){
	char *command, *command2;

	fdprintf(tty_fd, HelpStrings[HELP_STRING_TITLE]);
	fdprintf(tty_fd, "help command is 'help'\n");

	PreviousCommand[0]=null_character;

	while(1){
		fdprintf(tty_fd, " > ");

		if(fdgets(CommandBuffer, tty_fd)!=0){
			PrintMessage(CommandBuffer);
			if(strlen(CommandBuffer)==0) continue;

			if(strcmp(CommandBuffer, "/")!=0){
				strcpy(PreviousCommand, CommandBuffer);
			}
			else{
				strcpy(CommandBuffer, PreviousCommand);
			}

			command2=RemoveLeadingWhitespaces(GetEndOfString(command=RemoveLeadingWhitespaces(CommandBuffer)));

			if(command[0]=='#') continue;

			if(strncmp("quit", command, strlen(command))==0){
				break;
			}
			else if(strncmp("?", command, strlen(command))==0 || strncmp("help", command, strlen(command))==0){
				DisplayHelpMessage();
			}
			else if(strncmp("thlist", command, strlen(command))==0){
				thlist_cmd_handler(command2);
			}
			else if(strncmp("rdlist", command, strlen(command))==0){
				rdlist_cmd_handler(command2);
			}
			else if(strncmp("sllist", command, strlen(command))==0){
				sllist_cmd_handler(command2);
			}
			else if(strncmp("dllist", command, strlen(command))==0){
				dllist_cmd_handler(command2);
			}
			else if(strncmp("rtlist", command, strlen(command))==0){
				rtlist_cmd_handler(command2);
			}
			else if(strncmp("semlist", command, strlen(command))==0){
				ShowSemList(command2);
			}
			else if(strncmp("evlist", command, strlen(command))==0){
				ShowEventFlagList(command2);
			}
			else if(strncmp("msglist", command, strlen(command))==0){
				ShowMsgbxList(command2);
			}
			else if(strncmp("vpllist", command, strlen(command))==0){
				ShowVplList(command2);
			}
			else if(strncmp("fpllist", command, strlen(command))==0){
				ShowFplList(command2);
			}
			else if(strncmp("almlist", command, strlen(command))==0){
				ShowAlarmList(command2);
			}
			else if(strncmp("thswdisp", command, strlen(command))==0){
				ToggleThswdisp(command2);
			}
			else if(strncmp("thled", command, strlen(command))==0){
				ToggleThLED(command2);
			}
			else if(strncmp("warndisp", command, strlen(command))==0){
				ToggleWarnDisp(command2);
			}
			else if(strncmp("dumpthread", command, strlen(command))==0){
				DumpThreads(command2);
			}
			else if(strncmp("dumpready", command, strlen(command))==0){
				DumpReadyQueue(command2);
			}
			else{
				fdprintf(tty_fd, " ?? \n");
			}

		}
		else break;
	}
}

static int CreateThmonThread(unsigned int attr, void (*entry)(void *), unsigned int priority, unsigned int StackSize, unsigned int option){
	iop_thread_t thread;

	thread.attr=attr;
	thread.thread=entry;
	thread.priority=priority;
	thread.stacksize=StackSize;
	thread.option=option;

	return CreateThread(&thread);
}

/* Entry point */
int _start(int argc, char *argv[]){
	int ThreadID, result;

	tty_fd=open("tty9:", O_RDWR);

	ThbaseInternal=GetThbaseInternalStruct();	/* THBASE export #03 */
	if((ThreadID=CreateThmonThread(TH_C, &ThmonMainThread, 6, 0x800, 0))>=0){
		StartThread(ThreadID, NULL);
		result=MODULE_RESIDENT_END;
	}
	else result=MODULE_NO_RESIDENT_END;

	return result;
}
