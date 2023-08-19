    .globl _start
_start:
    callq main
    movl %eax, %edi
    movl $60, %eax
    syscall

