## 2017/06/10 - v0.9.5:
  + Re-added the SONY customization to `DECI2DRS` - which is to not wait for transfers to end or to inform the EE of SIF2 shutting down.
  + Added a delay to `TIFINET`, to allow the DCMP reset command packet to be sent to the EE before the IOP is reboot. This allows the DECI2 reset command to work.
  + Re-added `RDB-UIF`.
  + Updated to use LWIP v2.0.0.
  + Removed `POWEROFF.IRX`, as the modern `DEV9` revisions are no longer dependent on it.

## 2016/11/04 - v0.9.4:
  + `SIF_SYSREG_RPCINIT` will now be set for the stock kernel, to prevent the EE SIFRPC implementation from waiting on the IOP to initialize (when it already is). The TDB startup card's kernel already does this.
  + Added patch code to disable the reset of the GS when the stock kernel is used.

## 2016/07/11 - v0.9.3:
  + The IOP may crash if RDB was supplied an IOPRP image newer than v2.0, due to FILEIO being of the newer design. To prevent this, the console initialization process is moved earlier and an IOP reboot is added. The memory card is now the only supported boot device for RDB, from now on.

## 2016/06/19 - v0.9.2:
  + Updated the OSD setting initialization code.
  + Added code that copies the OSD settings to the scratchpad, so that the TDB startup card kernel can retrieve them.
  + Changed the initialization system for the stock kernel. The stock kernel will now be loaded from rom0 and patched.
  + The stock kernel will now break upon initialization and on Exit(), like the TDB startup card's kernel.
  + Fixed the mlist command for DSIDB; `DECI2LOAD`'s support for the module LIST command under the DBGP protocol was broken.
  + Added support for the use of an external `IOPRP` image, making it possible to use different IOP kernel modules.
  + Refactored code for better reusability across the various variants of RDB.

## 2014/02/15 - v0.9.1:
  + Initial public release
