    .text
    .extern exit
    .global _start
_start:
    bl main

    bl exit

    .global _syscall
_syscall:
    mov x8, x0
    mov x0, x1
    mov x1, x2
    mov x2, x3
    mov x3, x4
    mov x4, x5
    mov x5, x6
    svc #0
    ret

    .data
    .global SYS_READ
SYS_READ:   .short 63
    .global SYS_WRITE
SYS_WRITE:   .short 64
    .global SYS_EXIT
SYS_EXIT:   .short 93
