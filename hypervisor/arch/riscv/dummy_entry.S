.section .text
.global _start

_start:
    csrci sstatus, 0x2
    li sp, 0x81010000
    call print_hello_world

wait_loop:
    wfi
    j wait_loop

.global sbi_call
sbi_call:
    mv t0, a0
    mv t1, a1

    mv a0, a2
    mv a1, a3
    mv a2, a4
    mv a3, a5
    mv a4, a6
    mv a5, a7

    mv a6, t1
    mv a7, t0

    ecall

    ret

