EE_BIN = RDB-UIF.elf

#IOP modules
EE_IOP_OBJS = DECI2_img.o IMGDRV_irx.o DEV9_irx.o DECI2DRU_irx.o

RDB_OBJS =  ../common/crt0.o ../common/rdb.o ../common/KernelBoot.o ../common/ipconfig.o ../common/ioprstctrl.o ../common/modutils.o ../osd/libcdvd_add.o ../osd/OSDConfig.o
EE_OBJS = main.o $(RDB_OBJS) $(EE_IOP_OBJS)

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I. -I../common -I../modules/common -I../osd
EE_LDFLAGS := -nostartfiles -T../common/linkfile -L$(PS2SDK)/ee/lib -L./lib -s
EE_LIBS := -lpatches -ldebug -lcdvd -lc -lkernel
EE_GPVAL = -G8192
EE_CFLAGS += -D_EE -Os -mgpopt $(EE_GPVAL)

%.o : %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.S
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_BIN) : $(EE_OBJS)
	$(EE_CC) $(EE_CFLAGS) $(EE_LDFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LIBS)

all:
	$(MAKE) $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_BIN_REL) $(EE_OBJS)

DECI2_img.o:
	bin2o $(EE_GPVAL) IOPRP.img DECI2_img.o DECI2_img

DEV9_irx.o:
	bin2o $(EE_GPVAL) $(PS2SDK)/iop/irx/ps2dev9.irx DEV9_irx.o DEV9_irx

IMGDRV_irx.o:
	bin2o $(EE_GPVAL) imgdrv.irx IMGDRV_irx.o IMGDRV_irx

DECI2DRU_irx.o:
	bin2o $(EE_GPVAL) deci2dru.irx DECI2DRU_irx.o DECI2DRU_irx

include $(PS2SDK)/Defs.make
