.set noreorder
.set noat

.extern PS2IPTimerIntrNum

.globl HostIFActivityPollCallback
.ent HostIFActivityPollCallback
HostIFActivityPollCallback:
	li	$v0, 0xBF801070
	lui	$v1, 0xB000
	lw	$v0, ($v0)
	ori	$v1, 0x0028
	andi	$v0, $v0, (1<<13)	#INUM_PCMCIA (IOP_IRQ_DEV9)

	bne	$v0, $zero, 1f
	addiu	$v0, $zero, 1

	lhu	$v0, ($v1)
	nop
	andi	$v0, $v0, 0x68		#EMAC3|RXEND|RXDNV

	bne	$v0, $zero, 1f
	addiu	$v0, $zero, 1

	la	$v1, PS2IPTimerIntrNum
	lw	$v1, ($v1)
	addiu	$v0, $zero, 1
	sllv	$v1, $v0, $v1		#INUM_ of counter.
	li	$v0, 0xBF801070
	lw	$v0, ($v0)
	nop
	and	$v0, $v0, $v1

	beq	$v0, $zero, 1f
	nop
	addiu	$v0, $zero, 1

1:
	jr	$ra
	nop
.end HostIFActivityPollCallback

.set reorder
.set at
