    .text
    .global _start
    .global syscall
    .extern main
    .extern exit

_start:
    xor %rax, %rax
    call main
    mov %rax, %rdi
    call exit

syscall:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    mov %r8, %r10
    mov %r9, %r8
    mov 8(%rsp), %r9
    syscall 
    ret

    .data
    .global SYS_EXIT
    .global SYS_WRITE
SYS_EXIT:   .quad 60
SYS_WRITE:  .quad 1
