    section .text
    global _start
    global write_str
    global write_char
    global exit
    extern main

_start:
    xor rax, rax
    call main
    mov rdi, rax
    call exit

; writes a string to stdout
; rdi = pointer to string
; rsi = bytes
write_str:
    push rbp
    mov rbp, rsp
    push rdx
    mov rax, 1    ; sys_write
    mov rdx, rsi  ; rdx = bytes
    mov rsi, rdi  ; rsi = pointer
    mov rdi, 1    ; rdi = file
    syscall
    pop rdx
    pop rbp
    ret

; writes a character to stdout
; rdi = char
write_char:
    push rbp
    mov rbp, rsp
    push rdi
    lea rax, [rsp]
    mov rdi, rax
    mov rsi, 1
    call write_str
    pop rdi
    pop rbp
    ret

; exit(code: u8)
; note: calls sys_exit
; rdi = exit code
exit:
    mov rax, 60
    syscall

