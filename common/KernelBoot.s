.set noreorder
.set nomacro
.set noat

.globl BootKernel
.ent BootKernel
BootKernel:
	por $at, $zero, $zero
	por $v0, $zero, $zero
	por $v1, $zero, $zero
	por $a0, $zero, $zero
	por $a1, $zero, $zero
	por $a2, $zero, $zero
	por $a3, $zero, $zero
	por $t0, $zero, $zero
	por $t1, $zero, $zero
	por $t2, $zero, $zero
	por $t3, $zero, $zero
	por $t4, $zero, $zero
	por $t5, $zero, $zero
	por $t6, $zero, $zero
	por $t7, $zero, $zero
	por $s0, $zero, $zero
	por $s1, $zero, $zero
	por $s2, $zero, $zero
	por $s3, $zero, $zero
	por $s4, $zero, $zero
	por $s5, $zero, $zero
	por $s6, $zero, $zero
	por $s7, $zero, $zero
	por $t8, $zero, $zero
	por $t9, $zero, $zero
	por $k0, $zero, $zero
	por $k1, $zero, $zero
	por $gp, $zero, $zero
	por $sp, $zero, $zero
	por $fp, $zero, $zero
	por $ra, $zero, $zero

	lui	$v0, 0x8000
	ori	$v0, 0x1000
	jr $v0
	nop
.end BootKernel

.set at
.set reorder
.set macro
