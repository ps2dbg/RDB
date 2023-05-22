/*++

  Module name:
	tty-ctl.c

  Description:
	Control Codes for tty device over DECI2 Remote File Protocol.

--*/

#ifndef _TTY_CTL_H
#define _TTY_CTL_H

// ioctl commands for DECI2FILE's tty device

// invoke sceDeci2Poll once
#define TTYDEV_DECI2_POLL			0x00006602

// Set raw mode. arg = enable (1) / disable (0)
// If in raw mode, received XON/XOFF characters are forwarded to the stream,
// instead of being interpreted by the driver.
#define TTYDEV_SET_RAW				0x00007401

// Flush input/buffers
#define TTYDEV_FLUSH				0x00007402

// Re-open terminal.
#define TTYDEV_REOPEN				0x00007403

// Set resume processing on EOT.  arg = enable (1) / disable (0)
// If enabled, EOT cancels a preceeding XOFF
#define TTYDEV_SET_RESUME_ON_EOT	0x00007405

#endif
