    .text
    .global _start
    .global syscall
    .extern main
    .extern sysexit

_start:
    bl main
    mov x0, #0
    bl sysexit

syscall:
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
    .global SYS_EXIT
    .global SYS_WRITE
SYS_EXIT:   .quad 93
SYS_WRITE:  .quad 64
