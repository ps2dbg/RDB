/*++

  Module name:
    deci2err.h

  Description:
    DECI2 Error Code Definitions.

--*/

#ifndef DECI2ERR_H
#define DECI2ERR_H

// Invalid argument
#define DECI2_ERR_INVALID -1

// Invalid socket
#define DECI2_ERR_INVALSOCK -2

// Protocol number is already being used
#define DECI2_ERR_ALREADYUSE -3

// Too many open protocols
#define DECI2_ERR_MFILE -4

// Invalid buffer address
#define DECI2_ERR_INVALADDR -5

// Buffer is too small to contain necessary packet header
#define DECI2_ERR_PKTSIZE -6

// Asynchronous call in wrong manager state
#define DECI2_ERR_WOULDBLOCK -7

// Node is already locked
#define DECI2_ERR_ALREADYLOCK -8

// Node is not locked
#define DECI2_ERR_NOTLOCKED -9

// No route to host
#define DECI2_ERR_NOROUTE -10

// Out of memory
#define DECI2_ERR_NOSPACE -11

// Invalid packet header
#define DECI2_ERR_INVALHEAD -12

// No host interface
#define DECI2_ERR_NOHOSTIF -13

#endif // ndef(DECI2ERR_H)
