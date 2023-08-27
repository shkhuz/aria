    .text
    .global _start
_start:
    callq ariamain
    movl %eax, %edi
    callq exit

    .global _syscall
_syscall:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    movq %rcx, %rdx
    movq %r8, %r10
    movq %r9, %r8
    movq 8(%rsp), %r9
    syscall
    ret

    .data
    .global SYS_READ
SYS_READ:   .short 0
    .global SYS_WRITE
SYS_WRITE:   .short 1
    .global SYS_EXIT
SYS_EXIT:   .short 60
