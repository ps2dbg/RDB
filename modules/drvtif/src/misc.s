.set noreorder
.set nomacro

# It's only using registers that are preserved by the interrupt handler. Do not use registers other than the ones that Sony used.
# //0x00000d10
# static int HostIFActivityPollCallback(void){
#	int result;
#
#	if((*(volatile unsigned int *)0x040C)&0x200){
#		(*(volatile unsigned int *)0x040C)&=0xFFFFFDFF;
#		(*(volatile unsigned int *)0x0408)&=0xFFFFFDFF;
#
#		result=1;
#	}
#	else result=0;
#
#	return result;
#}

.globl HostIFActivityPollCallback
.ent HostIFActivityPollCallback
HostIFActivityPollCallback:
	lw	$v0, 0x40C($zero)
	addiu	$v1, $zero, 0x200
	and	$v0, $v0, $v1
	beq	$v0, $zero, 1f
	nop
	lw	$v0, 0x40C($zero)
	addiu	$v1, $zero, 0xFDFF
	and	$v0, $v0, $v1
	sw	$v0, 0x40C($zero)
	lw	$v0, 0x408($zero)
	addiu	$v1, $zero, 0xFDFF
	and	$v0, $v0, $v1
	sw	$v0, 0x408($zero)
	addiu	$v0, $zero, 1
1:
	jr	$ra
	nop
.end HostIFActivityPollCallback

.set reorder
.set macro
