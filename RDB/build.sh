#!/bin/sh

echo "Generating IOPRP image..."

export RDB_DIR=$PWD
export MODULE_DIR=$PWD/../modules
export ROMIMG_DIR=$PWD/../tools/ROMIMG

make clean --silent

#DECI2
cp $MODULE_DIR/deci2.irx $ROMIMG_DIR/DECI2

#DECI2DRS
cp $MODULE_DIR/deci2drs.irx $ROMIMG_DIR/DECI2DRS

#DECI2FILE
cp $MODULE_DIR/deci2file.irx $ROMIMG_DIR/DECI2FILE

#DECI2HSYN
cp $MODULE_DIR/deci2hsyn.irx $ROMIMG_DIR/DECI2HSYN

#DECI2KPRT - not copied because DRVTIF already has a dummy implementation for it.
#cp $MODULE_DIR/deci2kprt.irx $ROMIMG_DIR/DECI2KPRT

#DECI2LOAD
cp $MODULE_DIR/deci2load.irx $ROMIMG_DIR/DECI2LOAD

#DRVTIF
cd $MODULE_DIR/drvtif
make --silent
mv irx/drvtif.irx $RDB_DIR/
make clean --silent

#TIFINET
cd $MODULE_DIR/tifinet
make --silent
mv irx/tifinet.irx $RDB_DIR/
make clean --silent

#IMGDRV
cp $MODULE_DIR/imgdrv.irx $RDB_DIR/

#IOPRP.img
cd $ROMIMG_DIR
cp $MODULE_DIR/common/IOPBTCOK $ROMIMG_DIR/IOPBTCONF
./romimg /c IOPRP.img IOPBTCONF DECI2 DECI2DRS DECI2FILE DECI2HSYN DECI2LOAD
cp IOPRP.img $RDB_DIR/

cd $RDB_DIR
make --silent
