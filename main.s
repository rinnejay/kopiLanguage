.arch armv8-a
.text
Main__main:
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	sub	sp, sp, #16
	str	w0, [x29, #-8]
	adrp	x0, :got:.Lfmt_sn
	ldr	x0, [x0, :got_lo12:.Lfmt_sn]
	adrp	x1, :got:cpool0
	ldr	x1, [x1, :got_lo12:cpool0]
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
	movz	x0, #0x200, lsl #16
	bl	malloc
	adrp	x1, kop_arena_base
	str	x0, [x1, :lo12:kop_arena_base]
	mov	w0, wzr
	bl	Main__main
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
	movz	x3, #0x200, lsl #16
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
cpool0:
	.asciz "Helo"
