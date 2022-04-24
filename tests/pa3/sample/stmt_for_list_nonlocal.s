	.text
	.attribute	4, 16
	.attribute	5, "rv32i2p0_m2p0_a2p0_c2p0"
	.file	"stmt_for_list_nonlocal.py"
	.globl	before_main
	.p2align	1
	.type	before_main,@function
before_main:
	.cfi_startproc
	addi	sp, sp, -16
	.cfi_def_cfa_offset 16
	sw	ra, 12(sp)
	.cfi_offset ra, -4
	#APP
	lui	a0, 8192
	mv	s11, a0
	#NO_APP
	call	heap.init@plt
	#APP
	mv	s10, gp
	add	s11, s11, s10
	li	s0, 0
	lw	ra, 12(sp)
	addi	sp, sp, 16
	ret

	#NO_APP
.Lfunc_end0:
	.size	before_main, .Lfunc_end0-before_main
	.cfi_endproc

	.globl	main
	.p2align	1
	.type	main,@function
main:
	.cfi_startproc
	addi	sp, sp, -144
	.cfi_def_cfa_offset 144
	sw	ra, 140(sp)
	sw	s0, 136(sp)
	sw	s1, 132(sp)
	sw	s2, 128(sp)
	.cfi_offset ra, -4
	.cfi_offset s0, -8
	.cfi_offset s1, -12
	.cfi_offset s2, -16
	#APP
	mv	s0, sp
	#NO_APP
	li	a1, 1
	sw	a1, 124(sp)
	lb	a2, 128(sp)
	lw	a3, 132(sp)
	lw	a4, 136(sp)
	li	a5, 2
	sw	a5, 40(sp)
	sw	a5, 120(sp)
	lb	a6, 124(sp)
	lw	a7, 128(sp)
	lw	a0, 132(sp)
	mv	t0, sp
	sw	a0, 0(t0)
	mv	a0, a5
	call	conslist@plt
	lw	a1, 40(sp)
	lw	a2, 0(a0)
	sw	a2, 64(sp)
	lb	a2, 4(a0)
	sw	a2, 68(sp)
	lw	a2, 8(a0)
	sw	a2, 72(sp)
	lw	a0, 12(a0)
	sw	a0, 76(sp)
	sw	a1, 116(sp)
	lb	a2, 120(sp)
	lw	a3, 124(sp)
	lw	a4, 128(sp)
	li	a5, 3
	sw	a5, 112(sp)
	lb	a6, 116(sp)
	lw	a7, 120(sp)
	lw	a0, 124(sp)
	mv	t0, sp
	sw	a0, 0(t0)
	mv	a0, a1
	call	conslist@plt
	mv	a1, a0
	lw	a0, 40(sp)
	lw	a2, 0(a1)
	sw	a2, 80(sp)
	lb	a2, 4(a1)
	sw	a2, 84(sp)
	lw	a2, 8(a1)
	sw	a2, 88(sp)
	lw	a1, 12(a1)
	sw	a1, 60(sp)
	li	a1, 4
	sw	a1, 92(sp)
	sw	a1, 108(sp)
	lb	a2, 112(sp)
	lw	a3, 116(sp)
	lw	a4, 120(sp)
	li	a5, 5
	sw	a5, 104(sp)
	lb	a6, 108(sp)
	lw	a7, 112(sp)
	lw	t0, 116(sp)
	mv	t1, sp
	sw	t0, 0(t1)
	call	conslist@plt
	mv	a1, a0
	lw	a0, 40(sp)
	lw	a2, 0(a1)
	sw	a2, 56(sp)
	lb	a2, 4(a1)
	sw	a2, 52(sp)
	lw	a2, 8(a1)
	sw	a2, 48(sp)
	lw	a1, 12(a1)
	sw	a1, 44(sp)
	li	a1, 6
	sw	a1, 100(sp)
	lb	a2, 104(sp)
	lw	a3, 108(sp)
	lw	a4, 112(sp)
	li	a5, 7
	sw	a5, 96(sp)
	lb	a6, 100(sp)
	lw	a7, 104(sp)
	lw	t0, 108(sp)
	mv	t1, sp
	sw	t0, 0(t1)
	call	conslist@plt
	lw	t5, 44(sp)
	lw	t4, 48(sp)
	lw	t3, 52(sp)
	lw	t2, 56(sp)
	lw	t0, 60(sp)
	lw	a1, 64(sp)
	lw	a2, 68(sp)
	lw	a3, 72(sp)
	lw	a4, 76(sp)
	lw	a5, 80(sp)
	lw	a6, 84(sp)
	lw	a7, 88(sp)
	mv	t1, a0
	lw	a0, 92(sp)
	lw	t6, 0(t1)
	lb	s0, 4(t1)
	lw	s1, 8(t1)
	lw	s2, 12(t1)
	mv	t1, sp
	sw	s2, 32(t1)
	sw	s1, 28(t1)
	sw	s0, 24(t1)
	sw	t6, 20(t1)
	sw	t5, 16(t1)
	sw	t4, 12(t1)
	sw	t3, 8(t1)
	sw	t2, 4(t1)
	sw	t0, 0(t1)
	call	conslist@plt
	call	($crunch)@plt
	lui	a0, %hi(x)
	lw	a0, %lo(x)(a0)
	call	makeint@plt
	call	print@plt
	#APP
	li	a7, 93	#exit system call
	ecall	

	#NO_APP
	lw	ra, 140(sp)
	lw	s0, 136(sp)
	lw	s1, 132(sp)
	lw	s2, 128(sp)
	addi	sp, sp, 144
	ret
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
	.cfi_endproc

	.globl	$crunch
	.p2align	1
	.type	$crunch,@function
