.arch armv8-a
.text
Scope__main:
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	sub	sp, sp, #48
	str	w0, [x29, #-8]
	mov	w0, #0
	str	w0, [x29, #-16]
	mov	w0, #0
	str	w0, [x29, #-24]
.L1:
	ldr	w0, [x29, #-24]
	str	w0, [sp, #-16]!
	mov	w0, #5
	ldr	w9, [sp], #16
	cmp	w9, w0
	cset	w0, lt
	cbz	w0, .L2
	ldr	w0, [x29, #-16]
	str	w0, [sp, #-16]!
	ldr	w0, [x29, #-24]
	ldr	w9, [sp], #16
	add	w0, w9, w0
	str	w0, [x29, #-16]
.L3:
	ldr	w0, [x29, #-24]
	str	w0, [sp, #-16]!
	mov	w0, #1
	ldr	w9, [sp], #16
	add	w0, w9, w0
	str	w0, [x29, #-24]
	b	.L1
.L2:
	mov	w0, #0
	str	w0, [x29, #-24]
.L4:
	ldr	w0, [x29, #-24]
	str	w0, [sp, #-16]!
	mov	w0, #3
	ldr	w9, [sp], #16
	cmp	w9, w0
	cset	w0, lt
	cbz	w0, .L5
	ldr	w0, [x29, #-16]
	str	w0, [sp, #-16]!
	mov	w0, #10
	ldr	w9, [sp], #16
	add	w0, w9, w0
	str	w0, [x29, #-16]
.L6:
	ldr	w0, [x29, #-24]
	str	w0, [sp, #-16]!
	mov	w0, #1
	ldr	w9, [sp], #16
	add	w0, w9, w0
	str	w0, [x29, #-24]
	b	.L4
.L5:
	mov	w0, #99
	str	w0, [x29, #-24]
	ldr	w0, [x29, #-16]
	str	w0, [sp, #-16]!
	ldr	w0, [x29, #-24]
	ldr	w9, [sp], #16
	add	w0, w9, w0
	str	w0, [x29, #-16]
	mov	w0, #0
	str	w0, [x29, #-24]
	ldr	w0, [x29, #-16]
	str	w0, [sp, #-16]!
	mov	w0, #0
	ldr	w9, [sp], #16
	cmp	w9, w0
	cset	w0, gt
	cbz	w0, .L7
	mov	w0, #100
	str	w0, [x29, #-32]
	mov	w0, #55
	str	w0, [x29, #-24]
	b	.L8
.L7:
	mov	w0, #0
	str	w0, [x29, #-32]
	mov	w0, #0
	str	w0, [x29, #-24]
.L8:
	ldr	w0, [x29, #-24]
	str	w0, [sp, #-16]!
	mov	w0, #1
	ldr	w9, [sp], #16
	add	w0, w9, w0
	str	w0, [x29, #-24]
	mov	w0, #0
	str	w0, [x29, #-32]
	mov	w0, #1
	str	w0, [x29, #-40]
	mov	w0, #2
	str	w0, [x29, #-48]
	ldr	w0, [x29, #-16]
	sxtw	x1, w0
	adrp	x0, :got:.Lfmt_in
	ldr	x0, [x0, :got_lo12:.Lfmt_in]
	bl	printf
	ldr	w0, [x29, #-24]
	sxtw	x1, w0
	adrp	x0, :got:.Lfmt_in
	ldr	x0, [x0, :got_lo12:.Lfmt_in]
	bl	printf
	ldr	w0, [x29, #-32]
	str	w0, [sp, #-16]!
	ldr	w0, [x29, #-40]
	ldr	w9, [sp], #16
	add	w0, w9, w0
	str	w0, [sp, #-16]!
	ldr	w0, [x29, #-48]
	ldr	w9, [sp], #16
	add	w0, w9, w0
	sxtw	x1, w0
	adrp	x0, :got:.Lfmt_in
	ldr	x0, [x0, :got_lo12:.Lfmt_in]
	bl	printf
	mov	w0, wzr
.Lret0:
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
	.globl main
main:
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	movz	x0, #0x80, lsl #16
	bl	malloc
	adrp	x1, kop_arena_base
	str	x0, [x1, :lo12:kop_arena_base]
	mov	w0, wzr
	bl	Scope__main
	mov	w0, wzr
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
.Klam:
	adrp	x0, :got:.Lmsg_lam
	ldr	x0, [x0, :got_lo12:.Lmsg_lam]
	bl	printf
	mov	w0, #1
	adrp	x9, :got:exit
	ldr	x9, [x9, :got_lo12:exit]
	blr	x9
.Karr:
	adrp	x0, :got:.Lmsg_arr
	ldr	x0, [x0, :got_lo12:.Lmsg_arr]
	bl	printf
	mov	w0, #1
	adrp	x9, :got:exit
	ldr	x9, [x9, :got_lo12:exit]
	blr	x9
.Koom:
	adrp	x0, :got:.Lmsg_oom
	ldr	x0, [x0, :got_lo12:.Lmsg_oom]
	bl	printf
	mov	w0, #1
	adrp	x9, :got:exit
	ldr	x9, [x9, :got_lo12:exit]
	blr	x9
.text
	.globl kopalloc
kopalloc:
	adrp	x1, kop_arena_used
	add	x1, x1, :lo12:kop_arena_used
	ldr	x2, [x1]
	add	x2, x2, x0
	movz	x3, #0x80, lsl #16
	cmp	x2, x3
	b.ge	.Koom
	str	x2, [x1]
	sub	x2, x2, x0
	adrp	x3, kop_arena_base
	add	x3, x3, :lo12:kop_arena_base
	ldr	x3, [x3]
	add	x0, x3, x2
	ret
	.section .data
	.align 3
kop_arena_base: .quad 0
kop_arena_used: .quad 0
	.section .rodata
.Lfmt_i: .asciz "%d"
.Lfmt_in: .asciz "%d\n"
.Lfmt_s: .asciz "%s"
.Lmsg_lam: .asciz "kopi: jumlah argumen lambda tidak cocok\n"
.Lmsg_arr: .asciz "kopi: array tidak valid (indeks/ukuran salah)\n"
.Lmsg_oom: .asciz "kopi: arena memori habis\n"
.Lfmt_sn: .asciz "%s\n"
