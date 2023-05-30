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

DECI2_img.o: IOPRP.img
	bin2o $(EE_GPVAL) $< $@ DECI2_img

DEV9_irx.o: $(PS2SDK)/iop/irx/ps2dev9.irx
	bin2o $(EE_GPVAL) $< $@ DEV9_irx

IMGDRV_irx.o: imgdrv.irx
	bin2o $(EE_GPVAL) $< $@ IMGDRV_irx

DECI2DRU_irx.o: deci2dru.irx
	bin2o $(EE_GPVAL) $< $@ DECI2DRU_irx

include $(PS2SDK)/Defs.make
