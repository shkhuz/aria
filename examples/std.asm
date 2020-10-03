    section .text
    global _start
    global exit
    extern main

_start:
    xor rax, rax
    call main
    mov rdi, rax
    call exit

; exit(code: u8)
; note: calls sys_exit
; rdi = exit code
exit:
    mov rax, 60
    syscall