$crunch:
	.cfi_startproc
	addi	sp, sp, -32
	.cfi_def_cfa_offset 32
	sw	ra, 28(sp)
	.cfi_offset ra, -4
	addi	a1, sp, 24
	sw	a1, 16(sp)
	sw	a0, 20(sp)
	addi	a0, sp, 16
	call	($crunch.make_z)@plt
	lui	a0, %hi(x)
	lw	a0, %lo(x)(a0)
	sw	a0, 8(sp)
	sw	a0, 12(sp)
	j	.LBB2_1
.LBB2_1:
	lw	a1, 8(sp)
	lw	a0, 12(sp)
	xor	a1, a1, a0
	snez	a1, a1
	sw	a1, 0(sp)
	addi	a0, a0, 1
	sw	a0, 4(sp)
	j	.LBB2_2
.LBB2_2:
	lw	a0, 0(sp)
	lw	a2, 4(sp)
	li	a1, 0
	sw	a2, 12(sp)
	bne	a0, a1, .LBB2_1
	j	.LBB2_3
.LBB2_3:
	lw	ra, 28(sp)
	addi	sp, sp, 32
	ret
.Lfunc_end2:
	.size	$crunch, .Lfunc_end2-($crunch)
	.cfi_endproc

	.globl	$crunch.make_z
	.p2align	1
	.type	$crunch.make_z,@function
$crunch.make_z:
	.cfi_startproc
	addi	sp, sp, -32
	.cfi_def_cfa_offset 32
	sw	ra, 28(sp)
	.cfi_offset ra, -4
	lw	a0, 4(a0)
	li	a1, 0
	sw	a1, 24(sp)
	call	($len)@plt
	sw	a0, 16(sp)
	sw	a0, 20(sp)
	j	.LBB3_1
.LBB3_1:
	lw	a1, 16(sp)
	lw	a0, 20(sp)
	xor	a1, a1, a0
	snez	a1, a1
	sw	a1, 8(sp)
	addi	a0, a0, 1
	sw	a0, 12(sp)
	j	.LBB3_2
.LBB3_2:
	lw	a0, 8(sp)
	lw	a2, 12(sp)
	li	a1, 0
	sw	a2, 20(sp)
	bne	a0, a1, .LBB3_1
	j	.LBB3_3
.LBB3_3:
	lw	ra, 28(sp)
	addi	sp, sp, 32
	ret
.Lfunc_end3:
	.size	$crunch.make_z, .Lfunc_end3-($crunch.make_z)
	.cfi_endproc

	.type	$object$prototype,@object
	.data
	.globl	$object$prototype
	.p2align	3
$object$prototype:
	.word	0
	.word	3
	.word	($object$dispatchTable)
	.size	$object$prototype, 12

	.type	$object$dispatchTable,@object
	.section	.sdata,"aw",@progbits
	.globl	$object$dispatchTable
	.p2align	3
$object$dispatchTable:
	.word	($object.__init__)
	.size	$object$dispatchTable, 4

	.type	$int$prototype,@object
	.data
	.globl	$int$prototype
	.p2align	3
$int$prototype:
	.word	1
	.word	4
	.word	($int$dispatchTable)
	.word	0
	.size	$int$prototype, 16

	.type	$int$dispatchTable,@object
	.section	.sdata,"aw",@progbits
	.globl	$int$dispatchTable
	.p2align	3
$int$dispatchTable:
	.word	($object.__init__)
	.size	$int$dispatchTable, 4

	.type	$bool$prototype,@object
	.data
	.globl	$bool$prototype
	.p2align	3
$bool$prototype:
	.word	2
	.word	4
	.word	($bool$dispatchTable)
	.byte	0
	.zero	3
	.size	$bool$prototype, 16

	.type	$bool$dispatchTable,@object
	.section	.sdata,"aw",@progbits
	.globl	$bool$dispatchTable
	.p2align	3
$bool$dispatchTable:
	.word	($object.__init__)
	.size	$bool$dispatchTable, 4

	.type	$str$prototype,@object
	.data
	.globl	$str$prototype
	.p2align	4
$str$prototype:
	.word	3
	.word	5
	.word	($str$dispatchTable)
	.word	0
	.word	0
	.size	$str$prototype, 20

	.type	$str$dispatchTable,@object
	.section	.sdata,"aw",@progbits
	.globl	$str$dispatchTable
	.p2align	3
$str$dispatchTable:
	.word	($object.__init__)
	.size	$str$dispatchTable, 4

	.type	$.list$prototype,@object
	.data
	.globl	$.list$prototype
	.p2align	4
$.list$prototype:
	.word	4294967295
	.word	5
	.zero	16
	.word	0
	.word	0
	.size	$.list$prototype, 32

	.type	x,@object
	.section	.sbss,"aw",@nobits
	.globl	x
	.p2align	2
x:
	.word	0
	.size	x, 4

	.section	.init_array,"aw",@init_array
	.p2align	2
	.word	before_main
	.section	".note.GNU-stack","",@progbits















