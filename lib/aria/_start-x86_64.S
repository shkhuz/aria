    .text
    .globl _start
_start:
    callq main
    movl %eax, %edi
    movl $60, %eax
    syscall

    .globl _syscall
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
