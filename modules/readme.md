Modules and load order:
-----------------------

| ID | Name             | Description (label)              | Description
| ---| ---------------- | -------------------------------- | ------------------------------------------ |
| 1. | `DECI2`[^4]      | Deci2_Manager				       | DECI2 manager 								|
| 2. | `DECI2DRP`[^3]   | Deci2_PIF_interface_driver	   | PIF interface driver 						|
| 3. | `DECI2HSYN`	    | Deci2_host_sync			       | Host synchronization module 				|
| 4. | `DECI2DRS` [^5]  | Deci2_SIF2_interface_driver	   | SIF2 interface driver 						|
| 5. | `DECI2FILE`	    | Deci2_TTY/FILE_driver		       | TTY and host device I/O driver. 			|
| 6. | `DECI2KPRT`[^2]  | Deci2_Kprintf_driver		       | Kprintf driver 							|
| 7. | `DECI2LOAD`	    | Deci2_Load_Manager			   | Executable file loading manager. 			|
| 8. | `DRVTIF` [^1]    | Deci2_TIF_interface_driver	   | TIF interface driver 						|
| 9. | `TIFINET`[^1]    | TIF_bridge_for_INET			   | Internet bridge driver for the TIF.	 	|


[^1]: DRVTIF and TIFINET are used on debugstation consoles, in place of DECI2DRP.
[^2]: DECI2KPRT is not used on debugstation consoles.
[^3]: DECI2DRP is only available on TOOL units because only TOOLs have the PIF.
[^4]: The TDB startup card has a newer DECI2 module that has additional functions for managing the TIF.
[^5]: The TDB startup card has a slight different DECI2DRS module that doesn't wait for ongoing transfers to end and doesn't notify the EE of the shutdown.

Additional functions in the newer DECI2 manager, as found in the TDB startup card.
- `#25`	sceDeci2SetTifHandlers
- `#26`	sceDeci2GetTifHandlers
- `#27`	(Notifies the DECI2 manager of completed transmission and receiving activities)
- `#28`	(Notifies the DECI2 manager of TIF activity)
- `#29-32`	Does nothing.
