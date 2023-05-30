# `THMON`
This version of THMON that was reverse-engineered was from a System 246's boot ROM.

It is compatible with the old "Multi_Thread_Manager" v1.01 modules. The ones (v2.03) that come with newer games require a newer THMON module, as Sony had changed the layout of THBASE slightly.

The most obvious change is that they added a new field before the TCB lists, causing their offsets to get increased by 4 bytes each. They have also added other things as well, but this change is the one that makes the old THMON module totally incompatible with the new THREADMAN modules.

While it's possible to adapt this clone to support the newer THREADMAN module, I am not sure whether it's worth doing so. The new THMON module seems to have a number of newer features too, which might be interesting to look into.