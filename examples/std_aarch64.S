.global _start
.extern main

_start:
    bl main

    /* exit syscall */
    mov x8, #93
    svc #0

