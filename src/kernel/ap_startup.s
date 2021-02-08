%include "tasking.inc"

section .ap_startup

BASE equ 0x8000 ; Where the kernel will map this at runtime

CR0_PE  equ (1 << 0)
CR0_MP  equ (1 << 1)
CR0_ET  equ (1 << 4)
CR0_NE  equ (1 << 5)
CR0_WP  equ (1 << 16)
CR0_PG  equ (1 << 31)
CR0_VAL equ CR0_PE|CR0_MP|CR0_ET|CR0_NE|CR0_WP|CR0_PG

CR4_DE         equ (1 << 3)
CR4_PAE        equ (1 << 5)
CR4_MCE        equ (1 << 6)
CR4_PGE        equ (1 << 7)
CR4_OSFXSR     equ (1 << 9)
CR4_OSCMMEXCPT equ (1 << 10)
CR4_FSGSBASE   equ (1 << 16)
CR4_PCIDE      equ (1 << 17)
CR4_VAL equ CR4_DE|CR4_PAE|CR4_MCE|CR4_PGE|CR4_OSFXSR|CR4_OSCMMEXCPT|CR4_FSGSBASE|CR4_PCIDE

EFER_MSR  equ 0xC0000080
EFER_SCE  equ (1 << 0)
EFER_LME  equ (1 << 8)
EFER_NXE  equ (1 << 11)
EFER_VAL equ EFER_SCE|EFER_LME|EFER_NXE

bits 16
default rel
align 8

global ap_startup
ap_startup:
	jmp .start_real

align 8
	.pml4:  dq 0
	.stack: dq 0
	.ret:   dq 0

align 16
.gdt:
	dq 0x0        ; Null GDT entry

	dq 0x00209A0000000000 ; Code
	dq 0x0000920000000000 ; Data

align 4
.gdtd:
	dw ($ - .gdt)
	dd BASE + (.gdt - ap_startup)

align 4
.idtd:
	dw 0 ; zero-length IDT descriptor
	dd 0

.start_real:
	cli
	cld

	xor ax, ax
	mov ds, ax

	; set the temporary null IDT
	lidt [BASE + (.idtd - ap_startup)]

	; Enter long mode
	mov eax, CR4_VAL
	mov cr4, eax

	mov eax, [BASE + (.pml4 - ap_startup)]
	mov cr3, eax

	mov ecx, EFER_MSR
	rdmsr
	or eax, EFER_VAL
	wrmsr

	mov eax, CR0_VAL
	mov cr0, eax

	; Set the temporary minimal GDT
	lgdt [BASE + (.gdtd - ap_startup)]

	jmp (1 << 3):(BASE + (.start_long - ap_startup))

bits 64
default abs
align 8
.start_long:
	; set data segments
	mov ax, (2 << 3)
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	mov rax, [BASE + (.stack - ap_startup)]
	mov rsp, rax

	mov rax, [BASE + (.ret - ap_startup)]
	jmp rax


global ap_startup_code_size
ap_startup_code_size:
	dq ($ - ap_startup)


section .text
global init_ap_trampoline
init_ap_trampoline:
	push rbp
	mov rbp, rsp

	; rdi is the kernel pml4
	mov [BASE + (ap_startup.pml4 - ap_startup)], rdi

	; rsi is the stack for this AP
	mov [BASE + (ap_startup.stack - ap_startup)], rsi

	; rdx is the address to jump to
	mov [BASE + (ap_startup.ret - ap_startup)], rdx

	; rcx is the processor id
	mov rdi, rdx

	pop rbp
	ret
