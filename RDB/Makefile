EE_BIN = RDB.elf

#IOP modules
EE_IOP_OBJS = IMGDRV_irx.o DECI2_img.o DEV9_irx.o DRVTIF_irx.o TIFINET_irx.o NETMAN_irx.o SMAP_irx.o PS2IP_irx.o

RDB_OBJS =  ../common/crt0.o ../common/rdb.o ../common/KernelBoot.o ../common/ipconfig.o ../common/ioprstctrl.o ../common/modutils.o ../osd/libcdvd_add.o ../osd/OSDConfig.o
EE_OBJS = main.o $(RDB_OBJS) $(EE_IOP_OBJS)

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I. -I../common -I../osd
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

DEV9_irx.o:
	bin2o $(EE_GPVAL) $(PS2SDK)/iop/irx/ps2dev9.irx DEV9_irx.o DEV9_irx

IMGDRV_irx.o:
	bin2o $(EE_GPVAL) imgdrv.irx IMGDRV_irx.o IMGDRV_irx

NETMAN_irx.o:
	bin2o $(EE_GPVAL) $(PS2SDK)/iop/irx/netman.irx NETMAN_irx.o NETMAN_irx

SMAP_irx.o:
	bin2o $(EE_GPVAL) $(PS2SDK)/iop/irx/smap.irx SMAP_irx.o SMAP_irx

PS2IP_irx.o:
	bin2o $(EE_GPVAL) $(PS2SDK)/iop/irx/ps2ip-nm.irx PS2IP_irx.o PS2IP_irx

DECI2_img.o:
	bin2o $(EE_GPVAL) IOPRP.img DECI2_img.o DECI2_img

DRVTIF_irx.o:
	bin2o $(EE_GPVAL) drvtif.irx DRVTIF_irx.o DRVTIF_irx

TIFINET_irx.o:
	bin2o $(EE_GPVAL) tifinet.irx TIFINET_irx.o TIFINET_irx

include $(PS2SDK)/Defs.make
