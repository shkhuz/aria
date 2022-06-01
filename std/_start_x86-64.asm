    section .text
    global _start
    global syscall
    extern main
    extern exit

_start:
    xor rax, rax
    call main
    mov rdi, rax
    call exit

syscall:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    mov r8, r9
    mov r9, [rsp+8]
    syscall
    ret

