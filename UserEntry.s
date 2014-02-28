	.file "UserEntry.s"
	
.section .init
	call UserEntry
	
.section .text
	.align 2
.globl UserEntry
	.type	UserEntry,@function
	
UserEntry:
	subl $32,%esp
	pushl 32(%esp)
	add $40,%esp

	pushf
	pusha

	subl 	$4,%esp
	call 	init_translator
	addl	$4,%esp
	
	movl 	(%eax),%eax
	jmpl	*%eax

.Lfe1:
	.size	UserEntry,.Lfe1-UserEntry
